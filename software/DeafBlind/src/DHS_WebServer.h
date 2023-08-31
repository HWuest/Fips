/*
 * DHS_WebServer.h
 *
 *  Created on: 16.05.2023
 *      Author: HW
 */

#ifndef DHS_WEBSERVER_H_
#define DHS_WEBSERVER_H_

#include "DHS_1Tick_Control.h"
#include "DHS_TickCommand.h"
//#include "esp_http_server.h"
#include <esp_https_server.h>

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  4096

class DHS_WebServer {
  public:
	char *base_path = (char*)"/spiflash";

    int outHandlerId;

	DHS_WebServer(DHS_1Tick_Control *Cont);

	static void setCommandProcessor( DHS_TickCommand *Com) { Command = Com; };
	static esp_err_t broadcast(char* data);// broadcast data to all WebSocket clients (skipping Controller->sourcfd)
	static esp_err_t send(char* data, int fd); // send to one http client using fd
	static esp_err_t ssend(char* data, int fd); // send to one https client using fd
	int8_t getNumConnections();
	static bool process(char* payload);   // process WebServer commands
	static void initialise_mdns();

    static DHS_1Tick_Control *Controler; // 1Tick_Controller pointer for callback
    static DHS_TickCommand   *Command;    // command processer pointer for callback
    static httpd_handle_t server,sserver;
};

#endif /* DHS_WEBSERVER_H_ */
