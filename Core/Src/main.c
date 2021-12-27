/** ***************************************************************************
 * @file
 * @brief Sets up the microcontroller, the clock system and the peripherals.
 *
 * Initialization is done for the system, the blue user button, the user LEDs,
 * and the LCD display with the touchscreen.
 * @n Then the code enters an infinite while-loop, where the data transferring
 * is managed and the analytics and lcd_gui handler gets called regularly.
 *
 * @author  Jonas Bollhalder, bollhjon@students.zhaw.ch
 * @author  Tarik Durmaz, durmatar@students.zhaw.ch
 * @date	27.12.2021
 *****************************************************************************/


/******************************************************************************
 * Includes
 *****************************************************************************/
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ts.h"

#include "main.h"
#include "pushbutton.h"
#include "measuring.h"
#include "lcd_gui.h"
#include "analytics.h"


/******************************************************************************
 * Defines
 *****************************************************************************/


/******************************************************************************
 * Variables
 *****************************************************************************/


/******************************************************************************
 * Functions
 *****************************************************************************/
static void SystemClock_Config(void);	///< System Clock Configuration
static void gyro_disable(void);			///< Disable the onboard gyroscope


/** ***************************************************************************
 * @brief  Main function
 * @return not used because main ends in an infinite loop
 *
 * Initialization and infinite while loop
 *****************************************************************************/
int main(void) {
	HAL_Init();						// Initialize the system

	SystemClock_Config();			// Configure system clocks

	BSP_LCD_Init();					// Initialize the LCD display
	BSP_LCD_LayerDefaultInit(LCD_FOREGROUND_LAYER, LCD_FRAME_BUFFER);
	BSP_LCD_SelectLayer(LCD_FOREGROUND_LAYER);
	BSP_LCD_DisplayOn();
	BSP_LCD_Clear(LCD_COLOR_WHITE);

	BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());	// Touchscreen

	PB_init();						// Initialize the user pushbutton
	PB_enableIRQ();					// Enable interrupt on user pushbutton

	BSP_LED_Init(LED3);				// Toggles in while loop
	BSP_LED_Init(LED4);				// Is toggled by user button

	gyro_disable();					// Disable gyro, use those analog inputs

	MEAS_GPIO_analog_init();		// Configure GPIOs in analog mode
	MEAS_timer_init();				// Configure the timer

	/* Infinite while loop */
	while (1) {						// Infinitely loop in main function
		BSP_LED_Toggle(LED3);		// Visual feedback when running

		if (PB_pressed()) {			// Check if user pushbutton was pressed
			ANA_inBtn = true;		// Send to analytics handler
			GUI_inputBtn = true;	// Send to site handler
		}

		if (MEAS_data_ready) {		// Analyse data if new data available
			// Transfer data to analytics handler
			ANA_inAmpLeft = MEAS_amplitude_left;
			ANA_inAmpRight = MEAS_amplitude_right;
			ANA_inMeasReady = true;		// Send to analytics handler
			MEAS_data_ready = false;	// Reset meas data ready bit
		}

		if (ANA_outStartHALL) {		// Start hall measurement
			ADC3_IN11_IN6_scan_init();
			ADC3_dual_scan_start();
			ANA_outStartHALL = false; // Reset hall start event
		}

		if (ANA_outStartWPC) {		// Start wpc measurement
			ADC3_IN13_IN4_scan_init();
			ADC3_dual_scan_start();
			ANA_outStartWPC = false; // Reset wpc start event
		}

		if (ANA_outDataReady) {		// Analytics data ready
			// Transfer Data
			if (ANA_inOptn[1]==0) {
				// Analysed
				if (ANA_outResults[1]<300) {
					// Data usable
					GUI_angle = ANA_outResults[0];
					GUI_distance = ANA_outResults[1];
					GUI_distanceDeviation = ANA_outResults[2];
					GUI_current = ANA_outResults[3];
					GUI_cable_detected = true;
				} else {
					// Data unusable
					GUI_angle = 100;
					GUI_distance = -1;
					GUI_distanceDeviation = -1;
					GUI_current = -1;
					GUI_cable_not_detected = true;
				}


			} else {
				// Raw
				GUI_rawHallRight = ANA_outResults[0];
				GUI_rawHallLeft = ANA_outResults[1];
				GUI_rawWpcRight = ANA_outResults[2];
				GUI_rawWpcLeft = ANA_outResults[3];
			}
			GUI_inputMeasReady = true;
			ANA_outDataReady = false;
		}

		// Show measurement state on led
		if (ANA_measBusy) {
			BSP_LED_On(LED4);
		} else {
			BSP_LED_Off(LED4);
		}

		if (GUI_outOptn) {						// Check if Options were changed
			ANA_inOptn[0]=GUI_mode;				// Transfer mode
			ANA_inOptn[1]=GUI_options[0].active;// Transfer data type
			ANA_inOptn[2]=GUI_options[1].active;// Transfer measuring type
			switch (GUI_options[2].active) {	// Transfer accuracy
				case 0:
					ANA_inOptn[3]=1;
					break;
				case 1:
					ANA_inOptn[3]=5;
					break;
				case 2:
					ANA_inOptn[3]=10;
					break;
				default:
					break;
			}
			GUI_outOptn = false;				// Reset option bit
		}

		//Analytics handler
		ANA_Handler();

		//Site handler
		GUI_SiteHandler();
	}
}


/** ***************************************************************************
 * @brief System Clock Configuration
 *
 *****************************************************************************/
static void SystemClock_Config(void){
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
	/* Configure the main internal regulator output voltage */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
	/* Initialize High Speed External Oscillator and PLL circuits */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	HAL_RCC_OscConfig(&RCC_OscInitStruct);
	/* Initialize gates and clock dividers for CPU, AHB and APB busses */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
	/* Initialize PLL and clock divider for the LCD */
	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
	PeriphClkInitStruct.PLLSAI.PLLSAIN = 192;
	PeriphClkInitStruct.PLLSAI.PLLSAIR = 4;
	PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_8;
	HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
	/* Set clock prescaler for ADCs */
	ADC->CCR |= ADC_CCR_ADCPRE_0;
}


/** ***************************************************************************
 * @brief Disable the GYRO on the microcontroller board.
 *
 * @note MISO of the GYRO is connected to PF8 and CS to PC1.
 * @n Some times the GYRO goes into an undefined mode at startup
 * and pulls the MISO low or high thus blocking the analog input on PF8.
 * @n The simplest solution is to pull the CS of the GYRO low for a short while
 * which is done with the code below.
 * @n PF8 is also reconfigured.
 * @n An other solution would be to remove the GYRO
 * from the microcontroller board by unsoldering it.
 *****************************************************************************/
static void gyro_disable(void)
{
	__HAL_RCC_GPIOC_CLK_ENABLE();		// Enable Clock for GPIO port C
	/* Disable PC1 and PF8 first */
	GPIOC->MODER &= ~GPIO_MODER_MODER1; // Reset mode for PC1
	GPIOC->MODER |= GPIO_MODER_MODER1_0;	// Set PC1 as output
	GPIOC->BSRR |= GPIO_BSRR_BR1;		// Set GYRO (CS) to 0 for a short time
	HAL_Delay(10);						// Wait some time
	GPIOC->MODER |= GPIO_MODER_MODER1_Msk; // Analog mode PC1 = ADC123_IN11
	__HAL_RCC_GPIOF_CLK_ENABLE();		// Enable Clock for GPIO port F
	GPIOF->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED8;	// Reset speed of PF8
	GPIOF->AFR[1] &= ~GPIO_AFRH_AFSEL8;			// Reset alternate func. of PF8
	GPIOF->PUPDR &= ~GPIO_PUPDR_PUPD8;			// Reset pulup/down of PF8
	HAL_Delay(10);						// Wait some time
	GPIOF->MODER |= GPIO_MODER_MODER8_Msk; // Analog mode for PF6 = ADC3_IN4
}
