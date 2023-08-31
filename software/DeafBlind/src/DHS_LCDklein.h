/*
 * DHS_LCDklein.h
 *
 *  Created on: 02.06.2023
 *      Author: HW
 */

#ifndef SRC_DHS_LCDKLEIN_H_
#define SRC_DHS_LCDKLEIN_H_

#include "ssd1306.h"

#define I2C_MASTER_SCL_IO 35        /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 33        /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_1    /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 1000000  /*!< I2C master clock frequency 1MHz max.*/

class DHS_LCDklein {
public:
	static uint8_t lcdActive;
	static ssd1306_handle_t ssd1306_dev;

	void init();
};

#endif /* SRC_DHS_LCDKLEIN_H_ */
