/** ***************************************************************************
 * @file
 * @brief The LCD GUI
 *
 * Description
 *
 * @author  Jonas Bollhalder, bollhjon@students.zhaw.ch
 * @date	20.12.2021
 *****************************************************************************/

/******************************************************************************
 * Includes
 *****************************************************************************/
#include "lcd_gui.h"

#include "stdio.h"

#include "stm32f4xx.h"
#include "stm32f429i_discovery.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ts.h"


/******************************************************************************
 * Defines
 *****************************************************************************/
#define MODE_FONT			&Font20 ///< Possible font sizes: 8 12 16 20 24
#define MODE_HEIGHT			40		///< Height of mode select bar
#define MODE_MARGIN			2		///< Margin around menu entry
#define MODE_Y				(BSP_LCD_GetYSize()-40) ///< Locate bar at bottom of screen
#define MODE_ENTRY_COUNT	3		///< Number of menu entries

#define TOP_FONT			&Font20	///< Possible font sizes: 8 12 16 20 24
#define TOP_HEIGHT			40		///< Height of top bar
#define	TOP_MARGIN			2		///< Margin around top bar elements


/******************************************************************************
 * Variables
 *****************************************************************************/
static MODE_entry_t MODE_entry[MODE_ENTRY_COUNT] = {
		{" L",		LCD_COLOR_LIGHTRED,		LCD_COLOR_RED},
		{" LN",		LCD_COLOR_LIGHTBLUE,	LCD_COLOR_BLUE},
		{"LNPE",	LCD_COLOR_LIGHTGREEN,	LCD_COLOR_GREEN},
};

bool GUI_cable_detected = false;///< Input true if cable detected with last measurement
GUI_mode_t GUI_mode = MODE_L;	///< Default measurement mode

//General measurements
uint32_t GUI_measAccuracy = 10;	///< Settings for measurement accuracy
float GUI_distance = 5; 		///< Distance value to display
float GUI_distanceDeviation = 35; ///< Standard deviation of distance
float GUI_angle = 0;			///< Angle value to display
float GUI_current = 4;			///< Current to display

//Raw measurements
uint32_t GUI_rawHallLeft = 5000;
uint32_t GUI_rawHallRight = 5000;
uint32_t GUI_rawWpcLeft = 5000;
uint32_t GUI_rawWpcRight = 5000;

OPTN_entry_t GUI_options[3] = {
		{"Display Data","Analysed","Raw","",0,2,false},
		{"Meas. Type","Single","Continous","",0,2,false},
		{"Accuracy","1x","5x","10x",0,3,false},
};

GUI_site_t GUI_currentSite = SITE_NONE;
GUI_site_t GUI_previousSite = SITE_NONE;

bool GUI_inputBtn = false;
bool GUI_inputTS = false;
bool GUI_inputMeasReady = false;

TS_StateTypeDef GUI_previousTSstate;
TS_StateTypeDef GUI_currentTSstate;
GUI_touch_t GUI_TSinputType = TOUCH_NONE;

/******************************************************************************
 * Functions
 *****************************************************************************/

/** ***************************************************************************
 * @brief Wrapper for touch screen sate
 *
 * Enable use of BSP_TS_GetState() function with 180° rotated display
 *****************************************************************************/
void GUI_TS_GetState(TS_StateTypeDef* TS_State){
	//readout TS state
	BSP_TS_GetState(TS_State);
	//translate to correct coordinate system
	TS_State->X = BSP_LCD_GetXSize()-TS_State->X;
	//TS_State->Y = BSP_LCD_GetYSize()-TS_State->Y;
}


/** ***************************************************************************
 * @brief Wrapper to draw filled rectangle
 *
 * Enable use of BSP_LCD_FillRect() function with 180° rotated display
 *****************************************************************************/
void GUI_LCD_FillRect(uint16_t Xpos, uint16_t Ypos, uint16_t Width, uint16_t Height){
	//calculate diagonal corner coordinates and translate to other system
	Xpos = BSP_LCD_GetXSize()-(Xpos+Width);
	Ypos = BSP_LCD_GetYSize()-(Ypos+Height);
	//draw rectangular shape
	BSP_LCD_FillRect(Xpos, Ypos, Width, Height);
}


/** ***************************************************************************
 * @brief Wrapper to draw rectangle
 *
 * Enable use of BSP_LCD_DrawRect() function with 180° rotated display
 *****************************************************************************/
void GUI_LCD_DrawRect(uint16_t Xpos, uint16_t Ypos, uint16_t Width, uint16_t Height){
	//calculate diagonal corner coordinates and translate to other system
	Xpos = BSP_LCD_GetXSize()-(Xpos+Width);
	Ypos = BSP_LCD_GetYSize()-(Ypos+Height);
	//draw rectangular shape
	BSP_LCD_DrawRect(Xpos, Ypos, Width, Height);
}


/** ***************************************************************************
 * @brief Wrapper to draw horizontal line
 *
 * Enable use of BSP_LCD_DrawHLine() function with 180° rotated display
 *****************************************************************************/
void GUI_LCD_DrawHLine(uint16_t Xpos, uint16_t Ypos, uint16_t Length){
	//translate to other system
	Xpos = BSP_LCD_GetXSize()-(Xpos+Length);
	Ypos = BSP_LCD_GetYSize()-Ypos;
	//draw rectangular shape
	BSP_LCD_DrawHLine(Xpos, Ypos, Length);
}


/** ***************************************************************************
 * @brief Wrapper to draw vertical line
 *
 * Enable use of BSP_LCD_DrawVLine() function with 180° rotated display
 *****************************************************************************/
void GUI_LCD_DrawVLine(uint16_t Xpos, uint16_t Ypos, uint16_t Length){
	//translate to other system
	Xpos = BSP_LCD_GetXSize()-Xpos;
	Ypos = BSP_LCD_GetYSize()-(Ypos+Length);
	//draw rectangular shape
	BSP_LCD_DrawHLine(Xpos, Ypos, Length);
}


/** ***************************************************************************
 * @brief Wrapper to draw circle
 *
 * Enable use of BSP_LCD_DrawCircle() function with 180° rotated display
 *****************************************************************************/
void GUI_LCD_DrawCircle(uint16_t Xpos, uint16_t Ypos, uint16_t Radius){
	//translate to other system
	Xpos = BSP_LCD_GetXSize()-Xpos;
	Ypos = BSP_LCD_GetYSize()-Ypos;
	//draw rectangular shape
	BSP_LCD_DrawCircle(Xpos, Ypos, Radius);
}


/** ***************************************************************************
 * @brief Wrapper to draw filled circle
 *
 * Enable use of BSP_LCD_FillCircle() function with 180° rotated display
 *****************************************************************************/
void GUI_LCD_FillCircle(uint16_t Xpos, uint16_t Ypos, uint16_t Radius){
	//translate to other system
	Xpos = BSP_LCD_GetXSize()-Xpos;
	Ypos = BSP_LCD_GetYSize()-Ypos;
	//draw rectangular shape
	BSP_LCD_FillCircle(Xpos, Ypos, Radius);
}

/** ***************************************************************************
 * @brief Draw hint
 *
 *****************************************************************************/
void GUI_DrawHint(void){
	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_SetFont(&Font24);
	BSP_LCD_DisplayStringAt(5,10, (uint8_t *)"Cable-Monitor", LEFT_MODE);
	BSP_LCD_SetFont(&Font16);
	BSP_LCD_DisplayStringAt(5, 60, (uint8_t *)"Touch on screen or", LEFT_MODE);
	BSP_LCD_DisplayStringAt(5, 80, (uint8_t *)"press blue button",LEFT_MODE);
	BSP_LCD_DisplayStringAt(5, 100, (uint8_t *)"to proceed to", LEFT_MODE);
	BSP_LCD_DisplayStringAt(5, 120, (uint8_t *)"the main sceen", LEFT_MODE);
	BSP_LCD_SetFont(&Font12);
	BSP_LCD_DisplayStringAt(5, 290, (uint8_t *)"(c)bollhjon & durmatar", LEFT_MODE);
	BSP_LCD_DisplayStringAt(5, 305, (uint8_t *)"Version 20.12.2021", LEFT_MODE);
}


/** ***************************************************************************
 * @brief Draw Mode Selection
 *
 *****************************************************************************/
void GUI_DrawModeSel(void){
	BSP_LCD_SetFont(MODE_FONT);
	uint32_t x, y, m, w, h;
	y = MODE_Y;
	m = MODE_MARGIN;
	w = BSP_LCD_GetXSize()/MODE_ENTRY_COUNT;
	h = MODE_HEIGHT;

	for (int i = 0; i < MODE_ENTRY_COUNT; ++i) {
		x = i*w;
		BSP_LCD_SetTextColor(MODE_entry[i].back_color);
		GUI_LCD_FillRect(x+m, y+m, w-2*m, h-2*m);
		BSP_LCD_SetTextColor(MODE_entry[i].frame_color);
		GUI_LCD_DrawRect(x+m, y+m, w-2*m, h-2*m);
		BSP_LCD_SetBackColor(MODE_entry[i].back_color);
		BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
		BSP_LCD_DisplayStringAt(x+7*m, y+6*m, (uint8_t*)MODE_entry[i].line, LEFT_MODE);
	}
}


/** ***************************************************************************
 * @brief Draw Mode field to Top Bar
 *
 * Display selected Mode and green background if cable was detected
 *****************************************************************************/
void GUI_DrawTopMode(void){
	BSP_LCD_SetFont(TOP_FONT);
	uint32_t x, y, m, w, h;
	x = 0;
	y = 0;
	m = TOP_MARGIN;
	w = (BSP_LCD_GetXSize()/3);
	h = TOP_HEIGHT;

	//Display framed mode and background green if cable detected
	if (GUI_cable_detected){
		BSP_LCD_SetTextColor(LCD_COLOR_LIGHTGREEN);
		BSP_LCD_SetBackColor(LCD_COLOR_LIGHTGREEN);
	} else {
		BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
		BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	}
	GUI_LCD_FillRect(x+m, y+m, (w*2)-2*m, h-2*m);
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	GUI_LCD_DrawRect(x+m, y+m, (w*2)-2*m, h-2*m);
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_DisplayStringAt(x+3*m, y+6*m, (uint8_t*)"Mode:", LEFT_MODE);
	BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
	//display current mode
	switch (GUI_mode) {
		case MODE_L:
			BSP_LCD_DisplayStringAt(x+3*m+12*7, y+6*m, (uint8_t*)"L", LEFT_MODE);
			break;
		case MODE_LN:
			BSP_LCD_DisplayStringAt(x+3*m+12*7, y+6*m, (uint8_t*)"LN", LEFT_MODE);
			break;
		case MODE_LNPE:
			BSP_LCD_DisplayStringAt(x+3*m+12*7, y+6*m, (uint8_t*)"LNPE", LEFT_MODE);
			break;
		default:
			break;
	}
}


/** ***************************************************************************
 * @brief Draw options field to top bar
 *
 * Draw options field in top bar
 *****************************************************************************/
void GUI_DrawTopOptions(void){
	BSP_LCD_SetFont(TOP_FONT);
	uint32_t x, y, m, w, h;
	x = 0;
	y = 0;
	m = TOP_MARGIN;
	w = (BSP_LCD_GetXSize()/3);
	h = TOP_HEIGHT;

	//display Options area
	BSP_LCD_SetTextColor(LCD_COLOR_LIGHTGRAY);
	GUI_LCD_FillRect(x+m+2*w, y+m, w-2*m, h-2*m);
	BSP_LCD_SetBackColor(LCD_COLOR_LIGHTGRAY);
	//display according to site state
	if (GUI_currentSite != SITE_OPTN) {
		BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
		BSP_LCD_DisplayStringAt(x+7*m+2*w, y+6*m, (uint8_t*)"OPTN", LEFT_MODE);
		GUI_LCD_DrawRect(x+m+2*w, y+m, w-2*m, h-2*m);
	} else {
		BSP_LCD_SetTextColor(LCD_COLOR_RED);
		BSP_LCD_DisplayStringAt(x+7*m+2*w, y+6*m, (uint8_t*)"BACK", LEFT_MODE);
		GUI_LCD_DrawRect(x+m+2*w, y+m, w-2*m, h-2*m);
	}
}


/** ***************************************************************************
 * @brief Draw options field to top bar
 *
 * Draw options field in top bar
 *****************************************************************************/
void GUI_ClearSite(void){
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	GUI_LCD_FillRect(0, 40, 240, 240);
}


/** ***************************************************************************
 * @brief Display measurements
 *
 * Always angle and distance, if measuring accuracy factor is greater than 1
 * also display standard deviation and accuracy factor, if distance is smaller
 * than 10mm display current
 *****************************************************************************/
void GUI_DrawMeasurement(void){
	GUI_ClearSite();
	//display angle
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	GUI_LCD_FillRect(0, 45, 240, 70);
	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_DrawCircle(120, 110, 50);
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	GUI_LCD_FillRect(0, 110, 240, 60);
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_DrawLine(60, 110, 180, 110);
	BSP_LCD_DrawLine(120, 50, 120, 110);
	//display angle direction
	if ((-46>GUI_angle)|(GUI_angle<46)) {
		BSP_LCD_SetTextColor(LCD_COLOR_RED);
		uint16_t x,y;
		float dx,dy;
		dx = 0.888;
		dy = 0.333;
		if (GUI_angle>0) {
			x=(uint16_t)(120+(int)(dx*GUI_angle));
			y=(uint16_t)(55+(int)(dy*GUI_angle));
		} else {
			x=(uint16_t)(120+(int)(dx*GUI_angle));
			y=(uint16_t)(55-(int)(dy*GUI_angle));
		}
		BSP_LCD_DrawLine(120, 110, x, y);
	}

	//Display Text
	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_SetFont(&Font16);
	char text[25];
	uint32_t x = 30;
	uint32_t y = 125;
	//Angle
	snprintf(text,24,"Angle:    %4ddeg", (int)(GUI_angle));
	BSP_LCD_DisplayStringAt(x, y, (uint8_t *)text, LEFT_MODE);
	y = y+30;
	//Distance
	snprintf(text,24,"Distance: %4dmm", (int)(GUI_distance));
	BSP_LCD_DisplayStringAt(x, y, (uint8_t *)text, LEFT_MODE);
	if (GUI_measAccuracy > 1) {
		//Standard deviation
		y = y+20;
		snprintf(text,24,"Std.Dev.: %4dmm", (int)(GUI_distanceDeviation));
		BSP_LCD_DisplayStringAt(x, y, (uint8_t *)text, LEFT_MODE);
		//Measurement count
		y = y+20;
		BSP_LCD_SetFont(&Font16);
		snprintf(text,24,"Accuracy: %4dx", (int)(GUI_measAccuracy));
		BSP_LCD_DisplayStringAt(x, y, (uint8_t *)text, LEFT_MODE);
	}
	//Current
	if (GUI_distance <= 10) {
		y = y+30;
		BSP_LCD_SetFont(&Font16);
		snprintf(text,24,"Current:  %4dA", (int)(GUI_current));
		BSP_LCD_DisplayStringAt(x, y, (uint8_t *)text, LEFT_MODE);
	}
	//Display measuring type
	y = y+30;
	if (GUI_options[1].active==0) {
		BSP_LCD_DisplayStringAt(x, y, (uint8_t *)"Meas.Type:   sng", LEFT_MODE);
	} else {
		BSP_LCD_DisplayStringAt(x, y, (uint8_t *)"Meas.Type:  cont", LEFT_MODE);
	}
}


/** ***************************************************************************
 * @brief Draw options site
 *
 * Draw options window to adjust settings
 * Available settings:
 *  - Meassuring Accuracy (1x, 5x, 10x)
 *  - Continous Meassuring (single, continous)
 *  - Display values (analysed, raw)
 *****************************************************************************/
void GUI_DrawOptions(void){
	uint32_t x, y, m, w, h;
	x = 0;
	m = 4;
	h = 40;
	for (int i = 0; i < 3; ++i) {
		y=38+i*80;
		w=240;
		BSP_LCD_SetTextColor(LCD_COLOR_LIGHTGRAY);
		GUI_LCD_FillRect(x+m, y+m, w-2*m, 2*h-m);

		BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
		BSP_LCD_SetBackColor(LCD_COLOR_LIGHTGRAY);
		GUI_LCD_DrawRect(x+m, y+m, w-2*m, h);
		BSP_LCD_SetFont(&Font20);
		BSP_LCD_DisplayStringAt(x+3*m, y+3*m, (uint8_t *)GUI_options[i].title, LEFT_MODE);

		for (int j = 0; j < GUI_options[i].optnCount; ++j) {
			w = (240-2*m)/GUI_options[i].optnCount;
			if (GUI_options[i].active == j) {
				BSP_LCD_SetTextColor(LCD_COLOR_LIGHTCYAN);
				BSP_LCD_SetBackColor(LCD_COLOR_LIGHTCYAN);
				GUI_LCD_FillRect(x+m+j*w, y+m+h, w, h-m);
			} else {
				BSP_LCD_SetBackColor(LCD_COLOR_LIGHTGRAY);
			}
			BSP_LCD_SetFont(&Font16);
			if ((j>0)&&(GUI_options[i].disabled)) {
				BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
			} else {
				BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
			}
			GUI_LCD_DrawRect(x+m+j*w, y+m+h, w, h-m);
			uint8_t * text;
			switch (j) {
				case 0:
					text = (uint8_t *)GUI_options[i].optn0;
					break;
				case 1:
					text = (uint8_t *)GUI_options[i].optn1;
					break;
				case 2:
					text = (uint8_t *)GUI_options[i].optn2;
					break;
				default:
					break;
			}
			BSP_LCD_DisplayStringAt(x+3*m+j*w, y+4*m+h, text, LEFT_MODE);
		}
	}
}


/** ***************************************************************************
 * @brief Display raw measurements
 *
 * Display raw amplitude values of all sensors
 *****************************************************************************/
void GUI_DrawRaw(void){
	GUI_ClearSite();
	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);

	char text[25];
	uint32_t x = 30;
	uint32_t y = 60;
	//Hall Sensors
	BSP_LCD_SetFont(&Font20);
	BSP_LCD_DisplayStringAt(x, y, (uint8_t *)"Hall Sensors:", LEFT_MODE);
	y = y+20;
	BSP_LCD_SetFont(&Font16);
	snprintf(text,24,"Right:    %5d", (int)(GUI_rawHallRight));
	BSP_LCD_DisplayStringAt(x, y, (uint8_t *)text, LEFT_MODE);
	y = y+20;
	snprintf(text,24,"Left:     %5d", (int)(GUI_rawHallLeft));
	BSP_LCD_DisplayStringAt(x, y, (uint8_t *)text, LEFT_MODE);
	y = y+35;
	//WPC Sensors
	BSP_LCD_SetFont(&Font20);
	BSP_LCD_DisplayStringAt(x, y, (uint8_t *)"WPC Sensors:", LEFT_MODE);
	y = y+20;
	BSP_LCD_SetFont(&Font16);
	snprintf(text,24,"Right:    %5d", (int)(GUI_rawWpcRight));
	BSP_LCD_DisplayStringAt(x, y, (uint8_t *)text, LEFT_MODE);
	y = y+20;
	snprintf(text,24,"Left:     %5d", (int)(GUI_rawWpcLeft));
	BSP_LCD_DisplayStringAt(x, y, (uint8_t *)text, LEFT_MODE);
}


/** ***************************************************************************
 * @brief Manage LCD
 *
 * Read out GUI_inputs and display sites accordingly
 * This Function needs to be called every cycle
 *****************************************************************************/
void GUI_SiteHandler(void){
	GUI_TSHandler();
	//Init LCD with hint when no site is selected
	switch (GUI_currentSite) {
		case SITE_NONE:
			GUI_DrawHint();
			GUI_currentSite = SITE_HINT;
			break;
		case SITE_HINT:
			if(GUI_inputBtn | GUI_inputTS){
				BSP_LCD_Clear(LCD_COLOR_WHITE);
				GUI_DrawTopMode();
				GUI_DrawTopOptions();
				GUI_DrawModeSel();
				GUI_currentSite = SITE_MAIN;
			}
			break;
		case SITE_MAIN:
			if(GUI_inputTS){
				//Display updated mode or go to options
				if (GUI_TSinputType == TOUCH_MODE) {
					GUI_DrawTopMode();
				} else if (GUI_TSinputType == TOUCH_OPTN) {
					GUI_currentSite = SITE_OPTN;
					GUI_ClearSite();
					GUI_DrawOptions();
					GUI_DrawTopOptions();
				}

			} else if (GUI_inputMeasReady) {
				//Display Measurement
				if(GUI_options[0].active==0){
					//analysed
					GUI_DrawMeasurement();
					GUI_DrawTopMode();
				} else {
					//Raw
					GUI_DrawRaw();
					GUI_DrawTopMode();
				}
				GUI_currentSite = SITE_MEAS;
			}
			break;
		case SITE_MEAS:
			if(GUI_inputTS){
				//Display updated mode or go to options
				if (GUI_TSinputType == TOUCH_MODE) {
					GUI_DrawTopMode();
				} else if (GUI_TSinputType == TOUCH_OPTN) {
					GUI_currentSite = SITE_OPTN;
					GUI_ClearSite();
					GUI_DrawOptions();
					GUI_DrawTopOptions();
				}

			} else
			if (GUI_inputMeasReady) {
				//Display Measurement
				if(GUI_options[0].active==0){
					//analysed
					GUI_DrawMeasurement();
					GUI_DrawTopMode();
				} else {
					//Raw
					GUI_DrawRaw();
					GUI_DrawTopMode();
				}
			}
			break;
		case SITE_OPTN:
			if(GUI_inputTS){
			//Display updated mode, updated settings or go to main screen
				if (GUI_TSinputType == TOUCH_MODE) {
					GUI_DrawTopMode();
				} else if (GUI_TSinputType == TOUCH_OPTN) {
					GUI_currentSite = SITE_MEAS;
					GUI_ClearSite();
					if(GUI_options[0].active==0){
						//analysed
						GUI_DrawMeasurement();
						GUI_DrawTopMode();
					} else {
						//Raw
						GUI_DrawRaw();
						GUI_DrawTopMode();
					}
					GUI_DrawTopOptions();
				} else if (GUI_TSinputType == TOUCH_OPTN_CHANGE){
					GUI_DrawOptions();
				}
			}
		default:
			break;
	}

	//Reset Inputs
	GUI_inputBtn = false;
	GUI_inputTS = false;
	GUI_inputMeasReady = false;
	GUI_TSinputType = TOUCH_NONE;
}


/** ***************************************************************************
 * @brief Handle touch screen inputs
 *
 * Determine touch input from Touch position and current site
 *****************************************************************************/
void GUI_TSHandler(void){
	GUI_TS_GetState(&GUI_currentTSstate);
	//detect rising edge of touch input
	if ((GUI_currentTSstate.TouchDetected==1) & (GUI_previousTSstate.TouchDetected==0)) {
		//set touch input to true
		GUI_inputTS = true;
		uint16_t X,Y;
		X = GUI_currentTSstate.X;
		Y = GUI_currentTSstate.Y;
		if (GUI_currentSite == SITE_HINT) {
			GUI_TSinputType = TOUCH_GENERAL;
		}
		//detect mode change
		if ((GUI_currentSite == SITE_MAIN)|
			(GUI_currentSite == SITE_MEAS)|
			(GUI_currentSite == SITE_OPTN)) {
			if ((Y>280) & (X<80) & (GUI_mode != MODE_L)) {
				GUI_TSinputType = TOUCH_MODE;
				GUI_mode = MODE_L;
			} else if ((Y>280) & (80<X) & (X<160) & (GUI_mode != MODE_LN)) {
				GUI_TSinputType = TOUCH_MODE;
				GUI_mode = MODE_LN;
			} else if ((Y>280) & (160<X) & (GUI_mode != MODE_LNPE)) {
				GUI_TSinputType = TOUCH_MODE;
				GUI_mode = MODE_LNPE;
			}
			//detect option area
			if ((Y<40) & (X>160)) {
				GUI_TSinputType = TOUCH_OPTN;
				HAL_Delay(200);
			}
		}
		//detect option changes
		if (GUI_currentSite == SITE_OPTN) {
			if ((80<Y)&(Y<120)){
				if ((X<120)&(GUI_options[0].active!=0)) {
					GUI_options[0].active=0;
					GUI_options[1].disabled = false;
					GUI_options[2].disabled = false;
					GUI_TSinputType = TOUCH_OPTN_CHANGE;
				} else if ((X>120)&(GUI_options[0].active!=1)) {
					GUI_options[0].active=1;
					GUI_options[1].disabled = true;
					GUI_options[1].active = 0;
					GUI_options[2].disabled = true;
					GUI_options[2].active = 0;
					GUI_TSinputType = TOUCH_OPTN_CHANGE;
				}
			} else if ((160<Y)&(Y<200)&!(GUI_options[1].disabled)){
				if ((X<120)&(GUI_options[1].active!=0)) {
					GUI_options[1].active = 0;
					GUI_options[2].disabled = false;
					GUI_TSinputType = TOUCH_OPTN_CHANGE;
				} else if ((X>120)&(GUI_options[1].active!=1)) {
					GUI_options[1].active = 1;
					GUI_options[2].disabled = true;
					GUI_options[2].active = 0;
					GUI_TSinputType = TOUCH_OPTN_CHANGE;
				}
			} else if ((240<Y)&(Y<280)&!(GUI_options[2].disabled)){
				if ((X<80)&(GUI_options[2].active!=0)) {
					GUI_options[2].active = 0;
					GUI_TSinputType = TOUCH_OPTN_CHANGE;
				} else if ((X>80)&(X<160)&(GUI_options[2].active!=1)) {
					GUI_options[2].active = 1;
					GUI_TSinputType = TOUCH_OPTN_CHANGE;
				} else if ((X>160)&(GUI_options[2].active!=2)) {
					GUI_options[2].active = 2;
					GUI_TSinputType = TOUCH_OPTN_CHANGE;
				}
			}
		}
	}
	//save current TS state as previous state
	GUI_previousTSstate.TouchDetected = GUI_currentTSstate.TouchDetected;
	GUI_previousTSstate.X = GUI_currentTSstate.X;
	GUI_previousTSstate.Y = GUI_currentTSstate.Y;
	GUI_previousTSstate.Z = GUI_currentTSstate.Z;
}







