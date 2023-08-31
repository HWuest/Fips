/*
 * DHS_GPIO.hpp
 *
 *  Created on: 10.05.2023
 *      Author: HW
 */

#ifndef DHS_GPIO_H_
#define DHS_GPIO_H_

#include <unistd.h>

#include "DHS_1Tick_Control.h"
#include "DHS_Config.h"
#include "driver/gpio.h"

class DHS_GPIO {
  public:
    static uint8_t  IN[5];   // in GPIO Ports
    static uint8_t  OUT[5];    // out GPIO Ports
    static int64_t stabTime;     	     // tick input stabilization time in µs
    static int pulseDuration; // duration of output tick pulses
    static uint8_t previousinput; // previous input state
    static uint8_t tickMode; // select tickMode Bit0 up/down or down only delay, Bit1 time at start or time at every change (slower)

    int inHandlerId;
    int outHandlerId;

	DHS_GPIO(DHS_1Tick_Control *Cont, DHS_Config *Conf); // constructor
    ~DHS_GPIO(); // destructor

    void ledOn()  { gpio_set_level((gpio_num_t)CONFIG_BLINK_GPIO, 1); };
    void ledOff() { gpio_set_level((gpio_num_t)CONFIG_BLINK_GPIO, 0); };
    void static initIOs();
    static uint8_t getIOs();   // get value of all IOs
    void setIO(uint8_t io, uint8_t x); // set actuator io to x

  private:

    DHS_1Tick_Control *Controller; // 1Tick_Controller pointer for callback
    DHS_Config *Config;			// Config storage pointer
    int64_t tickTime; // time for tick input stabelization

	// DHS::event_handler <char> GPIOoutHandler([this](char c)  --> Out Eventhandler defined as Lambda-Function in Constructor
    // DHS::event_handler <char*> GPIOinHandler([this](char* c) --> In  Eventhandler defined as Lambda-Function in Constructor
};

#endif /* DHS_GPIO_H_ */
