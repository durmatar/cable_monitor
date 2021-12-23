/** ***************************************************************************
 * @file
 * @brief See lcd_gui.c
 *
 * Prefix GUI
 *
 *****************************************************************************/

#ifndef INC_LCD_GUI_H_
#define INC_LCD_GUI_H_

/******************************************************************************
 * Includes
 *****************************************************************************/
#include "stdint.h"
#include "stdbool.h"

/******************************************************************************
 * Types
 *****************************************************************************/
/** Struct with fields of mode entry */
typedef struct {
	char line[16];						///< Text
	uint32_t back_color;				///< Background color
	uint32_t frame_color;				///< Frame color
} MODE_entry_t;

/** Enumeration of possible modes */
typedef enum {
	MODE_L = 0, MODE_LN, MODE_LNPE
} GUI_mode_t;

/** Struct with fields of options entry */
typedef struct {
	char title[16];						///< Text
	char optn0[16];						///< Option 1
	char optn1[16];						///< Option 2
	char optn2[16];						///< Option 3
	uint16_t active;					///< Active option
	uint16_t optnCount;					///< Option count
	bool disabled;						///< Option disabled
} OPTN_entry_t;

/** Enumeration of possible sites */
typedef enum {
	SITE_NONE = 0, SITE_MEAS, SITE_OPTN, SITE_CALI, SITE_HINT, SITE_MAIN
} GUI_site_t;

/** Enumeration of possible TS inputs */
typedef enum {
	TOUCH_NONE = 0, TOUCH_GENERAL, TOUCH_MODE, TOUCH_OPTN, TOUCH_OPTN_CHANGE
} GUI_touch_t;

/******************************************************************************
 * Defines
 *****************************************************************************/
//Mode and Cable deteced
extern bool GUI_cable_detected;
extern bool GUI_cable_not_detected;
extern GUI_mode_t GUI_mode;

//General measurements
extern uint32_t GUI_measAccuracy;
extern float GUI_angle;
extern float GUI_distance;
extern float GUI_distanceDeviation;
extern float GUI_current;

//Raw measurements
extern uint32_t GUI_rawHallLeft;
extern uint32_t GUI_rawHallRight;
extern uint32_t GUI_rawWpcLeft;
extern uint32_t GUI_rawWpcRight;

extern OPTN_entry_t GUI_options[3];

extern bool GUI_inputBtn;
extern bool GUI_inputMeasReady;
extern bool GUI_outOptn;

/******************************************************************************
 * Functions
 *****************************************************************************/
void GUI_DrawHint(void);
void GUI_DrawModeSel(void);
void GUI_DrawTopMode(void);
void GUI_DrawTopOptions(void);
void GUI_DrawMeasurement(void);
void GUI_DrawOptions(void);
void GUI_DrawRaw(void);
void GUI_SiteHandler(void);
void GUI_TSHandler(void);


#endif /* INC_LCD_GUI_H_ */
