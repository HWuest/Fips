/*
 * DHS_LCD.h
 *
 *  Created on: 20.05.2023
 *      Author: HW
 */

#ifndef SRC_DHS_LCD_H_
#define SRC_DHS_LCD_H_

#include "st7789.h"
#include "bmpfile.h"
#include "pngle.h"
#include "esp_lcd_touch_xpt2046.h"

#define TOUCH_CS_PIN 11
#define CONFIG_LCD_HRES 240
#define CONFIG_LCD_VRES 320

/*
 * esp_lcd_touch_get_coordinates:
 * x-min: 240  x-max: 3800  y-min: 160  y-max: 3700
 *
 */
void png_init(pngle_t *pngle, uint32_t w, uint32_t h);
void png_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]);
void png_finish(pngle_t *pngle);

class DHS_LCD {
  public:
	static uint8_t lcdActive;

	TFT_t dev;
	esp_lcd_touch_handle_t tp = 0;

	FontxFile fx16G[2];
	FontxFile fx24G[2];
	FontxFile fx32G[2];
	FontxFile fx32L[2];

	void init(); // initialize LCD IOs/SPI

	void lcd_OnOff(bool s) { if((lcdActive) && (dev._bl >= 0)) gpio_set_level( (gpio_num_t) dev._bl, s ); };

	uint8_t BMP(TFT_t * dev, char * file, int width, int height);
	uint8_t PNG(TFT_t * dev, char * file, int width, int height);
};

#endif /* SRC_DHS_LCD_H_ */
