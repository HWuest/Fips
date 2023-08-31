/*
 * DHS_Console.cpp
 *
 *  Created on: 10.05.2023
 *      Author: HW
 */

#include "DHS_Console.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "esp_log.h"

#include "DHS_1Tick_Control.h"

DHS_Console::DHS_Console(DHS_1Tick_Control *Cont) {

	Controller = Cont; // set one tick controller
	fd_id = fileno(stdin);

	// 1Tick inHandler setup
	fcntl( fd_id, F_SETFL, O_NONBLOCK); // Enable non-blocking mode on stdin to read single characters without waiting for input (read(fileno(stdin), &char, 1))

	DHS::event_handler <char*> ConsoleInHandler([this](char* c)  { // GPIO char handler function

		if ( !read(fd_id, c, 1)) *c = 255; // 255 if no char recieved
//		if (*c!=255) ESP_LOGW("DHS","Got console char %d",*c);
	});

	inHandlerId = Controller->inHandlers += ConsoleInHandler;
    ConsoleInHandler.setType(DHS_LocalHandler); // local handler

	// 1Tick outHandler setup
	fflush(stdout); // empty buffer
	setvbuf(stdout, NULL, _IONBF, 0); // needed to enable direct char output

    DHS::event_handler <char> ConsoleOutHandler([this](char c) { // output char handler function
		if ( c=='\n' ) printf("\r\n"); // cr changed to /cr/lf sequence
		else if ( c=='\b' ) printf("\b\b\b\b    \b\b\b\b"); // remove last char incl. command sequence *-
		else printf("%c",c);

		fsync(fd_id); // emidiately output each char !
	});

    ConsoleOutHandler.setType(DHS_LocalHandler); // local handler
    outHandlerId = Controller->outHandlers += ConsoleOutHandler;
}

DHS_Console::~DHS_Console() {

	Controller->inHandlers.remove_id(  inHandlerId );
	Controller->outHandlers.remove_id( outHandlerId);
}
