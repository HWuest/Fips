/*
 * DHS_Train_Control.h
 *
 *  Created on: 15.08.2023
 *      Author: HW
 */

#ifndef SRC_DHS_TRAIN_CONTROL_H_
#define SRC_DHS_TRAIN_CONTROL_H_

#include "DHS_TickCommand.h"

class DHS_Train_Control {
public:
    DHS_TickCommand* Com; // pointer to 1Tick_Controller

	DHS_Train_Control(DHS_TickCommand* command);
	void processHandlers();

private:
	uint8_t state, train_num;
	char train_set[28] = "abcdefghijklmnopqrstuvwxyz";
};

#endif /* SRC_DHS_TRAIN_CONTROL_H_ */
