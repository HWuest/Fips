/*
 * DHS_Console.hpp
 *
 *  Created on: 10.05.2023
 *      Author: HW
 */

#ifndef DHS_CONSOLE_H_
#define DHS_CONSOLE_H_

#include <unistd.h>

#include "DHS_1Tick_Control.h"

class DHS_Console {
  public:
    int fd_id;

    DHS_Console(DHS_1Tick_Control *Cont); // constructor
    ~DHS_Console(); // destructor

  private:
    int inHandlerId;
	int outHandlerId;
    DHS_1Tick_Control *Controller; // 1Tick_Controller pointer for callback
};

#endif /* DHS_CONSOLE_H_ */
