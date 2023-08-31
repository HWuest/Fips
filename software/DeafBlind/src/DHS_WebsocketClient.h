/*
 * DHS_WebsocketClient.h
 *
 *  Created on: 23.05.2023
 *      Author: HW
 */

#ifndef SRC_DHS_WEBSOCKETCLIENT_H_
#define SRC_DHS_WEBSOCKETCLIENT_H_

#include "DHS_1Tick_Control.h"
#include "DHS_Config.h"
#include "esp_websocket_client.h"

class DHS_WebsocketClient {
  public:
	static char uri[50];
	static esp_websocket_client_handle_t client;
	static uint8_t serverConnect;

	static DHS_1Tick_Control *Controller;
    static DHS_Config *Config;

	DHS_WebsocketClient(DHS_1Tick_Control *Cont, DHS_Config *Conf);
	virtual ~DHS_WebsocketClient();

	static void start() {
 	    	ESP_LOGW("DHS", "Connecting to internet server...");
   	        esp_websocket_client_start(client);
    };

	static void stop()  { esp_websocket_client_stop(client); };
	void send(char* t);
	static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

};

#endif /* SRC_DHS_WEBSOCKETCLIENT_H_ */
