#include "DHS_1Tick_Control.h"

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

using namespace std;

SemaphoreHandle_t xMutex = NULL;

DHS_1Tick_Control::DHS_1Tick_Control() {
	 charSet = 0;
	 locked = false;
 	 sourcefd = -1; // clear source fd
     serverfd = -1; // clear server fd

     xMutex = xSemaphoreCreateMutex();
}

void DHS_1Tick_Control::processHandlers() { // process loop, process all in--/outHandlers
	 char c = 255;

	 xSemaphoreTake( xMutex, portMAX_DELAY ); // protect processHandler
	 outHandlers.setActiveType(255); // send to all handlers
	 for (const auto& handler : inHandlers.m_handlers) // loop over all inHandlers
	 {
 		 c = 255;
		 handler(&c); // call inHandler
// 		 if (c!=255) checkChar(&c); // check char if valid
		 if (c != 255) {
			 outHandlers.call(c); // call all registerd outHandlers for every recived character
		 }
	 }
     xSemaphoreGive( xMutex );
}

void DHS_1Tick_Control::inHandler(char c, uint8_t activType) { // process input character by outHandlers

	checkChar(&c); // check char if valid
	if (c == 255) return;

     xSemaphoreTake( xMutex, portMAX_DELAY ); // protect processHandler
     outHandlers.setActiveType(activType);
     outHandlers.call(c); // call all registerd outHandlers for given character and type bit pattern
     xSemaphoreGive( xMutex );
}

void DHS_1Tick_Control::checkChar(char *c) { // convert tick bit patern to char using charsets
	int i;
	for (i = 0; i < 32; i++) if (tickChar[charSet][i] == *c) break; // search for char in active charset
	if (i != 32) return;  // char found
    if ((*c==' ')||(*c == '\n') || (*c== '\b')) return; // allow space, backspace or newline (from commands)

	ESP_LOGI("DHS", "not in current charset");

	for (uint8_t j = 1; j<DHS_CHARSETNUM; j++) { // not found try other charsets (if charset change happend parallel, tick might be wrong!)
		for (i = 0; i < 32; i++) if (tickChar[ (charSet+j) % DHS_CHARSETNUM ][i] == *c) break; // search for char in active charset
		if (i != 32) return;  // char found in other charset
	}
    *c = 255;
	ESP_LOGW("DHS", "not found in any charset");
	return; // not found
}

uint8_t DHS_1Tick_Control::charToTick(char c) { // convert tick bit patern to char using charsets
	int i;
    if ((c==' ')||(c == '\n') || (c== '\b')) return 255; // ignore space, backspace or newline (from commands)

    for (i = 0; i < 32; i++) if (tickChar[charSet][i] == c) break; // search for char in active charset
	if (i != 32) return i;  // char found
	return 255; // not found
}
