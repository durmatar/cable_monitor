/** ***************************************************************************
 * @file
 * @brief See measuring.c
 *
 * Prefixes MEAS, ADC, DAC
 *
 *****************************************************************************/

#ifndef MEAS_H_
#define MEAS_H_


/******************************************************************************
 * Includes
 *****************************************************************************/
#include <stdbool.h>


/******************************************************************************
 * Defines
 *****************************************************************************/
extern bool MEAS_data_ready;			///< New data is ready
extern uint32_t MEAS_amplitude_left;	///< Amplitude of the left channel
extern uint32_t MEAS_amplitude_right;	///< Amplitude of the right channel


/******************************************************************************
 * Functions
 *****************************************************************************/
void MEAS_GPIO_analog_init(void);
void MEAS_timer_init(void);
void ADC_reset(void);
void ADC3_IN13_IN4_scan_init(void);
void ADC3_IN11_IN6_scan_init(void);
void ADC3_dual_scan_start(void);

void MEAS_analyse_data(void);

#endif
