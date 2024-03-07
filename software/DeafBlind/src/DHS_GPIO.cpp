/*
 * DHS_GPIO.cpp
 *
 *  Created on: 10.05.2023
 *      Author: HW
 */

/*
 * DHS_ConsoleInput.cpp
 *
 *  Created on: 10.05.2023
 *      Author: HW
 */

#include "DHS_GPIO.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>

#include "DHS_1Tick_Control.h"

TaskHandle_t GPIOoffTaskHandle = NULL;

int64_t DHS_GPIO::stabTime;
int DHS_GPIO::pulseDuration;
int DHS_GPIO::pulseSequence;

uint8_t  DHS_GPIO::IN[5] = {6, 7, 8, 9, 10};   // in GPIO Ports
uint8_t  DHS_GPIO::OUT[5] ={1, 2, 3, 4, 5};
uint8_t DHS_GPIO::previousinput;
uint8_t DHS_GPIO::tickMode;

void GPIOoffTask(void *arg)
{
	vTaskDelay(((DHS_GPIO*)arg)->pulseDuration / portTICK_PERIOD_MS); // wait pulseDuration time

	for (int i=0; i<5; i++) gpio_set_level((gpio_num_t) ((DHS_GPIO*)arg)->OUT[i], 0);  // actuators off
	gpio_set_level((gpio_num_t)CONFIG_BLINK_GPIO, 0); // set LED back to initial state
	vTaskDelete(NULL);
}

void DHS_GPIO::initIOs() {
    for (uint8_t i=0; i<5; i++) {
    	gpio_reset_pin( (gpio_num_t)OUT[i]);
    	gpio_reset_pin( (gpio_num_t)IN[i]);
        gpio_set_direction( (gpio_num_t)OUT[i], GPIO_MODE_OUTPUT);
        gpio_set_direction( (gpio_num_t)IN[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode( (gpio_num_t)IN[i], GPIO_PULLUP_ONLY);
    }
	previousinput = getIOs(); // get start configuration from input device
}

DHS_GPIO::DHS_GPIO(DHS_1Tick_Control *Cont, DHS_Config *Conf) {
    uint8_t i;

    /* Configure the peripheral outputs */
    gpio_reset_pin( (gpio_num_t)CONFIG_BLINK_GPIO);
    gpio_set_direction( (gpio_num_t)CONFIG_BLINK_GPIO, GPIO_MODE_OUTPUT);

    for (i=0; i<5; i++) {
    	gpio_reset_pin( (gpio_num_t)OUT[i]);
    	gpio_reset_pin( (gpio_num_t)IN[i]);
        gpio_set_direction( (gpio_num_t)OUT[i], GPIO_MODE_OUTPUT);
        gpio_set_direction( (gpio_num_t)IN[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode( (gpio_num_t)IN[i], GPIO_PULLUP_ONLY);
    }

	Controller = Cont; // set one tick controller
	Config = Conf; // set configuration storag

    stabTime = Config->read( (char*)"stabTime", (int64_t) 70*1000);
    pulseDuration = Config->read( (char*)"pulseDuration", 100);
    pulseSequence = Config->read( (char*)"pulseSequence", 50);
    tickMode = Config->read( (char*)"tickMode",(uint8_t)0);
	previousinput = getIOs(); // get start configuration from input device
    tickTime = 0; // no tick

	DHS::event_handler <char> GPIOoutHandler([this](char c)  { // GPIO char output handler function
		if (c == 255) return; // no character given

		uint8_t u = Controller->charToTick(c); // get tick from charcter

		if (u==255) return; // no valid char

		gpio_set_level((gpio_num_t)CONFIG_BLINK_GPIO, 1);  // swith LED

		for (uint8_t i=0; i<5; i++) {
			gpio_set_level((gpio_num_t) OUT[i], u & (1<<i));  // set tick actuators
			if ((u & (1<<i)) && (pulseSequence > 0)) vTaskDelay(pulseSequence / portTICK_PERIOD_MS); // wait pulseSequence time for seriell activation of ticks
		}

		xTaskCreate(GPIOoffTask, "GPIOoffTask", 1000, this, 2, &GPIOoffTaskHandle); // could be optimized with xTaskGenericNotify(...) / xTaskNotifyWaitIndexed(...)
	});


	DHS::event_handler <char*> GPIOinHandler([this](char* c)  { // GPIO char input handler function
        *c = 255; // set initially no character
        int64_t currentTime = esp_timer_get_time();
        uint8_t input = getIOs(); // get new status from input device

        if (input == previousinput) { // no input change
        	if ((tickTime >0) && (currentTime - tickTime > stabTime)) { // stabilization running and stab time over
    			  if (input > 0) *c = Controller->tickToChar(input); // return tick character from current char set
    			  tickTime = 0; // stabilization time off
        	}
        	return;
        }
        if (tickMode & 2)  tickTime = currentTime; // if tickMode bit 1, reset stabilization on every change, old version (slower)
        if ((tickMode & 1) && (input < previousinput)) { previousinput = input; return; } // if tickMode bit 0 and only fingers removed, don't react
		if (input == 0) { previousinput = 0; return; } // all fingers up, don't react

		if (tickTime == 0) tickTime = currentTime; // stabilization not running, start stabilization
		previousinput = input; // update input memory
	});

	GPIOinHandler.setType(DHS_LocalHandler);
	GPIOoutHandler.setType(DHS_LocalHandler); // local handler

	inHandlerId = Controller->inHandlers += GPIOinHandler;
    outHandlerId = Controller->outHandlers += GPIOoutHandler;
}

DHS_GPIO::~DHS_GPIO() { 
	Controller->inHandlers.remove_id( inHandlerId);
	Controller->outHandlers.remove_id(outHandlerId);
}

uint8_t DHS_GPIO::getIOs() {  // get ticks from IO-Device
      uint8_t input=0,i;

      for (i=0; i<5; i++) input += ( ! gpio_get_level( (gpio_num_t)IN[i])) << i; 	  // get inputs

      return input;
}

void DHS_GPIO::setIO(uint8_t io, uint8_t x) // set actuator io
{
	gpio_set_level((gpio_num_t) OUT[io],x);
}
