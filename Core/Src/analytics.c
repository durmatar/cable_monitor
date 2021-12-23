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
#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"

#include "math.h"
#include "analytics.h"

/******************************************************************************
 * Defines
 *****************************************************************************/
#define CALC_ADCVOLTRESOLUTION	(float)(0.0008056640625)
#define CALC_AMPOPAMP			(float)(95)
#define CALC_AMPHALLSENS		(float)(90)
#define CALC_PIDANDPERM			(float)(4998556.330)

#define CALC_LUTSIZE			11

/******************************************************************************
 * Variables
 *****************************************************************************/
bool ANA_inBtn = false;
bool ANA_inMeasReady = false;
uint32_t ANA_inAmpLeft = 0;
uint32_t ANA_inAmpRight = 0;
uint16_t ANA_inOptn[4]={0,0,0,1}; ///< Mode,DataType,MeasuringType,Accuracy
bool ANA_outStartHALL = false;
bool ANA_outStartWPC = false;
float ANA_outResults[4];			///<Angle,Distance,Std.Dev.,Current
bool ANA_outDataReady = false;

bool ANA_measBusy = false;
bool ANA_wpcBusy = false;
bool ANA_hallBusy = false;
uint16_t ANA_cycle = 0;

float ANA_wpcLeft[10];
float ANA_wpcRight[10];
float ANA_hallLeft[10];
float ANA_hallRight[10];

// Distance [cm]
float CALC_distanceLUT[CALC_LUTSIZE] = { 0, 10, 20, 30, 40, 50, 70, 100, 150, 200, 300};

// Measurments with 1L
float CALC_wpcRightL[CALC_LUTSIZE] = {810,690,620,565,530,510,490,450,395,380,330};
float CALC_wpcLeftL[CALC_LUTSIZE] = {795,740,683,570,540,510,490,460,430,420,410};

// Measurment with LN
float CALC_wpcRightLN[CALC_LUTSIZE] = {570,510,430,375,340,330,290,265,245,195,165};
float CALC_wpcLeftLN[CALC_LUTSIZE] = {365,350,350,325,320,305,275,265,262,215,210};


// Measurment with LNPE
float CALC_wpcRightLNPE[CALC_LUTSIZE] = {450,363,306,283,273,267,263,237,215,198,170};
float CALC_wpcLeftLNPE[CALC_LUTSIZE] = {315,292,280,263,260,255,242,235,220,211,204};

// Array to LUTs
float* CALC_wpcLeft[3] = {CALC_wpcLeftL,CALC_wpcLeftLN,CALC_wpcLeftLNPE};
float* CALC_wpcRight[3] = {CALC_wpcRightL,CALC_wpcRightLN,CALC_wpcRightLNPE};

/******************************************************************************
 * Functions
 *****************************************************************************/

/** ***************************************************************************
 * @brief Calculate Angle with three length inputs
 *****************************************************************************/
float CALC_Angle(float left, float right, float middle){
	left = left / middle;
	right = right / middle;
	middle = 1;

	float x;

	if (left < (middle-0.2)) {
		x = -45;
		//-45° to 0°
	} else if (right < (middle-0.2)){
		//0° - 45°
		x = 45;
	} else {
		x = 0;
	}



	return x;
}


/** ***************************************************************************
 * @brief Calculate electrical current from magnetic field
 * distance[m] amplitude[1]
 * return current[A]
 *****************************************************************************/
float CALC_ElCurrent(float amplitude, float distance){
	float I,B;
	// Calculate electro-magnetic field strength
	B = (((amplitude*CALC_ADCVOLTRESOLUTION)/CALC_AMPOPAMP)/CALC_AMPHALLSENS);

	// Calculate current
	I = CALC_PIDANDPERM*(distance/B);

	//return current
	return I;
}


/** ***************************************************************************
 * @brief Calculate distance
 *****************************************************************************/
float CALC_Distance(float* lutDistance, float* lutStrenght, float measurement){
	int t,d,s;
	float distance = -1;
	t=0;
	s=-10;
	d=-10;

	// Catch to high and to low values
	if(lutStrenght[CALC_LUTSIZE-1]>measurement){
		measurement = lutStrenght[CALC_LUTSIZE-1];
	} else if (measurement>lutStrenght[0]) {
		measurement = lutStrenght[0];
	}

	for(int i=0; i < CALC_LUTSIZE ; i++ ){
		if (measurement == lutStrenght[i]){
			t = i;
			s=t;
		}
		if ( (measurement < lutStrenght[i])&& (measurement > lutStrenght[i+1])){
			t = i;
			d=t;
		}
	}

	if(t==d){
		float a = (lutDistance[t+1]-lutDistance[t])/(lutStrenght[t+1]-lutStrenght[t]);
		distance = a*(measurement-lutStrenght[t]) + lutDistance[t];
	}

	if(t==s){
		distance = lutDistance[t];
	}

	return distance;
}


/** ***************************************************************************
 * @brief Calculate distance from measurement input and mode setting
 *****************************************************************************/
float CALC_DistanceMode(float measurement, uint16_t mode, bool right){
	float distance;
	float* lut;
	// Select left or right
	if (right) {
		lut = CALC_wpcRight[mode];
	} else {
		lut = CALC_wpcLeft[mode];
	}
	// Calculate distance with correct mode
	distance = CALC_Distance(CALC_distanceLUT, lut, measurement);

	return distance;
}



/** ***************************************************************************
 * @brief Analytics handler
 *****************************************************************************/
void ANA_Handler(void){
	//start measurement with button input
	if (ANA_inBtn & !ANA_measBusy) {
		ANA_measBusy = true;
		ANA_inBtn = false;
	}

	//collect data
	if (ANA_wpcBusy & ANA_inMeasReady) {
		ANA_wpcLeft[ANA_cycle]=(float)ANA_inAmpLeft;
		ANA_wpcRight[ANA_cycle]=(float)ANA_inAmpRight;
	} else if (ANA_hallBusy & ANA_inMeasReady){
		ANA_hallLeft[ANA_cycle]=(float)ANA_inAmpLeft;
		ANA_hallRight[ANA_cycle]=(float)ANA_inAmpRight;
	}

	//while meas busy, start measurements
	if (ANA_measBusy){
		//start wpc
		if ((ANA_cycle < ANA_inOptn[3])&(!ANA_wpcBusy)&(!ANA_hallBusy)) {
			ANA_outStartWPC = true;
			ANA_wpcBusy = true;
		//start hall
		} else if (ANA_wpcBusy & ANA_inMeasReady){
			ANA_outStartHALL = true;
			ANA_wpcBusy = false;
			ANA_hallBusy = true;
		} else if (ANA_hallBusy & ANA_inMeasReady){
			ANA_hallBusy = false;
			ANA_cycle ++;
		}
	}

	//When cycles finished or in continuous mode and single cycle is completed
	if ((ANA_cycle == ANA_inOptn[3])&(!ANA_wpcBusy)&(!ANA_hallBusy)) {
		//Analyse data
		float mean,stdDeviation,angle,current;
		mean = 0;
		stdDeviation = 0;
		angle = 0;
		current = 0;
		if (ANA_inOptn[1]==0) {
			int accuracy = ANA_inOptn[3];
			// Calculate distance
			for (int i = 0; i < accuracy; ++i) {
				ANA_wpcLeft[i]=CALC_DistanceMode(ANA_wpcLeft[i], ANA_inOptn[0], false);
				ANA_wpcRight[i]=CALC_DistanceMode(ANA_wpcRight[i], ANA_inOptn[0], true);
			}
			// Sum
			float sumLeft,sumRight;
			for (int i = 0; i < accuracy; ++i) {
				sumLeft = sumLeft+ANA_wpcLeft[i];
				sumRight = sumRight+ANA_wpcRight[i];
			}
			// Mean
			float meanLeft,meanRight;
			meanLeft = sumLeft/accuracy;
			meanRight = sumRight/accuracy;
			mean = (sumLeft+sumRight)/(2*accuracy);

			// Standard Deviation
			if (accuracy>1){
				// Deviations
				float deviation[2*accuracy];
				for (int i = 0; i < accuracy; ++i) {
					// Left
					deviation[i]=ANA_wpcLeft[i]-mean;
					deviation[i]=deviation[i]*deviation[i];
					// Right
					deviation[i*2]=ANA_wpcRight[i]-mean;
					deviation[i*2]=deviation[i*2]*deviation[i*2];
				}
				// Variation
				float var = 0;
				for (int i = 0; i < 2*accuracy; ++i) {
					var = var + deviation[1];
				}
				var = var/(accuracy*2);
				// Standard Deviation
				stdDeviation = sqrtf(var);
			}

			// Angle
			angle = CALC_Angle(meanRight, meanLeft, mean);

			// Current
			if ((mean<10)&(mean>0)) {
				// Mean of hall sensor
				float sumHall;
				for (int i = 0; i < accuracy; ++i) {
					sumHall = ANA_hallLeft[i]+sumHall;
					sumHall = ANA_hallRight[i]+sumHall;
				}
				sumHall = sumHall/(2*accuracy);

				current = CALC_ElCurrent(sumHall, (mean/1000));
			}

			// Transfer results
			ANA_outResults[0]=angle; // Angle
			ANA_outResults[1]=mean; // Distance
			ANA_outResults[2]=stdDeviation; //Standard deviation
			ANA_outResults[3]=current; //Current
			ANA_outDataReady = true;

		} else { //transfer raw data
			// Calculate means of all 4 inputs
			float meanWPCright, meanWPCleft, meanHALLright, meanHALLleft;
			meanHALLleft = 0;
			meanHALLright = 0;
			meanWPCleft = 0;
			meanWPCright = 0;

			int accuracy = ANA_inOptn[3];

			for (int i = 0; i < accuracy; ++i) {
				meanHALLleft = meanHALLleft + ANA_hallLeft[i];
				meanHALLright = meanHALLright + ANA_hallRight[i];
				meanWPCleft = meanWPCleft + ANA_wpcLeft[i];
				meanWPCright = meanWPCright + ANA_wpcRight[i];
			}
			meanHALLleft = meanHALLleft / accuracy;
			meanHALLright = meanHALLright / accuracy;
			meanWPCleft = meanWPCleft / accuracy;
			meanWPCright = meanWPCright / accuracy;

			// Transfer results
			ANA_outResults[0]=meanHALLright; // HallRight
			ANA_outResults[1]=meanHALLleft; // HallLeft
			ANA_outResults[2]=meanWPCright; //WPCRight
			ANA_outResults[3]=meanWPCleft; //WPCLeft
			ANA_outDataReady = true;
		}

	}

	//end measurement when cycles are finished
	//or button is pressed when in streaming mode
	if (((ANA_cycle == ANA_inOptn[3])&!(ANA_inOptn[2]==1))|(ANA_inBtn & (ANA_inOptn[2]==1))){
		ANA_measBusy = false;
	}

	// Set cycles to zero if measurement finished
	if (ANA_cycle == ANA_inOptn[3]) {
		ANA_cycle = 0;
	}

	ANA_inBtn = false;
	ANA_inMeasReady = false;
}








