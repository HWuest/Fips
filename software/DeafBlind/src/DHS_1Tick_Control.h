/*
 * DHS_1Tick_Control.hpp
 *
 *  Created on: 09.05.2023
 *      Author: HW
 */

#ifndef DHS_1TICK_CONTROL_H_
#define DHS_1TICK_CONTROL_H_

#define DHS_CHARSETNUM   3  // number of charsets

#include "DHS_EventHandler.h"

#define DHS_LocalHandler  1
#define DHS_RemoteHandler 2
#define DHS_CommandHandler 4
#define DHS_WebClientHandler 8
#define DHS_WebsocketHandler 16

class DHS_1Tick_Control {
  public:
    char tickChar[DHS_CHARSETNUM][32]={{' ','a','e','n','i','d','t','\x84' /*ä*/,'o','k','m','f','l','g','\x94' /*ö*/,'r','u','y','b','p','z','w','q','j','s', '\x81' /*ü*/,'x','v','*','c','h','#'},
                                       {' ','1','2','3','4','5','6','7'         ,'8','9','0','!','-',':','='         ,'&','/','(',')','<','>',',','.','?','{', '\x81' /*ü*/,'}','$','*','@','%','#'},
                                       {' ','A','E','N','I','D','T','\x8E' /*Ä*/,'O','K','M','F','L','G','\x99' /*Ö*/,'R','U','Y','B','P','Z','W','Q','J','S', '\x9A' /*Ü*/,'X','V','*','C','H','#'}};

	uint8_t charSet; // current active charset
	bool locked;	 // charset caps-locked
    int sourcefd;	 // web socket message source file-descriptor to be skipped
    int serverfd;	 // web socket message server file-desvriptor to be skipped
    uint8_t iClients=0; // number of connected internet clients

	DHS::event <char> outHandlers; // list of 1Tick output modules
	DHS::event <char*> inHandlers; // list of 1Tick input modules

	DHS_1Tick_Control(); // constructor

	void processHandlers(); // process all input and output handlers
	void inHandler(char c, uint8_t activeType); // character input handler, process new character

	char tickToChar(uint8_t tick) { return tickChar[charSet][tick]; }; // convert tick to char from ative charset
	uint8_t charToTick( char c);  // convert char to tick from active charset
	void checkChar(char *c); // check if character is in charsets
};


#endif /* DHS_1TICK_CONTROL_H_ */
