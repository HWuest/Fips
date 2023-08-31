/*
 * DHS_LCD.cpp
 *
 *  Created on: 20.05.2023
 *      Author: HW
 */

#include "../src/DHS_LCD.h"

#include "fontx.h"
#include "esp_log.h"

#include <cstring>

uint8_t DHS_LCD::lcdActive = 0;

void DHS_LCD::init() {

	if (!lcdActive) return;

	// init SPI
	spi_bus_config_t buscfg = {
		.mosi_io_num = CONFIG_MOSI_GPIO,
		.miso_io_num = 12,
		.sclk_io_num = CONFIG_SCLK_GPIO,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 0,
		.flags = 0
	};

	esp_err_t ret = spi_bus_initialize( SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO );
	assert(ret==ESP_OK);

	// init touch

	esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_spi_config_t tp_io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(TOUCH_CS_PIN);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = CONFIG_LCD_HRES,
        .y_max = CONFIG_LCD_VRES,
        .rst_gpio_num = (gpio_num_t) -1,
        .int_gpio_num = (gpio_num_t) -1,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    ESP_LOGI("DHS", "Initialize touch controller XPT2046");
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_touch_new_spi_xpt2046(tp_io_handle, &tp_cfg, &tp));

    // init LCD

	// set font file
	InitFontx(fx16G,"/spiflash/ILGH16XB.FNT",""); // 8x16Dot Gothic
	InitFontx(fx24G,"/spiflash/ILGH24XB.FNT",""); // 12x24Dot Gothic
	InitFontx(fx32G,"/spiflash/ILGH32XB.FNT",""); // 16x32Dot Gothic
	InitFontx(fx32L,"/spiflash/LATIN32B.FNT",""); // 16x32Dot Latin

	spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO, CONFIG_BL_GPIO);

	if(dev._bl >= 0) {
		gpio_reset_pin( (gpio_num_t) dev._bl);
		gpio_set_level( (gpio_num_t) dev._bl, 0 );
		gpio_set_direction( (gpio_num_t) dev._bl, GPIO_MODE_OUTPUT);
	}

	lcdInit(&dev, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);
}

uint8_t DHS_LCD::BMP(TFT_t * dev, char * file, int width, int height) {
	if (!lcdActive) return 0;

	// open requested file
	esp_err_t ret;
	FILE* fp = fopen(file, "rb");
	if (fp == NULL) {
		ESP_LOGI(__FUNCTION__, "File not found [%s]", file);
		return 0;
	}

	// read bmp header
	bmpfile_t *result = (bmpfile_t*)malloc(sizeof(bmpfile_t));
	ret = fread(result->header.magic, 1, 2, fp);
	assert(ret == 2);
	if (result->header.magic[0]!='B' || result->header.magic[1] != 'M') {
		ESP_LOGW(__FUNCTION__, "File is not BMP");
		free(result);
		fclose(fp);
		return 0;
	}
	ret = fread(&result->header.filesz, 4, 1 , fp);
	assert(ret == 1);
	ESP_LOGD(__FUNCTION__,"result->header.filesz=%"PRIu32, result->header.filesz);
	ret = fread(&result->header.creator1, 2, 1, fp);
	assert(ret == 1);
	ret = fread(&result->header.creator2, 2, 1, fp);
	assert(ret == 1);
	ret = fread(&result->header.offset, 4, 1, fp);
	assert(ret == 1);
	// read dib header
	ret = fread(&result->dib.header_sz, 4, 1, fp);
	assert(ret == 1);
	ret = fread(&result->dib.width, 4, 1, fp);
	assert(ret == 1);
	ret = fread(&result->dib.height, 4, 1, fp);
	assert(ret == 1);
	ret = fread(&result->dib.nplanes, 2, 1, fp);
	assert(ret == 1);
	ret = fread(&result->dib.depth, 2, 1, fp);
	assert(ret == 1);
	ret = fread(&result->dib.compress_type, 4, 1, fp);
	assert(ret == 1);
	ret = fread(&result->dib.bmp_bytesz, 4, 1, fp);
	assert(ret == 1);
	ret = fread(&result->dib.hres, 4, 1, fp);
	assert(ret == 1);
	ret = fread(&result->dib.vres, 4, 1, fp);
	assert(ret == 1);
	ret = fread(&result->dib.ncolors, 4, 1, fp);
	assert(ret == 1);
	ret = fread(&result->dib.nimpcolors, 4, 1, fp);
	assert(ret == 1);

	if((result->dib.depth == 24) && (result->dib.compress_type == 0)) {
		// BMP rows are padded (if needed) to 4-byte boundary
		uint32_t rowSize = (result->dib.width * 3 + 3) & ~3;
		int w = result->dib.width;
		int h = result->dib.height;
		ESP_LOGI(__FUNCTION__,"w=%d h=%d", w, h);
		int _x;
		int _w;
		int _cols;
		int _cole;
		if (width >= w) {
			_x = (width - w) / 2;
			_w = w;
			_cols = 0;
			_cole = w - 1;
		} else {
			_x = 0;
			_w = width;
			_cols = (w - width) / 2;
			_cole = _cols + width - 1;
		}
		ESP_LOGD(__FUNCTION__,"_x=%d _w=%d _cols=%d _cole=%d",_x, _w, _cols, _cole);

		int _y;
		int _rows;
		int _rowe;
		if (height >= h) {
			_y = (height - h) / 2;
			_rows = 0;
			_rowe = h -1;
		} else {
			_y = 0;
			_rows = (h - height) / 2;
			_rowe = _rows + height - 1;
		}
		ESP_LOGD(__FUNCTION__,"_y=%d _rows=%d _rowe=%d", _y, _rows, _rowe);

#define BUFFPIXEL 20
		uint8_t sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
		uint16_t *colors = (uint16_t*)malloc(sizeof(uint16_t) * w);

		for (int row=0; row<h; row++) { // For each scanline...
			if (row < _rows || row > _rowe) continue;
			// Seek to start of scan line.	It might seem labor-
			// intensive to be doing this on every line, but this
			// method covers a lot of gritty details like cropping
			// and scanline padding.  Also, the seek only takes
			// place if the file position actually needs to change
			// (avoids a lot of cluster math in SD library).
			// Bitmap is stored bottom-to-top order (normal BMP)
			int pos = result->header.offset + (h - 1 - row) * rowSize;
			fseek(fp, pos, SEEK_SET);
			int buffidx = sizeof(sdbuffer); // Force buffer reload

			int index = 0;
			for (int col=0; col<w; col++) { // For each pixel...
				if (buffidx >= sizeof(sdbuffer)) { // Indeed
					fread(sdbuffer, sizeof(sdbuffer), 1, fp);
					buffidx = 0; // Set index to beginning
				}
				if (col < _cols || col > _cole) continue;
				// Convert pixel from BMP to TFT format, push to display
				uint8_t b = sdbuffer[buffidx++];
				uint8_t g = sdbuffer[buffidx++];
				uint8_t r = sdbuffer[buffidx++];
				colors[index++] = rgb565_conv(r, g, b);
			} // end for col
			ESP_LOGD(__FUNCTION__,"lcdDrawMultiPixels _x=%d _y=%d row=%d",_x, _y, row);
			//lcdDrawMultiPixels(dev, _x, row+_y, _w, colors);
			lcdDrawMultiPixels(dev, _x, _y, _w, colors);
			_y++;
		} // end for row
		free(colors);
	} // end if
	free(result);
	fclose(fp);
	return 1;
}

uint8_t DHS_LCD::PNG(TFT_t * dev, char * file, int width, int height) {
	if (!lcdActive) return 0;

	// open PNG file
	FILE* fp = fopen(file, "rb");
	if (fp == NULL) {
		ESP_LOGW(__FUNCTION__, "File not found [%s]", file);
		return 0;
	}
	char buf[1024];
	size_t remain = 0;
	int len;

	pngle_t *pngle = pngle_new(width, height);
    if (pngle == NULL) return 0;

	pngle_set_init_callback(pngle, png_init);
	pngle_set_draw_callback(pngle, png_draw);
	pngle_set_done_callback(pngle, png_finish);

	double display_gamma = 2.2;
	pngle_set_display_gamma(pngle, display_gamma);

	while (!feof(fp)) {
		if (remain >= sizeof(buf)) {
			ESP_LOGE(__FUNCTION__, "Buffer exceeded");
			return 0;
		}

		len = fread(buf + remain, 1, sizeof(buf) - remain, fp);
		if (len <= 0) {
			//printf("EOF\n");
			break;
		}

		int fed = pngle_feed(pngle, buf, remain + len);
		if (fed < 0) {
			ESP_LOGE(__FUNCTION__, "ERROR; %s", pngle_error(pngle));
			return 0;
		}

		remain = remain + len - fed;
		if (remain > 0) memmove(buf, buf + fed, remain);
	}

	fclose(fp);

	uint16_t _width = width;
	uint16_t _cols = 0;
	if (width > pngle->imageWidth) {
		_width = pngle->imageWidth;
		_cols = (width - pngle->imageWidth) / 2;
	}
	ESP_LOGD(__FUNCTION__, "_width=%d _cols=%d", _width, _cols);

	uint16_t _height = height;
	uint16_t _rows = 0;
	if (height > pngle->imageHeight) {
			_height = pngle->imageHeight;
			_rows = (height - pngle->imageHeight) / 2;
	}
	ESP_LOGD(__FUNCTION__, "_height=%d _rows=%d", _height, _rows);
	uint16_t *colors = (uint16_t*)malloc(sizeof(uint16_t) * _width);

#if 0
	for(int y = 0; y < _height; y++){
		for(int x = 0;x < _width; x++){
			pixel_png pixel = pngle->pixels[y][x];
			uint16_t color = rgb565_conv(pixel.red, pixel.green, pixel.blue);
			lcdDrawPixel(dev, x+_cols, y+_rows, color);
		}
	}
#endif

	for(int y = 0; y < _height; y++){
		for(int x = 0;x < _width; x++){
			//pixel_png pixel = pngle->pixels[y][x];
			//colors[x] = rgb565_conv(pixel.red, pixel.green, pixel.blue);
			colors[x] = pngle->pixels[y][x];
		}
		lcdDrawMultiPixels(dev, _cols, y+_rows, _width, colors);
		vTaskDelay(0);
}
	free(colors);
	pngle_destroy(pngle, width, height);

	return 1;
}

void png_init(pngle_t *pngle, uint32_t w, uint32_t h)
{
	ESP_LOGD(__FUNCTION__, "png_init w=%"PRIu32" h=%"PRIu32, w, h);
	ESP_LOGD(__FUNCTION__, "screenWidth=%d screenHeight=%d", pngle->screenWidth, pngle->screenHeight);
	pngle->imageWidth = w;
	pngle->imageHeight = h;
	pngle->reduction = false;
	pngle->scale_factor = 1.0;

	// Calculate Reduction
	if (pngle->screenWidth < pngle->imageWidth || pngle->screenHeight < pngle->imageHeight) {
		pngle->reduction = true;
		double factorWidth = (double)pngle->screenWidth / (double)pngle->imageWidth;
		double factorHeight = (double)pngle->screenHeight / (double)pngle->imageHeight;
		pngle->scale_factor = factorWidth;
		if (factorHeight < factorWidth) pngle->scale_factor = factorHeight;
		pngle->imageWidth = pngle->imageWidth * pngle->scale_factor;
		pngle->imageHeight = pngle->imageHeight * pngle->scale_factor;
	}
	ESP_LOGI(__FUNCTION__, "reduction=%d scale_factor=%f", pngle->reduction, pngle->scale_factor);
	ESP_LOGI(__FUNCTION__, "imageWidth=%d imageHeight=%d", pngle->imageWidth, pngle->imageHeight);

}

#define rgb565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

void png_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
{
	ESP_LOGD(__FUNCTION__, "png_draw x=%"PRIu32" y=%"PRIu32" w=%"PRIu32" h=%"PRIu32, x,y,w,h);
#if 0
	uint8_t r = rgba[0];
	uint8_t g = rgba[1];
	uint8_t b = rgba[2];
#endif

	// image reduction
	uint32_t _x = x;
	uint32_t _y = y;
	if (pngle->reduction) {
		_x = x * pngle->scale_factor;
		_y = y * pngle->scale_factor;
	}
	if (_y < pngle->screenHeight && _x < pngle->screenWidth) {
#if 0
		pngle->pixels[_y][_x].red = rgba[0];
		pngle->pixels[_y][_x].green = rgba[1];
		pngle->pixels[_y][_x].blue = rgba[2];
#endif
		pngle->pixels[_y][_x] = rgb565(rgba[0], rgba[1], rgba[2]);
	}

}

void png_finish(pngle_t *pngle) {
	ESP_LOGI(__FUNCTION__, "png_finish");
}

