/*
 * DHSMesh_lite.cpp
 *
 *  Created on: 14.05.2023
 *      Author: HW
 */

#include "DHS_Mesh_lite.h"
#include "DHS_WebsocketClient.h"
#include "DHS_WebServer.h"

#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_bridge.h"

//#define CONFIG_MESH_ROOT 1 // define 1 if root node!

DHS_1Tick_Control *DHS_Mesh_lite::Controller;

int DHS_Mesh_lite::id;

static cJSON* rec_process(cJSON *payload, uint32_t seq)
{
	cJSON *data = NULL;
	cJSON *mac = NULL;
	cJSON *t = NULL;
	cJSON_bool fromRoot;
	cJSON *item = NULL;
    uint8_t sta_mac[6]  = {0};
    char mac_str[MAC_MAX_LEN];

	item = cJSON_CreateObject();

    esp_wifi_get_mac(WIFI_IF_STA, sta_mac);
    snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(sta_mac));

	data = cJSON_GetObjectItem(payload, "send");
	mac = cJSON_GetObjectItem(payload, "mac");
	t = cJSON_GetObjectItem(payload, "time");
	fromRoot = cJSON_IsTrue(cJSON_GetObjectItem(payload, "fromRoot"));

	uint8_t l = esp_mesh_lite_get_level();
    if (l == 1) { // root
        if (strlen(data->valuestring) == 1) DHS_Mesh_lite::Controller->inHandler(data->valuestring[0],DHS_LocalHandler | DHS_WebClientHandler | DHS_CommandHandler | DHS_WebsocketHandler); // call tickRecHandler if single char recieved

      ESP_LOGI("DHS", "%d Root recieved %ld %s from %s, send back to childs",l,seq, data->valuestring, mac->valuestring);
      cJSON_AddStringToObject(item, "send", data->valuestring);
      cJSON_AddStringToObject(item, "mac", mac->valuestring);
      cJSON_AddNumberToObject(item, "time", t->valuedouble);
      cJSON_AddBoolToObject(item, "fromRoot", true);
      esp_mesh_lite_try_sending_msg((char*)"rec", (char*)"rec_ack", 1, item, &esp_mesh_lite_send_broadcast_msg_to_child);
////      esp_mesh_lite_try_sending_msg((char*)"rec", NULL, 0, item, &esp_mesh_lite_send_broadcast_msg_to_child);

    }
    else { // client, check source and skip own broadcast
    	if (!strcmp(mac->valuestring, mac_str)) {
//    		ESP_LOGW("DHS", "Send Time: %4.0f Msg-Delay %4.0f",t->valuedouble* (0.001 / _TICKS_PER_US),timer_delta_ms(timer_u32()-(uint32_t)t->valuedouble));
    	}
    	else {
    		if (fromRoot) {
    			ESP_LOGI("DHS","fromRoot");
    			if (strlen(data->valuestring) == 1) DHS_Mesh_lite::Controller->inHandler(data->valuestring[0],DHS_LocalHandler | DHS_WebClientHandler | DHS_CommandHandler | DHS_WebsocketHandler); // call tickRecHandler if single char recieved
    			ESP_LOGI("DHS", "%d Client recieved %ld %s from %s",l,seq, data->valuestring, mac->valuestring);
    			cJSON_AddStringToObject(item, "send", data->valuestring);
    			cJSON_AddStringToObject(item, "mac", mac->valuestring);
    		    cJSON_AddNumberToObject(item, "time", (double)timer_u32());
    		    cJSON_AddBoolToObject(item, "fromRoot", true);
    			esp_mesh_lite_try_sending_msg((char*)"rec", (char*)"rec_ack", 1, item, &esp_mesh_lite_send_broadcast_msg_to_child);
    	////      esp_mesh_lite_try_sending_msg((char*)"rec", NULL, 0, item, &esp_mesh_lite_send_broadcast_msg_to_child);
    		}
    		else { // from child, send up to parent
     			ESP_LOGI("DHS", "Message from Child: Level %d Client recieved %ld %s from %s",l,seq, data->valuestring, mac->valuestring);
     			cJSON_AddStringToObject(item, "data", data->valuestring);
     			cJSON_AddStringToObject(item, "mac", mac->valuestring);
     			cJSON_AddNumberToObject(item, "time", (double)timer_u32());
     		    cJSON_AddBoolToObject(item, "fromRoot", false);
     			esp_mesh_lite_try_sending_msg((char*)"rec", (char*)"rec_ack", 1, item, &esp_mesh_lite_send_msg_to_parent);
    		}
    	}
    }
    cJSON_Delete(item);

    return NULL;
}

static cJSON* rec_ack(cJSON *payload, uint32_t seq)
{
	ESP_LOGI("DHS", "Ack %ld",seq);
    return NULL;
}
static const esp_mesh_lite_msg_action_t node_action[] = {
    /* Report information to the root node */
    {"rec", "rec_ack", rec_process},
    {"rec_ack", NULL, rec_ack},
////    {"rec", NULL, rec_process},
    {NULL, NULL, NULL} /* Must be NULL terminated */
};

void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGW("DHS","Device IP is: " IPSTR, IP2STR(&event->ip_info.ip));

    if ( esp_mesh_lite_get_level() == 1 ) { // if root node
	  DHS_WebServer::initialise_mdns(); // start mdns server
	  if (DHS_WebsocketClient::serverConnect) DHS_WebsocketClient::start(); // start websocket client
    }
}

void ip_event_handler2(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
   ESP_LOGE("DHS","Lost IP");
   //esp_wifi_connect(); // try reconnection
}

DHS_Mesh_lite::DHS_Mesh_lite(DHS_1Tick_Control *Cont, DHS_Config *Conf) {

	Controller = Cont; // set one tick controller
	Config = Conf;

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_init());

//  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
//	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_APSTA));

	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_loop_create_default());
	esp_bridge_create_all_netif();

    esp_log_level_set("*", ESP_LOG_INFO);
    // Station
    wifi_config_t wifi_config;

    Config->read_string("ssid",(char*)wifi_config.sta.ssid, CONFIG_MESH_ROUTER_SSID);
    Config->read_string("pw", (char*)wifi_config.sta.password, CONFIG_MESH_ROUTER_PASSWD);
    ESP_LOGW("DHS","WLAN %s,%s",wifi_config.sta.ssid,wifi_config.sta.password);

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_bridge_wifi_set(WIFI_MODE_STA, (char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password, NULL));

    // Softap
    memset(&wifi_config, 0x0, sizeof(wifi_config_t));
    size_t softap_ssid_len = sizeof(wifi_config.ap.ssid);
    if (esp_mesh_lite_get_softap_ssid_from_nvs((char *)wifi_config.ap.ssid, &softap_ssid_len) != ESP_OK) {
        snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "%s", CONFIG_BRIDGE_SOFTAP_SSID);
    }

    size_t softap_psw_len = sizeof(wifi_config.ap.password);
    if (esp_mesh_lite_get_softap_psw_from_nvs((char *)wifi_config.ap.password, &softap_psw_len) != ESP_OK) {
        strlcpy((char *)wifi_config.ap.password, CONFIG_BRIDGE_SOFTAP_PASSWORD, sizeof(wifi_config.ap.password));
    }

    ESP_LOGW("DHS","Mesh-WLAN: %s %s",(char *)wifi_config.ap.ssid,(char *)wifi_config.ap.password);

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_bridge_wifi_set(WIFI_MODE_AP, (char *)wifi_config.ap.ssid, (char *)wifi_config.ap.password, NULL));

    esp_mesh_lite_config_t mesh_lite_config = ESP_MESH_LITE_DEFAULT_INIT();

    id = Config->read( (char*)"MeshID",77);
    mesh_lite_config.mesh_id = id;

    esp_mesh_lite_init(&mesh_lite_config);

    esp_mesh_lite_set_mesh_id( mesh_lite_config.mesh_id,true); // set id and force NVS update

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_lite_msg_action_list_register(node_action));

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &ip_event_handler2, NULL));

	DHS::event_handler <char> MeshoutHandler([this](char c)  { // mesh char output handler function
		if (c == 255) return; // no character given
		ESP_LOGI("DHS", "Send %c to mesh",c);
		char s[2]={c,0}; // build string from char
		broadcast(s); // send char to all local clients
	});
	MeshoutHandler.setType(DHS_RemoteHandler); // remote handler type
	outHandlerId = Controller->outHandlers += MeshoutHandler;
}

void DHS_Mesh_lite::send_empty() { // send empty message to root for time measurement
  uint8_t sta_mac[6] = {0};
  char mac_str[MAC_MAX_LEN];
  cJSON* item = NULL;
  item = cJSON_CreateObject();

  cJSON_AddStringToObject(item, "send", "");

  esp_wifi_get_mac((wifi_interface_t)ESP_IF_WIFI_STA, sta_mac);
  snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(sta_mac));
  cJSON_AddStringToObject(item, "mac", mac_str);

  cJSON_AddNumberToObject(item, "time", (double)timer_u32());
  cJSON_AddBoolToObject(item, "fromRoot", false);
  esp_mesh_lite_try_sending_msg((char*)"rec", (char*)"rec_ack", 1, item, &esp_mesh_lite_send_msg_to_parent);
}

esp_err_t DHS_Mesh_lite::broadcast(char* buffer)
{
    uint8_t sta_mac[6] = {0};
    char mac_str[MAC_MAX_LEN];
	cJSON* item = NULL;
    item = cJSON_CreateObject();

    esp_wifi_get_mac((wifi_interface_t)ESP_IF_WIFI_STA, sta_mac);
    snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(sta_mac));
    cJSON_AddStringToObject(item, "mac", mac_str);
    cJSON_AddNumberToObject(item, "time", (double)timer_u32());

	uint8_t l = esp_mesh_lite_get_level();

    cJSON_AddStringToObject(item, "send", buffer);
    if (l > 1) { // client
      cJSON_AddBoolToObject(item, "fromRoot", false);
      ESP_LOGI("DHS", "send to root from %s",mac_str);
      esp_mesh_lite_try_sending_msg((char*)"rec", (char*)"rec_ack", 1, item, &esp_mesh_lite_send_msg_to_parent);
    }
    if (l == 1) { // root
       cJSON_AddBoolToObject(item, "fromRoot", true);
       ESP_LOGI("DHS", "send to childs from %s",mac_str);
      esp_mesh_lite_try_sending_msg((char*)"rec",(char*)"rec_ack", 1, item, &esp_mesh_lite_send_broadcast_msg_to_child);
    }
    cJSON_Delete(item);
    return ESP_OK;
}
