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
extern bool ANA_inMeasReady;   ///< Input button pushed event
extern bool ANA_inBtn;		   ///< Input measurement ready event
extern uint32_t ANA_inAmpLeft; ///< Input raw amplitude left
extern uint32_t ANA_inAmpRight;///< Input raw amplitude right
extern uint16_t ANA_inOptn[4]; ///< Input Mode,DataType,MeasuringType,Accuracy

//outputs
extern bool ANA_outStartHALL;  ///< Output start hall measurement event
extern bool ANA_outStartWPC;   ///< Output start wpc measurement event
extern bool ANA_outDataReady;  ///< Output data ready event
extern float ANA_outResults[4];///< Output analysed results
extern bool ANA_measBusy;	   ///< Output measurement state


/******************************************************************************
 * Functions
 *****************************************************************************/
void ANA_Handler(void);



#endif /* INC_ANALYTICS_H_ */
