/** ***************************************************************************
 * @file
 * @brief Measuring voltages with the ADC(s) in different configurations
 *
 *
 * Demonstrates different ADC (Analog to Digital Converter) modes
 * ==============================================================
 *
 * - ADC in single conversion mode
 * - ADC triggered by a timer and with interrupt after end of conversion
 * - ADC combined with DMA (Direct Memory Access) to fill a buffer
 * - Dual mode = simultaneous sampling of two inputs by two ADCs
 * - Scan mode = sequential sampling of two inputs by one ADC
 * - Simple DAC output is demonstrated as well
 * - Analog mode configuration for GPIOs
 * - Display recorded data on the graphics display
 *
 * Peripherals @ref HowTo
 *
 * @image html demo_screenshot_board.jpg
 *
 *
 * HW used for the demonstrations
 * ==============================
 * A simple HW was used for testing the code.
 * It is connected to the pins marked in red in the above image.
 *
 * @image html demo_board_schematic.png
 *
 * Of course the code runs also without this HW.
 * Simply connect a signal or waveform generator to the input(s).
 *
 *
 * @anchor HowTo
 * How to Configure the Peripherals: ADC, TIMER and DMA
 * ====================================================
 *
 * All the peripherals are accessed by writing to or reading from registers.
 * From the programmer’s point of view this is done exactly as
 * writing or reading the value of a variable.
 * @n Writing to a register configures the HW of the associated peripheral
 * to do what is required.
 * @n Reading from a registers gets status and data from the HW peripheral.
 *
 * The information on which bits have to be set to get a specific behavior
 * is documented in the <b>reference manual</b> of the mikrocontroller.
 *
 *
 * ----------------------------------------------------------------------------
 * @author Hanspeter Hochreutener, hhrt@zhaw.ch
 * @date 17.06.2021
 *****************************************************************************/


/******************************************************************************
 * Includes
 *****************************************************************************/
#include <stdio.h>
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ts.h"

#include "measuring.h"

/******************************************************************************
 * Defines
 *****************************************************************************/
#define ADC_DAC_RES		12			///< Resolution
#define ADC_MAX_VALUE   4095		///< Maximum value of ADC output
#define ADC_NUMS		60			///< Number of samples
#define ADC_FS			600	///< Sampling freq. => 12 samples for a 50Hz period
#define ADC_CLOCK		84000000	///< APB2 peripheral clock frequency
#define ADC_CLOCKS_PS	15			///< Clocks/sample: 3 hold + 12 conversion
#define TIM_CLOCK		84000000	///< APB1 timer clock frequency
#define TIM_TOP			9			///< Timer top value
#define TIM_PRESCALE	(TIM_CLOCK/ADC_FS/(TIM_TOP+1)-1) ///< Clock prescaler
#define MEAS_INPUT_COUNT 2			///< Input count per measurement

/******************************************************************************
 * Variables
 *****************************************************************************/
bool MEAS_data_ready = false;			///< New data is ready
uint32_t MEAS_amplitude_left = 0;		///< Amplitude of the left channel
uint32_t MEAS_amplitude_right = 0;		///< Amplitude of the right channel

static uint32_t ADC_sample_count = 0;	///< Index for buffer
static uint32_t ADC_samples[2*ADC_NUMS];///< ADC values of max. 2 input channels


/******************************************************************************
 * Functions
 *****************************************************************************/


/** ***************************************************************************
 * @brief Configure GPIOs in analog mode.
 *
 * @note The input number for the ADCs is not equal to the GPIO pin number!
 * - ADC3_IN4 = GPIO PF6
 * - ADC123_IN13 = GPIO PC3
 * - ADC12_IN5 = GPIO PA5
 * - DAC_OUT2 = GPIO PA5 (= same GPIO as ADC12_IN5)
 *****************************************************************************/
void MEAS_GPIO_analog_init(void)
{
	__HAL_RCC_GPIOF_CLK_ENABLE();		// Enable Clock for GPIO port F
	GPIOF->MODER |= (GPIO_MODER_MODER6_Msk);// Analog mode for PF6 = ADC3_IN4
	GPIOF->MODER |= (GPIO_MODER_MODER8_Msk);// Analog mode for PF8 = ADC3_IN6
	__HAL_RCC_GPIOC_CLK_ENABLE();		// Enable Clock for GPIO port C
	GPIOC->MODER |= (GPIO_MODER_MODER1_Msk);// Analog mode for PC1 = ADC123_IN11
	GPIOC->MODER |= (GPIO_MODER_MODER3_Msk);// Analog mode for PC3 = ADC123_IN13
}


/** ***************************************************************************
 * @brief Resets the ADCs and the timer
 *
 * to make sure the different demos do not interfere.
 *****************************************************************************/
void ADC_reset(void) {
	RCC->APB2RSTR |= RCC_APB2RSTR_ADCRST;	// Reset ADCs
	RCC->APB2RSTR &= ~RCC_APB2RSTR_ADCRST;	// Release reset of ADCs
	TIM2->CR1 &= ~TIM_CR1_CEN;				// Disable timer
}



/** ***************************************************************************
 * @brief Configure the timer to trigger the ADC(s)
 *
 * @note For debugging purposes the timer interrupt might be useful.
 *****************************************************************************/
void MEAS_timer_init(void)
{
	__HAL_RCC_TIM2_CLK_ENABLE();		// Enable Clock for TIM2
	TIM2->PSC = TIM_PRESCALE;			// Prescaler for clock freq. = 1MHz
	TIM2->ARR = TIM_TOP;				// Auto reload = counter top value
	TIM2->CR2 |= TIM_CR2_MMS_1; 		// TRGO on update
	/* If timer interrupt is not needed, comment the following lines */
	TIM2->DIER |= TIM_DIER_UIE;			// Enable update interrupt
	NVIC_ClearPendingIRQ(TIM2_IRQn);	// Clear pending interrupt on line 0
	NVIC_EnableIRQ(TIM2_IRQn);			// Enable interrupt line 0 in the NVIC
}


/** ***************************************************************************
 * @brief Initialize ADC, timer and DMA for sequential acquisition = scan mode
 *
 * Uses ADC2 and DMA2_Stream1 channel2
 * @n The ADC2 trigger is set to TIM2 TRGO event
 * @n At each trigger both inputs are converted sequentially
 * and transfered to memory by the DMA.
 * @n As each conversion triggers the DMA, the number of transfers is doubled.
 * @n The DMA triggers the transfer complete interrupt when all data is ready.
 * @n The inputs used are ADC123_IN13 = GPIO PC3 and ADC3_IN4 = GPIO PF6
 *****************************************************************************/
void ADC3_IN13_IN4_scan_init(void)
{
	__HAL_RCC_ADC3_CLK_ENABLE();		// Enable Clock for ADC3
	ADC3->SQR1 |= ADC_SQR1_L_0;			// Convert 2 inputs
	ADC3->SQR3 |= (13UL << ADC_SQR3_SQ1_Pos);	// Input 13 = first conversion
	ADC3->SQR3 |= (4UL << ADC_SQR3_SQ2_Pos);	// Input 4 = second conversion
	ADC3->CR1 |= ADC_CR1_SCAN;			// Enable scan mode
	ADC3->CR2 |= (1UL << ADC_CR2_EXTEN_Pos);	// En. ext. trigger on rising e.
	ADC3->CR2 |= (6UL << ADC_CR2_EXTSEL_Pos);	// Timer 2 TRGO event
	ADC3->CR2 |= ADC_CR2_DMA;			// Enable DMA mode
	__HAL_RCC_DMA2_CLK_ENABLE();		// Enable Clock for DMA2
	DMA2_Stream1->CR &= ~DMA_SxCR_EN;	// Disable the DMA stream 1
	while (DMA2_Stream1->CR & DMA_SxCR_EN) { ; }	// Wait for DMA to finish
	DMA2->LIFCR |= DMA_LIFCR_CTCIF1;	// Clear transfer complete interrupt fl.
	DMA2_Stream1->CR |= (2UL << DMA_SxCR_CHSEL_Pos);	// Select channel 2
	DMA2_Stream1->CR |= DMA_SxCR_PL_1;		// Priority high
	DMA2_Stream1->CR |= DMA_SxCR_MSIZE_1;	// Memory data size = 32 bit
	DMA2_Stream1->CR |= DMA_SxCR_PSIZE_1;	// Peripheral data size = 32 bit
	DMA2_Stream1->CR |= DMA_SxCR_MINC;	// Increment memory address pointer
	DMA2_Stream1->CR |= DMA_SxCR_TCIE;	// Transfer complete interrupt enable
	DMA2_Stream1->NDTR = 2*ADC_NUMS;	// Number of data items to transfer
	DMA2_Stream1->PAR = (uint32_t)&ADC3->DR;	// Peripheral register address
	DMA2_Stream1->M0AR = (uint32_t)ADC_samples;	// Buffer memory loc. address

}


/** ***************************************************************************
 * @brief Initialize ADC, timer and DMA for sequential acquisition = scan mode
 *
 * Uses ADC2 and DMA2_Stream1 channel2
 * @n The ADC2 trigger is set to TIM2 TRGO event
 * @n At each trigger both inputs are converted sequentially
 * and transfered to memory by the DMA.
 * @n As each conversion triggers the DMA, the number of transfers is doubled.
 * @n The DMA triggers the transfer complete interrupt when all data is ready.
 * @n The inputs used are ADC123_IN13 = GPIO PC3 and ADC3_IN4 = GPIO PF6
 *****************************************************************************/
void ADC3_IN11_IN6_scan_init(void)
{
	__HAL_RCC_ADC3_CLK_ENABLE();		// Enable Clock for ADC3
	ADC3->SQR1 |= ADC_SQR1_L_0;			// Convert 2 inputs
	ADC3->SQR3 |= (11UL << ADC_SQR3_SQ1_Pos);	// Input 13 = first conversion
	ADC3->SQR3 |= (6UL << ADC_SQR3_SQ2_Pos);	// Input 4 = second conversion
	ADC3->CR1 |= ADC_CR1_SCAN;			// Enable scan mode
	ADC3->CR2 |= (1UL << ADC_CR2_EXTEN_Pos);	// En. ext. trigger on rising e.
	ADC3->CR2 |= (6UL << ADC_CR2_EXTSEL_Pos);	// Timer 2 TRGO event
	ADC3->CR2 |= ADC_CR2_DMA;			// Enable DMA mode
	__HAL_RCC_DMA2_CLK_ENABLE();		// Enable Clock for DMA2
	DMA2_Stream1->CR &= ~DMA_SxCR_EN;	// Disable the DMA stream 1
	while (DMA2_Stream1->CR & DMA_SxCR_EN) { ; }	// Wait for DMA to finish
	DMA2->LIFCR |= DMA_LIFCR_CTCIF1;	// Clear transfer complete interrupt fl.
	DMA2_Stream1->CR |= (2UL << DMA_SxCR_CHSEL_Pos);	// Select channel 2
	DMA2_Stream1->CR |= DMA_SxCR_PL_1;		// Priority high
	DMA2_Stream1->CR |= DMA_SxCR_MSIZE_1;	// Memory data size = 32 bit
	DMA2_Stream1->CR |= DMA_SxCR_PSIZE_1;	// Peripheral data size = 32 bit
	DMA2_Stream1->CR |= DMA_SxCR_MINC;	// Increment memory address pointer
	DMA2_Stream1->CR |= DMA_SxCR_TCIE;	// Transfer complete interrupt enable
	DMA2_Stream1->NDTR = 2*ADC_NUMS;	// Number of data items to transfer
	DMA2_Stream1->PAR = (uint32_t)&ADC3->DR;	// Peripheral register address
	DMA2_Stream1->M0AR = (uint32_t)ADC_samples;	// Buffer memory loc. address

}


/** ***************************************************************************
 * @brief Start DMA, ADC and timer
 *
 *****************************************************************************/
void ADC3_dual_scan_start(void)
{
	DMA2_Stream1->CR |= DMA_SxCR_EN;	// Enable DMA
	NVIC_ClearPendingIRQ(DMA2_Stream1_IRQn);	// Clear pending DMA interrupt
	NVIC_EnableIRQ(DMA2_Stream1_IRQn);	// Enable DMA interrupt in the NVIC
	ADC3->CR2 |= ADC_CR2_ADON;			// Enable ADC3
	TIM2->CR1 |= TIM_CR1_CEN;			// Enable timer
}


/** ***************************************************************************
 * @brief Interrupt handler for the timer 2
 *
 * @note This interrupt handler was only used for debugging purposes
 * and to increment the DAC value.
 *****************************************************************************/
void TIM2_IRQHandler(void)
{
	TIM2->SR &= ~TIM_SR_UIF;			// Clear pending interrupt flag
}


/** ***************************************************************************
 * @brief Interrupt handler for the ADCs
 *
 * Reads one sample from the ADC3 DataRegister and transfers it to a buffer.
 * @n Stops when ADC_NUMS samples have been read.
 *****************************************************************************/
void ADC_IRQHandler(void)
{
	if (ADC3->SR & ADC_SR_EOC) {		// Check if ADC3 end of conversion
		ADC_samples[ADC_sample_count++] = ADC3->DR;	// Read input channel 1 only
		if (ADC_sample_count >= ADC_NUMS) {		// Buffer full
			TIM2->CR1 &= ~TIM_CR1_CEN;	// Disable timer
			ADC3->CR2 &= ~ADC_CR2_ADON;	// Disable ADC3
			ADC_reset();
			MEAS_data_ready = true;
		}

	}
}


/** ***************************************************************************
 * @brief Interrupt handler for DMA2 Stream1
 *
 * The samples from the ADC3 have been transfered to memory by the DMA2 Stream1
 * and are ready for processing.
 *****************************************************************************/
void DMA2_Stream1_IRQHandler(void)
{
	if (DMA2->LISR & DMA_LISR_TCIF1) {	// Stream1 transfer compl. interrupt f.
		NVIC_DisableIRQ(DMA2_Stream1_IRQn);	// Disable DMA interrupt in the NVIC
		NVIC_ClearPendingIRQ(DMA2_Stream1_IRQn);// Clear pending DMA interrupt
		DMA2_Stream1->CR &= ~DMA_SxCR_EN;	// Disable the DMA
		while (DMA2_Stream1->CR & DMA_SxCR_EN) { ; }	// Wait for DMA to finish
		DMA2->LIFCR |= DMA_LIFCR_CTCIF1;// Clear transfer complete interrupt fl.
		TIM2->CR1 &= ~TIM_CR1_CEN;		// Disable timer
		ADC3->CR2 &= ~ADC_CR2_ADON;		// Disable ADC3
		ADC3->CR2 &= ~ADC_CR2_DMA;		// Disable DMA mode
		ADC_reset();
		MEAS_analyse_data();
		MEAS_data_ready = true;
	}
}


/** ***************************************************************************
 * @brief Draw buffer data as curves
 *
 * and write the first two samples as numbers.
 * @n After drawing, the buffer is cleared to get ready for the next run.
 * @note Drawing outside of the display crashes the system!
 * @todo Create new .h and .c files for calculating and displaying
 * of signals and results.
 * The code of this function was put into the same file for debugging purposes
 * and should be moved to a separate file in the final version
 * because displaying is not related to measuring.
 *****************************************************************************/
void MEAS_show_data(void)
{
	const uint32_t Y_OFFSET = 260;
	const uint32_t X_SIZE = 240;
	const uint32_t f = (1 << ADC_DAC_RES) / Y_OFFSET + 1;	// Scaling factor
	uint32_t data;
	uint32_t data_last;
	/* Clear the display */
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	BSP_LCD_FillRect(0, 0, X_SIZE, Y_OFFSET+1);
	/* Write first 2 samples as numbers */
	BSP_LCD_SetFont(&Font24);
	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	char text[16];
	snprintf(text, 15, "Amp left  %4d", (int)(MEAS_amplitude_left));
	BSP_LCD_DisplayStringAt(0, 50, (uint8_t *)text, LEFT_MODE);
	snprintf(text, 15, "Amp right %4d", (int)(MEAS_amplitude_right));
	BSP_LCD_DisplayStringAt(0, 80, (uint8_t *)text, LEFT_MODE);
	/* Draw the  values of input channel 1 as a curve */
	BSP_LCD_SetTextColor(LCD_COLOR_BLUE);
	data = ADC_samples[MEAS_INPUT_COUNT*0] / f;
	for (uint32_t i = 1; i < ADC_NUMS; i++){
		data_last = data;
		data = (ADC_samples[MEAS_INPUT_COUNT*i]) / f;
		if (data > Y_OFFSET) { data = Y_OFFSET; }// Limit value, prevent crash
		BSP_LCD_DrawLine(4*(i-1), Y_OFFSET-data_last, 4*i, Y_OFFSET-data);
	}
	/* Draw the  values of input channel 2 (if present) as a curve */
	if (MEAS_INPUT_COUNT == 2) {
		BSP_LCD_SetTextColor(LCD_COLOR_RED);
		data = ADC_samples[MEAS_INPUT_COUNT*0+1] / f;
		for (uint32_t i = 1; i < ADC_NUMS; i++){
			data_last = data;
			data = (ADC_samples[MEAS_INPUT_COUNT*i+1]) / f;
			if (data > Y_OFFSET) { data = Y_OFFSET; }// Limit value, prevent crash
			BSP_LCD_DrawLine(4*(i-1), Y_OFFSET-data_last, 4*i, Y_OFFSET-data);
		}
	}
	/* Clear buffer and flag */
	for (uint32_t i = 0; i < ADC_NUMS; i++){
		ADC_samples[2*i] = 0;
		ADC_samples[2*i+1] = 0;
	}
	ADC_sample_count = 0;
}

/** ***************************************************************************
 * @brief Analyse data to detect sine amplitude
 *****************************************************************************/
void MEAS_analyse_data(void)
{
	uint32_t buffer_left_channel[ADC_NUMS];
	uint32_t buffer_right_channel[ADC_NUMS];
	for (int i = 0; i < ADC_NUMS; ++i) {
		buffer_left_channel[i] = ADC_samples[2*i];
		buffer_right_channel[i] = ADC_samples[((2*i)+1)];
	}

	//sort arrays from low to high
	uint32_t temp_left;
	uint32_t temp_right;
	for (int i = 0; i < ADC_NUMS; ++i) {
		for (int j = i+1; j < ADC_NUMS; ++j) {
			if (buffer_left_channel[i]>buffer_left_channel[j]) {
				temp_left = buffer_left_channel[i];
				buffer_left_channel[i]=buffer_left_channel[j];
				buffer_left_channel[j]=temp_left;
			}
			if (buffer_right_channel[i]>buffer_right_channel[j]) {
				temp_right = buffer_right_channel[i];
				buffer_right_channel[i]=buffer_right_channel[j];
				buffer_right_channel[j]=temp_right;
			}
		}
	}


	uint32_t values_left[10];
	uint32_t values_right[10];
	//select 5 lowest values
	for (int i = 0; i < 5; ++i) {
		values_left[i]=buffer_left_channel[i];
		values_right[i]=buffer_right_channel[i];
	}

	//select 5 highest values
	for (int i = 0; i < 5; ++i) {
		values_left[i+5]=buffer_left_channel[i+55];
		values_right[i+5]=buffer_right_channel[i+55];
	}

	//check if values mean around middle of ADC range
	uint32_t check_sum_left = 0;
	uint32_t check_sum_right = 0;
	for (int i = 0; i < 10; ++i) {
		check_sum_left += values_left[i];
		check_sum_right += values_right[i];
	}
	check_sum_left = check_sum_left / 10;
	check_sum_right = check_sum_right / 10;

	//convert low values to high values
	for (int i = 0; i < 5; ++i) {
		values_left[i] = ADC_MAX_VALUE - values_left[i];
		values_right[i] = ADC_MAX_VALUE - values_right[i];
	}

	//convert into amplitudes
	//for (int i = 0; i < 10; ++i) {
	//	values_left[i] = values_left[i]-(ADC_MAX_VALUE/2);
	//	values_right[i] = values_right[i]-(ADC_MAX_VALUE/2);
	//}

	//calculate mean of all 10 values
	uint32_t sum_left = 0;
	uint32_t sum_right = 0;
	for (int i = 0; i < 10; ++i) {
		sum_left += values_left[i];
		sum_right += values_right[i];
	}
	sum_left = sum_left / 10;
	sum_right = sum_right / 10;

	MEAS_amplitude_left = sum_left-(ADC_MAX_VALUE/2);
	MEAS_amplitude_right = sum_right-(ADC_MAX_VALUE/2);
}








