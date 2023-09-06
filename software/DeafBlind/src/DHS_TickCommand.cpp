/*
 * DHS_TickCommand.cpp
 *
 *  Created on: 15.05.2023
 *      Author: HW
 *
 *      1-Tick command processor
 */

#include "DHS_TickCommand.h"

#include "DHS_GPIO.h" // to access static member set-functions
#include "DHS_WebsocketClient.h"
#include "DHS_Config.h"
#include "DHS_LCD.h"
#include "DHS_LCDklein.h"
#include "DHS_Mesh_lite.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_websocket_client.h"
#include <cstring>

int DHS_TickCommand::outSpeed;

DHS_TickCommand::DHS_TickCommand(DHS_1Tick_Control *Cont, DHS_Config *Conf) {

	Controller = Cont; // set one tick controller
	Config = Conf;
	outSpeed = Config->read( (char*)"outSpeed", 300);

	DHS::event_handler <char> TickCommandoutHandler([this](char c)  { // TickCommand char output handler function (get char from TickController)
		ESP_LOGI("DHS","CommandHandler %c",c);
		if (c == 255) return; // no character given

	    tickString += c;  // add char to inputString

		if (c == DHS_COMMANDCHAR) { // command character recieved?
			if (Controller->charSet && (!Controller->locked)) { // locke charset
				Controller->locked = true;
				charsetMode=0;
				return;
	        }
			comMode++; // activate command mode
			charsetMode=0;
			ESP_LOGI("DHS", "Comm %d", comMode);
			return;
		}
		if (comMode) { // command mode active ?
			if (comMode >= DHS_COMMNUM) { // given number of command chars recieved ?
				process(c); // process command char
				comMode = 0; // switch comMode off
				return;
		  }
			else {
				comMode = 0; // other than second command char recieved, switch comMode off
				return;
			}
		}
		else if (c == DHS_CHARSETCHAR) { // charset change character ?
			charsetMode++; // count charset chars
			if (charsetMode>=DHS_COMMNUM) { // given number of charset chars recieved ?
			  charsetMode=0; // clear charset counter
			  if (Controller->locked) {
				Controller->locked = false; // if charset locked -> unlock
				Controller->charSet = 0; // reset charset
				ESP_LOGI("DHS", "charSet unlocked",c);
			  }
			  else {
				Controller->charSet = ( Controller->charSet+1 ) % DHS_CHARSETNUM; // select next charset;
				ESP_LOGI("DHS", "charSet %d",Controller->charSet);
			  }
			}
			return;
		}
		charsetMode=0; // other charecter, clear charset counter

		if (Controller->charSet && (!Controller->locked)) { // other character and changed charset and not locked
		    	 Controller->charSet = 0; // reset charset after char is processed if charset is not locked
	  			 ESP_LOGI("DHS", "charSet reset");
        }
	});

	TickCommandoutHandler.setType(DHS_CommandHandler); // local handler
    outHandlerId = Controller->outHandlers += TickCommandoutHandler;
}

void DHS_TickCommand::process(char c) {
	ESP_LOGI("DHS", "Command %c", c);
	switch (c) {  // process command
		case DHS_CHARSETCHAR: // charSet change command char ?
		  if (Controller->charSet) Controller->locked = true; // lock charSet
  		  ESP_LOGI("DHS", "charSet locked");
		  break;
		case 'n': // newline command char
		  Controller->outHandlers.setActiveType(DHS_LocalHandler);
		  Controller->outHandlers.call('\n'); // output newline char
		  tickLine = tickString.substr(0, tickString.length() - DHS_COMMNUM -1);
	      tickString = "";
	      ESP_LOGW("DHS"," Stored line: %s",tickLine.c_str());
	      break;
		case 'l': // remove char
		  Controller->outHandlers.setActiveType(DHS_LocalHandler);
		  Controller->outHandlers.call('\b'); // output backspace char
	      tickString = tickString.substr(0, tickString.length() - DHS_COMMNUM-2);
	      break;
		case 'z': // space command char ?
	      tickString = tickString.substr(0, tickString.length() - DHS_COMMNUM) + " ";  // add space character to text line
	      Controller->outHandlers.setActiveType(DHS_LocalHandler);
		  Controller->outHandlers.call(' '); // output space char
	      break;
	    case 'm': // tick mode change
	      DHS_GPIO::tickMode++;
	      if (DHS_GPIO::tickMode > 3) DHS_GPIO::tickMode = 0;
	      break;
	    case 'e': // execute command char ?
	      if (tickLine.length() > (DHS_COMMNUM-1) ) {
	          processLine(tickLine); // process line command
	          tickString = ""; // clear line after processing
	      }
	      break;
	    case 't': // train mode command char ?
	      if (trainMode) trainMode = 0;
	      else trainMode = 1;
	      ESP_LOGW("DHS"," Train Mode: %d",trainMode);
	      break;
	    case 'r': // routerless mode on root node
	      if ( esp_mesh_lite_get_level() == 1) {
	    	  esp_wifi_set_mode(WIFI_MODE_AP);
	    	  ESP_LOGW("DHS"," routerless mode activated");
	      }
	      break;
	    default: ESP_LOGW("DHS", "Unknown command *%c",c);  // command not found
	  }
}

TaskHandle_t DHSLineOutTaskHandle = NULL;

void DHSLineOutTask(void *arg)
{	std::string l=((DHS_TickCommand*)arg)->tickLine;
    ((DHS_TickCommand*)arg)->tickLine = ""; // clear command line

    for (int i = 2; i < l.length(); i++) {
	    ESP_LOGI("DHS","-> %c",l[i]);
    	((DHS_TickCommand*)arg) -> Controller -> inHandler(l[i],255); // send line chars to all handlers
    	vTaskDelay(((DHS_TickCommand*)arg)->outSpeed / portTICK_PERIOD_MS); // wait outSpeed time
    }

	vTaskDelete(NULL);
}

extern int replacechar(char *str, char orig, char rep);
extern uint8_t MESH_ID[6];

void DHS_TickCommand::processLine(std::string l) {
	ESP_LOGI("DHS", "Line Command %s", l.c_str());
	  int pos;

	  switch (l[0]) {  // process command

	    case 's':
	    	pos = atoi((char*)&l[2]);
	    	DHS_GPIO::stabTime = (uint64_t)pos * 1000;
	    	ESP_LOGW("DHS","stabTime: %d ms", pos);
	      break;
	    case 'd':

	    	pos = atoi((char*)&l[2]);
	    	if (pos >= portTICK_PERIOD_MS) DHS_GPIO::pulseDuration = pos;
	    	ESP_LOGW("DHS","PulseDuration: %d ms", pos);
	      break;

	    case 'x':
	      pos = atoi((char*)&l[2]);
	      if (pos >= portTICK_PERIOD_MS) outSpeed = pos;
	      ESP_LOGW("DHS","outSpeed: %d ms", pos);
	      break;

	    case 'o':
	      tickLine = std::string(l.c_str());
		  xTaskCreate(DHSLineOutTask, "DHSLineOutTask", 5000, this, uxTaskPriorityGet(NULL)+1, &DHSLineOutTaskHandle); // could be optimized with on task build waiting for xTaskGenericNotify(...) / xTaskNotifyWaitIndexed(...)
	      break;
	    case 'r':
	      ESP_LOGE("DHS","Rebooting...");
	      esp_restart();
	      break;  // reset target

	    case 'g':
	    	outSpeed = Config->read( (char*)"outSpeed", 300);
	    	DHS_GPIO::stabTime = Config->read( (char*)"stabTime", (int64_t) 70*1000);
	    	DHS_GPIO::pulseDuration = Config->read( (char*)"pulseDuration", 100);
	    	DHS_GPIO::tickMode = Config->read( (char*)"tickMode",(uint8_t) 0);
	    	DHS_LCD::lcdActive = Config->read( (char*)"lcdActive",0);
	    	DHS_LCDklein::lcdActive = Config->read( (char*)"lcdklein",0);
	    	DHS_WebsocketClient::serverConnect = Config->read( (char*) "serverConnect",0);
	    	char s[50],p[50];
	    	Config->read_string("uri",s, (char*)"wss://88.130.0.18/ws");
	    	if (strcmp(s,DHS_WebsocketClient::uri)!=0) esp_websocket_client_set_uri(DHS_WebsocketClient::client,DHS_WebsocketClient::uri);

	    	Config->read_string("ssid",s,(char*)"db");
	    	Config->read_string("pw",p,(char*)"123456789");
	    	if (strcmp(s,(char*)&cfg.sta.ssid) || strcmp(p,(char*)&cfg.sta.password)) {
		      strncpy((char*) &cfg.sta.ssid, s, 32);
		   	  strncpy((char*) &cfg.sta.password, p, 64);
		      ESP_LOGW("DHS","ssid: %s, password: %s",s,p);
		      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config( WIFI_IF_STA, &cfg));
//		      esp_wifi_disconnect(); // disconnect with autmatic reconnect by disconnect event
	    	}
	        break;  // load current configuration from file system

	    case 'p':
	    	Config->write( (char*)"outSpeed", outSpeed);
	        Config->write( (char*)"stabTime", DHS_GPIO::stabTime);
	        Config->write( (char*)"pulseDuration", DHS_GPIO::pulseDuration);
	        Config->write( (char*)"tickMode", DHS_GPIO::tickMode);
	        Config->write( (char*)"lcd", DHS_LCD::lcdActive);
	        Config->write( (char*)"lcdklein", DHS_LCDklein::lcdActive);
	        Config->write( (char*)"serverConnect", DHS_WebsocketClient::serverConnect);
	        ESP_LOGW("DHS","Config saved");
	        break;  // save current configuration in file system

	    case 'c':
	      pos = atoi((char*)&l[4]);  // set character in character set. Example: c ! 43
	      if (pos / 32 < DHS_CHARSETNUM) {
	        Controller->tickChar[pos / 32][pos % 32] = l[2];
	        ESP_LOGW("DHS","Set Charset %d, Pos %d: %c", pos / 32, pos % 32, l[2]);
	      }
	      break;

	    case 't':
	      DHS_GPIO::tickMode = atoi((char*)&l[2]);
	      ESP_LOGW("DHS"," TickMode: %d", DHS_GPIO::tickMode);
	      break;  // change tickMode
	    case 'L':
	      DHS_LCD::lcdActive = atoi((char*)&l[2]);
	      ESP_LOGW("DHS"," LCD gross Active: %d", DHS_LCD::lcdActive);
	      break;  // change tickMode
	    case 'l':
	      DHS_LCDklein::lcdActive = atoi((char*)&l[2]);
	      ESP_LOGW("DHS"," LCD klein Active: %d", DHS_LCDklein::lcdActive);
	      break;  // change tickMode
	    case 'q':
	      if (l.length() > 2) DHS_GPIO::IN[0] = atoi((char*)&l[2]);  // set IO-Ports Example q 15 04 18
	      if (l.length() > 5) DHS_GPIO::IN[1] = atoi((char*)&l[5]);
	      if (l.length() > 8) DHS_GPIO::IN[2] = atoi((char*)&l[8]);
	      if (l.length() > 11) DHS_GPIO::IN[3] = atoi((char*)&l[11]);
	      if (l.length() > 14) DHS_GPIO::IN[4] = atoi((char*)&l[14]);
	      if (l.length() > 17) DHS_GPIO::OUT[0] = atoi((char*)&l[17]);
	      if (l.length() > 20) DHS_GPIO::OUT[1] = atoi((char*)&l[20]);
	      if (l.length() > 23) DHS_GPIO::OUT[2] = atoi((char*)&l[23]);
	      if (l.length() > 26) DHS_GPIO::OUT[3] = atoi((char*)&l[26]);
	      if (l.length() > 29) DHS_GPIO::OUT[4] = atoi((char*)&l[29]);
	      DHS_GPIO::initIOs();  // init IO-Pins
	      ESP_LOGW("DHS","In0 %2d In1 %2d In2 %2d In3 %2d In4 %2d  Out0 %2d Out1 %2d Out2 %2d Out3 %2d Out4 %2d",
	    		  DHS_GPIO::IN[0], DHS_GPIO::IN[1], DHS_GPIO::IN[2], DHS_GPIO::IN[3], DHS_GPIO::IN[4], DHS_GPIO::OUT[0], DHS_GPIO::OUT[1], DHS_GPIO::OUT[2], DHS_GPIO::OUT[3], DHS_GPIO::OUT[4]);
	      break;
	    case 'm':
	      pos = atoi((char*)&l[2]);
	      DHS_Mesh_lite::id = pos;
	      Config->write( (char*)"MeshID", pos);
	      esp_mesh_lite_set_mesh_id( pos,true); // set id and force NVS update
	      ESP_LOGW("DHS","Mesh-ID set to: %d and stored, reset device to activate!\n", pos);
	      break;

	      case 'w': // change WiFi ssid and password
	   		char *ptr;
	   		replacechar((char*)&l[2], '~', ' ');
	   		ptr = strtok((char*)&l[2], ",");
	        if (ptr) strncpy((char*) &cfg.sta.ssid, ptr, 32);
	   		ptr = strtok(NULL, " ");
	   		if (ptr) strncpy((char*) &cfg.sta.password, ptr, 64);
		    ESP_LOGW("DHS","Set ssid: %s, password: %s and saved in configuration",cfg.sta.ssid,cfg.sta.password);
	        Config->write_string("ssid",(char*)cfg.sta.ssid);
	        Config->write_string("pw",(char*)cfg.sta.password);
//		    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config( WIFI_IF_STA, &cfg));
//		    esp_wifi_disconnect(); // disconnect with autmatic reconnect by disconnect event
		    break;

	    case 'i': // change internet server uri
	   		replacechar((char*)&l[2], '~', ' ');
	    	sprintf(DHS_WebsocketClient::uri,"wss://%s/ws",(char*)&l[2]);
	        ESP_LOGW("DHS", "Internet server set to %s and saved in configuration", DHS_WebsocketClient::uri);
	        Config->write_string("uri",DHS_WebsocketClient::uri);
	    	esp_websocket_client_set_uri(DHS_WebsocketClient::client,DHS_WebsocketClient::uri);
	      break;

	    case 'I':
	      DHS_WebsocketClient::serverConnect = atoi((char*)&l[2]);
	      ESP_LOGW("DHS"," ServerConnect: %d", DHS_WebsocketClient::serverConnect);
	      if (DHS_WebsocketClient::serverConnect) DHS_WebsocketClient::start();
	      else DHS_WebsocketClient::stop();
	      break;  // change tickMode
	  }
}
