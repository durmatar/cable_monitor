/** ***************************************************************************
 * @file
 * @brief See measuring.c
 *
 * Prefixes ANA, CALC,
 *
 *****************************************************************************/
#ifndef INC_ANALYTICS_H_
#define INC_ANALYTICS_H_
/******************************************************************************
 * Includes
 *****************************************************************************/
#include "stdint.h"
#include "stdbool.h"
/******************************************************************************
 * Defines
 *****************************************************************************/

/******************************************************************************
 * Variables
 *****************************************************************************/
//inputs
extern bool ANA_inMeasReady;
extern bool ANA_inBtn;
extern uint32_t ANA_inAmpLeft;
extern uint32_t ANA_inAmpRight;
extern uint16_t ANA_inOptn[4];

//outputs
extern bool ANA_outStartHALL;
extern bool ANA_outStartWPC;
extern bool ANA_outDataReady;
extern float ANA_outResults[4];
extern bool ANA_measBusy;


/******************************************************************************
 * Functions
 *****************************************************************************/
void ANA_Handler(void);



#endif /* INC_ANALYTICS_H_ */
