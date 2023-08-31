/*
 * DB_LCD.h
 *
 *  Created on: 02.06.2023
 *      Author: HW
 *
 *  LCD Class for controlling small and large LCD in Deafblind application
 *  Depending on the Config settings lcd and lcdklein it is decided which LCD is present on HW side
 *
 *  Uses:
 *    DHS_Config, class to store information in Flash partition
 *    DHS_Mesh_lite, class to build mesh network
 *    DHS_1Tick_Control, class to control 1-tick Deafblind HW
 *    DHS_LCD, control large LCD display (class derived from)
 *    DHS_LCDklein, control samll LCD display (class derived from)
 *    DHS_FS, flash filesystem storage (has to be initialized before constructing DB_LCD)
 *
 *    esp_wifi, wifi functions
 *
 *  Defines:
 *    DHS_HEADLINE, Header-Text in display, should contain version information (less than 20 chars)
 */

#ifndef SRC_DB_LCD_H_
#define SRC_DB_LCD_H_

#define DHS_HEADLINE "DeafBlind FIPS 2.11" // Header-Text in display, should contain version information (less than 20 chars)

#include "DHS_1Tick_Control.h"
#include "DHS_Config.h"

#include "DHS_LCD.h"
#include "DHS_LCDklein.h"

#include "../src/DHS_Mesh_lite.h"
#include "esp_wifi.h"

class DB_LCD: public DHS_LCD, DHS_LCDklein {
  public:

   	DB_LCD(DHS_1Tick_Control *Cont, DHS_Config *Conf); // initialize LCD component

	void nodes(int n); 	// draw number of nodes on LCD
    void drawCharSet(); // draw current char set on LCD

  private:
	DHS_1Tick_Control *Controller; // command controller pointer
    DHS_Config 		  *Config;	   // config storage pointer

    int8_t numNodes = 0;    // number of connected nodes
    int layer 		= -1;   // layer of current node in mesh network
    int8_t iClients = 0;    // internet client connections
    int8_t touchActive = 0; // touch active counter
    int8_t outMode = 0;     // output mode on lcd
    uint8_t comMode = 0;    // command mode indicator

	int16_t xpos=0;	// current x char pos on display
	int16_t ypos=0; // current y char pos on display

    char charSetChars[3][31]={"abcdefghijklmnopqrstuvwxyz\x84\x94\x81*","1234567890+-:=&#/()<>,.?{}$@%*","ABCDEFGHIJKLMNOPQRSTUVWXYZ\x8E\x99\x9A*"};
    uint8_t charSet= 0; // currently active charset

   	void initGross();
   	void initKlein();
};

#endif /* SRC_DB_LCD_H_ */
