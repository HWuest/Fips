/*
 * DHS_WebsocketClient.cpp
 *
 *  Created on: 23.05.2023
 *      Author: HW
 */

#include "DHS_WebsocketClient.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"

DHS_1Tick_Control *DHS_WebsocketClient::Controller;
DHS_Config *DHS_WebsocketClient::Config;
esp_websocket_client_handle_t DHS_WebsocketClient::client;
char DHS_WebsocketClient::uri[50];
uint8_t DHS_WebsocketClient::serverConnect;

void DHS_WebsocketClient::websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {

    case WEBSOCKET_EVENT_CONNECTED:
        uint8_t addr[6];
        esp_wifi_get_mac(WIFI_IF_STA, addr);
        char r[20];
        sprintf(r,"%3d%3d%3d%3d%3d%3d",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
        esp_websocket_client_send_text(client, r, 19, portMAX_DELAY); // send to websocket start message over internet
        ESP_LOGW("DHS", "WEBSOCKET_INTERNET_CLIENT_CONNECTED, Send start message %s",r);
        DHS_WebsocketClient::Controller->iClients++;
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW("DHS", "WEBSOCKET_INTERNET_CLIENT_DISCONNECTED");
        if (DHS_WebsocketClient::Controller->iClients) DHS_WebsocketClient::Controller->iClients--;
        break;

    case WEBSOCKET_EVENT_DATA:
    	if (data->op_code == 1) { // text recieved
    		if (data->data_len == 1) {
    	        ESP_LOGI("DHS", "Recieved %c from internet websocket ID %ld",((char*)data->data_ptr)[0],event_id);
    			DHS_WebsocketClient::Controller->inHandler(*(char*)data->data_ptr,DHS_LocalHandler|DHS_CommandHandler); // process tick char from local handlers only
    		}
    		else if (data->data_len == 6) {
    			((char*)data->data_ptr)[6]=0;
    	        ESP_LOGI("DHS", "Recieved %s from internet websocket ID %ld",(char*)data->data_ptr,event_id);
    			DHS_WebsocketClient::Controller->inHandler( ((char*)data->data_ptr)[5], DHS_LocalHandler|DHS_CommandHandler); // process tick char from local handlers only
     		}
    	}
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW("DHS", "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

DHS_WebsocketClient::DHS_WebsocketClient(DHS_1Tick_Control *Cont, DHS_Config *Conf) {
	Controller = Cont; // set one tick controller

    esp_websocket_client_config_t websocket_cfg = {};
	Config = Conf; // set configuration storag

	Config->read_string("uri", uri, (char*)"wss://dhsserver.dynv6.net/ws"); //CONFIG_WEBSOCKET_URI;;
	websocket_cfg.uri = uri;
	websocket_cfg.reconnect_timeout_ms = 10000; // 10 seconds
	websocket_cfg.network_timeout_ms = 10000; // 10 seconds
	DHS_WebsocketClient::serverConnect = Config->read( (char*) "serverConnect",0);

	client = esp_websocket_client_init(&websocket_cfg);
	esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, DHS_WebsocketClient::websocket_event_handler, (void *)client);

//	serverConnect = Config->read((char*)"serverConnect", (uint8_t) 0);
//    if (serverConnect) {
//    	ESP_LOGW("DHS", "Connecting to %s...", websocket_cfg.uri);
//    	esp_websocket_client_start(client);
//    }

	DHS::event_handler <char> WSClientOutHandler([this](char c)  { // WebsocketClient char output handler function
		if (c == 255) return; // no character given
		if (Controller->serverfd>0) return; // I'm the server, don't send to myself, sending to others done with webserver broadcast
		if (!esp_websocket_client_is_connected(client)) return;
		ESP_LOGI("DHS","Send to internet server %c\n",c);
		esp_websocket_client_send_text(client, &c, 1, portMAX_DELAY); // send to websocket server over internet
	});
	WSClientOutHandler.setType(DHS_WebClientHandler); // WebClient Handler
	Controller->outHandlers += WSClientOutHandler;
}

DHS_WebsocketClient::~DHS_WebsocketClient() {
    esp_websocket_client_stop(client);
	ESP_LOGI("DHS", "Websocket Stopped");
	esp_websocket_client_destroy(client);
}

void DHS_WebsocketClient::send(char* t)
{
	if (esp_websocket_client_is_connected(client)) {
        ESP_LOGI("DHS", "Sending %s", t);
        esp_websocket_client_send_text(client, t, strlen(t), portMAX_DELAY);
	}
}
