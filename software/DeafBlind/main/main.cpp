/**********************************************
 * DHS DeafBlind main Programm                *
 **********************************************/

#include "../src/DHS_FileSystem.h"
#include "../src/DHS_Config.h"
#include "../src/DHS_1Tick_Control.h"
#include "../src/DHS_GPIO.h"
#include "../src/DB_LCD.h"
#include "../src/DHS_Mesh_lite.h"
#include "../src/DHS_WebServer.h"
#include "../src/DHS_WebsocketClient.h"
#include "../src/DHS_Console.h"
#include "../src/DHS_TickCommand.h"
#include "DHS_Train_Control.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_bridge.h"

TaskHandle_t DHSStartupTaskHandle = NULL;

void DHSStartup(void *arg) // report startup ready to local deafblind output
{
	uint8_t i, l;
	DHS_GPIO *GPIO = (DHS_GPIO*) arg;

	for (i = 0; i < 60; i++) { // wait 10 seconds for connection

		vTaskDelay(1000 / portTICK_PERIOD_MS); // wait 1s
		ESP_LOGW("DHS","Waiting for connection... %d", 60 - i);

		if ( (l = esp_mesh_lite_get_level()) == 1)	break;

	    GPIO->ledOn();
//		GPIO->setIO(i, 1); // report start process to deafblind tick output
		vTaskDelay(100 / portTICK_PERIOD_MS); // wait 100 ms
//		GPIO->setIO(i, 0);
	    GPIO->ledOff();
	}

	l = esp_mesh_lite_get_level();
	if (l) {
		if (l == 1)	{
			ESP_LOGW("DHS","Root node with Rooter connection");
		}
		else        ESP_LOGW("DHS","Child node Level %d", l);
		vTaskDelay(200 / portTICK_PERIOD_MS); // wait 200 ms

		for (i = 0; i < l; i++) { // report level to deafblind tick output
			GPIO->setIO(i, 1);
			vTaskDelay(200 / portTICK_PERIOD_MS); // wait 100 ms
			GPIO->setIO(i, 0);
		}
	} else {
		ESP_LOGW("DHS","Root node without Rooter connection");
		esp_mesh_lite_set_allowed_level(1); // enable rooterless network connection
//		esp_wifi_set_mode(WIFI_MODE_AP);
	}
	ESP_LOGW("DHS","Ready and running...");
	vTaskDelete(NULL);
}

DHS_FileSystem FS;  						// FileSystem instantiation
DHS_Config Config((char*)"DHS");			// Configuration storage instantiation

extern "C" void app_main(void)
{
    ESP_LOGW("DHS","Starting %s ( %s %s )...",DHS_HEADLINE,__DATE__ , __TIME__);

    //esp_log_level_set("*", ESP_LOG_INFO);

    DHS_1Tick_Control Tick_Controller; 			// 1Tick Controller instantiation
    DHS_GPIO	GPIO(&Tick_Controller, &Config);// GPIO In/OutHandler instantiation
    GPIO.ledOn();								// signal start by Onboard Led on
    DB_LCD LCD(&Tick_Controller, &Config);		// LCD and touch instantiation
    DHS_Mesh_lite Mesh(&Tick_Controller, &Config); 	// Mesh network instantiation
    DHS_WebServer WS(&Tick_Controller);   		// WebServer instantiation
    DHS_WebsocketClient WC(&Tick_Controller, &Config); // internet connection as Websocket-Client instantiation
    DHS_Console	Console(&Tick_Controller);  	// Console In/OutHandler instantiation
    // TickCommand process needs to be last component (generates additional tick outputs from input)
    DHS_TickCommand TickCommand(&Tick_Controller, & Config);
    // therefore WebSocket TickCommand-Processor has to be registered afterwards
    WS.setCommandProcessor(&TickCommand);
    DHS_Train_Control Train_Controller(&TickCommand);			// Train_Controller instantiation

    GPIO.ledOff();								// signal initialization finished by Onboard Led off

    xTaskCreate(DHSStartup, "DHSStartupTask", 3000, &GPIO, uxTaskPriorityGet(NULL), &DHSStartupTaskHandle); // report start ready after some delay to allow WiFi connections

//    uint32_t count = 0;
    while (1) { 								// endless processing loop
    	if (!TickCommand.trainMode) Tick_Controller.processHandlers();  	// main process loop
    	else Train_Controller.processHandlers(); // spezial train mode process loop
//    	count++;
//    	if (count % 20 == 0) Mesh.send_empty();      // send empty message for time measurement
        vTaskDelay(10/portTICK_PERIOD_MS);  	// sleep 10 ms (max. 100 ticks/sec)
    }
}
