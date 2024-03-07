/*
 * DHS_Train_Control.cpp
 *
 *  Created on: 15.08.2023
 *      Author: HW
 */

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "DHS_Train_Control.h"
#include "DHS_TickCommand.h"

extern SemaphoreHandle_t xMutex;

DHS_Train_Control::DHS_Train_Control(DHS_TickCommand* command) {
	Com = command;
	state = 0;
	train_num = 0;
}

void DHS_Train_Control::processHandlers() {
  char c;

  switch (state) {
  	  case 0: // send start sequence
  		  	  xSemaphoreTake( xMutex, portMAX_DELAY ); // protect processHandler
			  vTaskDelay(Com->outSpeed / portTICK_PERIOD_MS); // wait outSpeed ms
  		      Com->Controller->outHandlers.setActiveType(255); // send to all handlers
  			  for (uint8_t i = 0; i<3; i++) {
  				 Com->Controller->outHandlers.call(train_set[train_num]); // call all registerd outHandlers for train character
  			     vTaskDelay(Com->outSpeed/2 / portTICK_PERIOD_MS); // wait outSpeed/2 ms
  			  }
			  vTaskDelay(Com->outSpeed / portTICK_PERIOD_MS); // wait outSpeed ms
	  	  	  xSemaphoreGive( xMutex );
	  		  state = 1;
			  break;
  	  case 1: // output train character
			  vTaskDelay(Com->outSpeed / portTICK_PERIOD_MS); // wait outSpeed ms
  		  	  xSemaphoreTake( xMutex, portMAX_DELAY ); // protect processHandler
  		  	  Com->Controller->outHandlers.setActiveType(255); // send to all handlers
  		  	  Com->Controller->outHandlers.call(train_set[train_num]); // call all registerd outHandlers for train character
  		      xSemaphoreGive( xMutex );
  		  	  state = 2;
  	  	  	  break;
  	  case 2: // wait for input and report result
  		      xSemaphoreTake( xMutex, portMAX_DELAY ); // protect processHandler
  	  	  	  for (const auto& handler : Com->Controller->inHandlers.m_handlers) // loop over all inHandlers
  	  	  	  {
  	  	  		 c = 255;
  	  	  		 handler(&c); // call inHandler
  	  	  		 if (c!=255) { // input character?
  	  	  			 if (c == DHS_COMMANDCHAR) { // command char stops train mode
  	  	  				 xSemaphoreGive( xMutex );
  	  	  				 Com->trainMode = 0;
  	  	  				 state = 0;
  	  	  				 break;
  	  	  			 }
  	  	  			 if (c != train_set[train_num]) { // wrong character?
  	  	   			     for (uint8_t i = 0; i<2; i++) {
  	  	   				    Com->Controller->outHandlers.call(train_set[train_num]); // call all registerd outHandlers for train character
  	  	   			        vTaskDelay(Com->outSpeed/2 / portTICK_PERIOD_MS); // wait outSpeed/2 ms
  	  	   			     }

//  	  	  			   for (uint8_t i = 1; i<32; i= i<<1) { // loop over 5 bits
//  	  	  				   uint8_t x = Com->Controller->charToTick(train_set[train_num]); // get bit pattern
//  	  	  				 if (x & i) {
//  	  	  					 Com->Controller->outHandlers.call( Com->Controller->tickToChar(x & i)); // call all registerd outHandlers for bit sequence of train character
//  	  	  			         vTaskDelay(Com->outSpeed/2 / portTICK_PERIOD_MS); // wait outSpeed/2 ms
//  	  	  			     }
//  	  	  			   }

	  	  			   vTaskDelay(Com->outSpeed*2 / portTICK_PERIOD_MS); // wait outSpeed ms
	  	  	  	  	   xSemaphoreGive( xMutex );
	  	  	  		   state = 1;
  	  	  			 }
  	  	  			 else {
  	  	  				 Com->Controller->outHandlers.setActiveType(255); // send to all handlers
  	  	  				 Com->Controller->outHandlers.call('a'); // call all registerd outHandlers for result character
  	  	  				 vTaskDelay(Com->outSpeed/2 / portTICK_PERIOD_MS); // wait outSpeed/2 ms
  	  	  				 Com->Controller->outHandlers.call('e'); // call all registerd outHandlers for result character
  	  	  				 vTaskDelay(Com->outSpeed/2 / portTICK_PERIOD_MS); // wait outSpeed/2 ms
  	  	  				 Com->Controller->outHandlers.call('i'); // call all registerd outHandlers for result character
  	  	  				 vTaskDelay(Com->outSpeed/2 / portTICK_PERIOD_MS); // wait outSpeed/2 ms
  	  	  				 Com->Controller->outHandlers.call('o'); // call all registerd outHandlers for result character
  	  	  				 vTaskDelay(Com->outSpeed/2 / portTICK_PERIOD_MS); // wait outSpeed/2 ms
  	  	  				 Com->Controller->outHandlers.call('u'); // call all registerd outHandlers for result character
  	  	  				 vTaskDelay(Com->outSpeed / portTICK_PERIOD_MS); // wait outSpeed ms
  	  	  	  	  	     xSemaphoreGive( xMutex );
  	  	  		  	     train_num++; // next character in train set
  	  	  		  	     if (train_set[train_num] == 0) train_num = 0; // last character reached, start from beginning
  	  	  		  	     state = 1;
  	  	  			 }
  	  	  		 }
  	  	  	  }
	  		  xSemaphoreGive( xMutex );
  	  	  	  break;
  }
}

