/*
 * DHS_LCDklein.cpp
 *
 *  Created on: 02.06.2023
 *      Author: HW
 */

#include "DHS_LCDklein.h"

uint8_t DHS_LCDklein::lcdActive = 0;
ssd1306_handle_t DHS_LCDklein::ssd1306_dev = NULL;

void DHS_LCDklein::init() {

	if (!lcdActive) return;

    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    ssd1306_dev = ssd1306_create(I2C_MASTER_NUM, SSD1306_I2C_ADDRESS);
    ssd1306_clear_screen(ssd1306_dev, 0x00);
    ssd1306_refresh_gram(ssd1306_dev);
}
