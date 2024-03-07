/*
 * DB_LCD.cpp
 *
 *  Created on: 02.06.2023
 *      Author: HW
 *
 */

#include "DB_LCD.h"

DB_LCD::DB_LCD(DHS_1Tick_Control *Cont, DHS_Config *Conf) {

	Controller = Cont; // set one tick controller pointer
	Config = Conf;     // set configuration storage pointer

	DHS_LCD::lcdActive = Config->read((char*)"lcd", (uint8_t) 0);           // get config value lcd
	DHS_LCDklein::lcdActive = Config->read((char*)"lcdklein", (uint8_t) 0); // get config value lcdklein

	ESP_LOGI("DHS","LCD Gross: %d LCD Klein %d",DHS_LCD::lcdActive, DHS_LCDklein::lcdActive);

    if (DHS_LCD::lcdActive) initGross();           // init big lcd if configured
    else if (DHS_LCDklein::lcdActive) initKlein(); // init small lcd if configured
}

void DB_LCD::initGross() {
    DHS_LCD::init(); // initialize IOs, fonts and SPI

	PNG(&dev, (char*)"/spiflash/Background.png", CONFIG_WIDTH, CONFIG_HEIGHT); // draw background image Background.png from flash storage

  	uint8_t buffer[FontxGlyphBufSize]; // charset buffer
  	uint8_t fontWidth;
  	uint8_t fontHeight;
  	GetFontx(fx24G, 0, buffer, &fontWidth, &fontHeight); // get font fx24G for usage
  	GetFontx(fx32L, 0, buffer, &fontWidth, &fontHeight); // get font fx32L for usage

  	lcdDrawString(&dev, fx24G, 240-fx24G->h, 54, (char*)DHS_HEADLINE, RED); // draw header

  	// draw char input box lines
  	lcdDrawLine(&dev,2*fx32L->h,0,2*fx32L->h,320,BLUE);
  	lcdDrawLine(&dev,fx32L->h,0,fx32L->h,320,BLUE);
  	for (int i=0; i<15; i++) {
  		lcdDrawChar(&dev, fx32L, fx32L->h, 21*i+4 ,  charSetChars[charSet][i], BLACK);
  		lcdDrawLine(&dev,2*fx32L->h,i*21,0,i*21,BLUE);
  	}

  	// draw charset
  	for (int i=0; i<15; i++) lcdDrawChar(&dev, fx32L, 2, 21*i+4 ,  charSetChars[charSet][i+15], BLACK);
  	lcdSetFontFill(&dev, WHITE); // standard font background white

  	// char output event handler to be called by controller
  	DHS::event_handler <char> LCDOutHandler([this](char c)  {
  		if (c == 255) return; // no character given
  		if (c == '\n') { // process newline char
  			xpos=0;
  			ypos+=fx24G->h; // next line
  			return;
  		}
  		if (c == '\b') { // process backspace char
  		  int num= 3;
  		  if (outMode==0) num=1; // number of chars to delete is depending on output mode

  		  for (int i=0;i < num; i++) { // remove chars incl. *- command
  			xpos -= fx24G->w; // move char position one back
  			if (xpos<0) { // previous line, than one line up
  				xpos=25*fx24G->w;
  				ypos-=fx24G->h; // previous line
  				if (ypos < 0) ypos = 0; // start of display reached?
  			}
  			lcdDrawChar(&dev, fx24G, 240-2*fx24G->h-ypos-2, xpos+3, ' ', BLACK); // print space to lcd to remove last char
  		  }
  		  return;
  		}
  		if (outMode==0) { // no command char drawing mode?
  			if (c =='#') return; // dont draw special command char
  			if (c =='*') { comMode++; return; } // increase command mode on command char
  			if (comMode == 2) { // don't draw special command chars
  				comMode = 0;	// no second comand char, continue
  				return;
  			}
  			else comMode = 0;
  		}

  		if (ypos>5*fx24G->h) { // clear display after 6 lines
  			lcdDrawFillRect(&dev, 66,3, 240-25, 320-5, WHITE); // clear screen DHS to be optimsed to screen scroling
  			ypos=0;
  		}

  		lcdDrawChar(&dev, fx24G, 240-2*fx24G->h-ypos-2, xpos+3, c, BLACK); // print char to lcd
  		xpos+=fx24G->w; // next x-char position
  		if (xpos > 320-fx24G->w) { // end of line reached?
  			xpos=0; // back to start
  			ypos+=fx24G->h; // next line
  		}
  	});

  	LCDOutHandler.setType(DHS_LocalHandler);  // LCD is a local handler
  	Controller->outHandlers += LCDOutHandler; // register handler

  	// LCD char input event handler to be called continously by controller
  	DHS::event_handler <char*> LCDInHandler([this](char *c)  {
  		uint16_t x;
  	    uint16_t y;
  	    uint16_t strength;
  	    uint8_t count = 0;

// DHS to be updated to mesh_lite
//   	    wifi_sta_list_t wifi_sta_list;
//	    esp_wifi_ap_get_sta_list(&wifi_sta_list);
//  	    int m = wifi_sta_list.num; // check for changed numNodes
//      	if (m!=numNodes) {
//          	numNodes = m;
//      		nodes(numNodes); // show number of nodes in LCD
//      	}

      	int l = esp_mesh_lite_get_level();
      	if (l != layer) {
      		layer = l;
      	    char s[3];
      		lcdSetFontFill(&dev, 0xC61f);
      		sprintf(s,"%2d",l); // layer
      		lcdDrawString(&dev, fx24G, 240-fx24G->h, 2, s, WHITE);
      		lcdSetFontFill(&dev, WHITE);
      	}

      	if (Controller->iClients != iClients) { // check for changed iClients
      		iClients = Controller->iClients;
      	    char s[3];
      		lcdSetFontFill(&dev, 0xC61f);
      		sprintf(s,"%2d",iClients); // layer
      		lcdDrawString(&dev, fx24G, 240-fx24G->h, 30, s, WHITE);
      		lcdSetFontFill(&dev, WHITE);
      	}

      	if (Controller->charSet != charSet) { // check for changed charset
      		charSet= Controller->charSet;
      		drawCharSet();
      	}

  	    *c = 255; // start with no char

  	    // Update touch point data after touchActive time
  	    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_touch_read_data(tp));
  	    if (esp_lcd_touch_get_coordinates(tp, &x, &y, &strength, &count, 1) && (touchActive==0)) {
  	    	int8_t a = (x-240)/445-6; // calc x position (calibration factor 445 and header subtract -6 used)
  	    	int8_t b = (y-160)/233;   // calc y position (calibration factor 233 used)

  	    	// switch outMode on touch in upper lcd region
  	    	if (a < -5) { // above first char line?
  	    		outMode = !outMode;
  	    		lcdUnsetFontFill(&dev);
  	    	    if (outMode) lcdDrawString(&dev, fx24G, 240-fx24G->h, 54, (char*)DHS_HEADLINE, BLACK);
  	    	    else   	     lcdDrawString(&dev, fx24G, 240-fx24G->h, 54, (char*)DHS_HEADLINE, RED);
  	      		lcdSetFontFill(&dev, WHITE);
  	    		touchActive = 30; // reset touch timer
  	    		ESP_LOGI("DHS","OutMode %d",outMode);
  	    		return;
  	    	}
  	    	// calc char pos
  	    	a = a*15+b+1;

  	    	if ((a >0) && (a<31) ) { // character selected?
  	    		*c = charSetChars[Controller->charSet][a-1]; // take character
  	    		touchActive = 30; // reset touch timer
  	    	}
  	    }
          else if (touchActive) touchActive--; // decrease touch timer on every call
  	});

  	Controller->inHandlers += LCDInHandler; // register LCD in handler

	lcd_OnOff( true ); // switch LCD on
}

void DB_LCD::initKlein() {
    DHS_LCDklein::init(); // initialize IOs and SPI

    ssd1306_draw_string(ssd1306_dev, 32, 0, (uint8_t*) &DHS_HEADLINE[10], 16, 1); // draw header
    ssd1306_refresh_gram(ssd1306_dev);

  	DHS::event_handler <char> LCDOutHandler([this](char c)  { // LCD char output handler function
  		if (c == 255) return; // no character given
  		if (c == '\n') { // process newline character
  			xpos=0;
  			ypos+=16; // next line
  			return;
  		}
  		if (c == '\b') { // process backspace character
    	  int num= 3;
    	  if (outMode == 0) num=1; // get num chars depending on mode
  		  for (int i = 0; i < num; i++) { // remove chars incl. *- command
  			xpos -= 8;
  			if (xpos < 0) { // beginning of line reached
  				xpos = 128-8;
  				ypos -= 16; // previous line
  				if (ypos < 0) ypos = 0; // top of screen?
  			}
  			ssd1306_draw_char(ssd1306_dev, xpos, ypos+16, ' ', 16, 1); // print space to lcd to remove last char
  		  }
		  ssd1306_refresh_gram(ssd1306_dev);
  		  return;
  		}

  		if (outMode==0) { // no command char drawing mode?
  			if (c =='#') return; // dont draw special command char
  			if (c =='*') { comMode++; return; } // increase command mode on command char
  			if (comMode == 2) { // don't draw special command chars
  				comMode = 0;
  				return;
  			}
  			else comMode = 0; // no second comand char, continue   DHS to be extended to output single command char!!!
  		}

  		if (ypos > 2*16) { // clear after 3 lines
  			ssd1306_fill_rectangle(ssd1306_dev, 0, 16, 128, 64, 0); // clear screen DHS to be optimized to screen scrolling
  			ypos=0;
  		}

		ssd1306_draw_char(ssd1306_dev, xpos, ypos+16, c, 16, 1); // print char
		ssd1306_refresh_gram(ssd1306_dev);

	    xpos+=8; // next x-char position
  		if (xpos > 128-8) { // end of line reached?
  			xpos = 0; // back to start
  			ypos += 16; // next line
  		}
  	});

  	LCDOutHandler.setType(DHS_LocalHandler); // LCD is local handler
  	Controller->outHandlers += LCDOutHandler; // register LCD out handler

  	DHS::event_handler <char*> LCDInHandler([this](char *c)  { // LCD char input handler function (used for regular LCD update)
  		char s[4];

  	    *c = 255; // no char

// DHS to be updated to mesh_lite
//  	    wifi_sta_list_t wifi_sta_list;
//	    esp_wifi_ap_get_sta_list(&wifi_sta_list);
//  	    int m = wifi_sta_list.num; // check for changed numNodes
//  	    if (m!=numNodes) {
//          	numNodes = m;
//            sprintf(s,"%3d",m-1); // nodes without self
//      		ssd1306_draw_string(ssd1306_dev, 104, 0, (uint8_t*)s, 16,1);
//    	    ssd1306_refresh_gram(ssd1306_dev);
//        }

  	    // draw device level in mesh network (1 root, 2... parents
      	int l = esp_mesh_lite_get_level();
      	if (l != layer) {
      		layer = l;
      		sprintf(s,"%2d",l); // layer
      		ssd1306_draw_string(ssd1306_dev, 0, 0, (uint8_t*)s, 16,1);
    	    ssd1306_refresh_gram(ssd1306_dev);
      	}

// DHS to be updated to mesh_lite
//      	if (Controller->iClients != iClients) { // check for changed iClients
//      		iClients = Controller->iClients;
//      		sprintf(s,"%2d",iClients); // layer
//      		ssd1306_draw_string(ssd1306_dev, 32, 0, (uint8_t*)s, 16,1);
//    	    ssd1306_refresh_gram(ssd1306_dev);
//      	}
  	});

  	Controller->inHandlers += LCDInHandler; // register LCD in handler
}

void DB_LCD::drawCharSet() { // put current charSet to input area
	if (!DHS_LCD::lcdActive) return;
	lcdSetFontFill(&dev, 0xC61f);
	for (int i=0; i<15; i++) {
		lcdDrawChar(&dev, fx32L, fx32L->h, 21*i+4 ,  charSetChars[charSet][i], BLACK);
	}
	for (int i=0; i<15; i++) lcdDrawChar(&dev, fx32L, 2, 21*i+4 ,  charSetChars[charSet][i+15], BLACK);
	lcdSetFontFill(&dev, WHITE);
}
void DB_LCD::nodes(int n) { // put active nodes in header
	if (!DHS_LCD::lcdActive) return;
    char s[4];
	lcdSetFontFill(&dev, 0xC61f);
    sprintf(s,"%3d",n); // nodes without self
	lcdDrawString(&dev, fx24G, 240-fx24G->h, 278, s, WHITE);
	lcdSetFontFill(&dev, WHITE);
}

