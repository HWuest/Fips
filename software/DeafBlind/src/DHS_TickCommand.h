/*
 * DHSTickCommand.h
 *
 *  Created on: 15.05.2023
 *      Author: HW
 */

#ifndef DHS_TICKCOMMAND_H_
#define DHS_TICKCOMMAND_H_

#include "DHS_1Tick_Control.h"
#include "DHS_Config.h"

#include <string>
#include "esp_wifi.h"

#define DHS_COMMANDCHAR '*' // command character
#define DHS_COMMNUM 2		// number of comm char repetitions to sctivate command
#define DHS_CHARSETCHAR '#' // charset change character

class DHS_TickCommand {
  public:
	static int outSpeed; // o command character output speed
    std::string tickLine = ""; // memorized last text line
	DHS_1Tick_Control *Controller; // 1Tick_Controller pointer for callback
    int outHandlerId;
	DHS_Config *Config;
	uint8_t trainMode = 0; // train mode active?

	DHS_TickCommand(DHS_1Tick_Control *Cont, DHS_Config *Conf);

	void process(char c); // process command char
	void processLine(std::string l); // process line command

  private:
	wifi_config_t cfg;
	uint8_t comMode = 0; //comand Mode active?
	uint8_t charsetMode = 0; //charste char counter
    std::string tickString = ""; // current text line
};

void DHSLineOutTask(void *arg);

#endif /* DHS_TICKCOMMAND_H_ */
