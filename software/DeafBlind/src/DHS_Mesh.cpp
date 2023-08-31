/*
 * DHSMesh.cpp
 *
 *  Created on: 14.05.2023
 *      Author: HW
 */

#include "DHS_Mesh.h"
#include "DHS_WebsocketClient.h"

#include "esp_mac.h"
#include "mesh_netif.h"

DHS_1Tick_Control *DHS_Mesh::Controller;

uint8_t DHS_Mesh::channel;

uint8_t MESH_ID[6] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0x00}; // mesh id, MESH_ID[5] read from MeshID Config parameter

int DHS_Mesh::mesh_layer = -1;
mesh_addr_t DHS_Mesh::mesh_parent_addr;
esp_ip4_addr_t DHS_Mesh::s_current_ip;

DHS_Mesh::DHS_Mesh(DHS_1Tick_Control *Cont, DHS_Config *Conf) {

	Controller = Cont; // set one tick controller
	Config = Conf;

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_init());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_loop_create_default());
    ESP_ERROR_CHECK_WITHOUT_ABORT(mesh_netifs_init(recv_cb));
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_init(&config));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &ip_event_handler2, NULL));

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_init());

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_set_max_layer(CONFIG_MESH_MAX_LAYER));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_set_ap_assoc_expire(10));
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    MESH_ID[5] = Config->read((char*)"MeshID", (int) 0);
    memcpy((uint8_t*)&cfg.mesh_id, MESH_ID, 6);

    channel = Config->read((char*)"MeshChannel",(int)CONFIG_MESH_CHANNEL);
    cfg.channel = channel;
    cfg.allow_channel_switch = true;
    Config->read_string("ssid",(char*)cfg.router.ssid, CONFIG_MESH_ROUTER_SSID);
    Config->read_string("pw", (char*)cfg.router.password, CONFIG_MESH_ROUTER_PASSWD);
    cfg.router.ssid_len = strlen((char*)cfg.router.ssid);
    printf("WLAN %s %s\n",cfg.router.ssid,cfg.router.password);

    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode((wifi_auth_mode_t)CONFIG_MESH_AP_AUTHMODE));
    cfg.mesh_ap.max_connection = CONFIG_MESH_AP_CONNECTIONS;
    cfg.mesh_ap.nonmesh_max_connection = CONFIG_MESH_NON_MESH_AP_CONNECTIONS;
    memcpy((uint8_t *) &cfg.mesh_ap.password, (char*)CONFIG_MESH_AP_PASSWD, strlen(CONFIG_MESH_AP_PASSWD)+1);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_set_config(&cfg));

    printf("Mesh starting on ID %d Channel %d...\n",MESH_ID[5],cfg.channel);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_start());

    ESP_LOGI("DHS", "mesh starts successfully, heap:%ld, %s\n",  esp_get_free_heap_size(),
             esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed");

    mesh_addr_t GroupID = { .addr = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF }};
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_set_group_id(&GroupID,1)); // set group ID for broadcast messages

	DHS::event_handler <char> MeshoutHandler([this](char c)  { // mesh char output handler function
		if (c == 255) return; // no character given
		ESP_LOGI("DHS", "Send %c to mesh",c);
		broadcast(&c,1); // send char to all lokal clients
	});
	MeshoutHandler.setType(DHS_RemoteHandler); // remote handler type
	outHandlerId = Controller->outHandlers += MeshoutHandler;
}

/*
//#define ROOTER
#define ROOTERX
#define SENDROOTER

DHS_Mesh::DHS_Mesh(DHS_1Tick_Control *Cont, DHS_Config *Conf) {

	Controller = Cont; // set one tick controller
	Config = Conf;
	esp_log_level_set("wifi", ESP_LOG_DEBUG);
	esp_log_level_set("mesh", ESP_LOG_DEBUG);

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_init());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_loop_create_default());
    ESP_ERROR_CHECK_WITHOUT_ABORT(mesh_netifs_init(recv_cb));
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_init(&config));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &ip_event_handler2, NULL));

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_storage(WIFI_STORAGE_RAM));
#ifdef ROOTER
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_AP));
#else
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_APSTA));
#endif
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_init());

    esp_mesh_fix_root(true);
#ifdef ROOTER
    esp_mesh_set_type(MESH_ROOT);
#endif

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_set_max_layer(CONFIG_MESH_MAX_LAYER));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_set_ap_assoc_expire(10));
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    MESH_ID[5] = Config->read((char*)"MeshID", (int) 0);
    memcpy((uint8_t*)&cfg.mesh_id, MESH_ID, 6);

    channel = Config->read((char*)"MeshChannel",(int)CONFIG_MESH_CHANNEL);
    cfg.channel = channel;
    cfg.allow_channel_switch = true;
#ifdef ROOTERX
    Config->read_string("ssid",(char*)cfg.router.ssid, CONFIG_MESH_ROUTER_SSID);
//    Config->read_string("pw", (char*)cfg.router.password, CONFIG_MESH_ROUTER_PASSWD);
    cfg.router.ssid_len = strlen((char*)cfg.router.ssid);
    printf("WLAN %s %s\n",cfg.router.ssid,cfg.router.password);
#endif
    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode((wifi_auth_mode_t)CONFIG_MESH_AP_AUTHMODE));
    cfg.mesh_ap.max_connection = CONFIG_MESH_AP_CONNECTIONS;
    cfg.mesh_ap.nonmesh_max_connection = CONFIG_MESH_NON_MESH_AP_CONNECTIONS;
    memcpy((uint8_t *) &cfg.mesh_ap.password, (char*)CONFIG_MESH_AP_PASSWD, strlen(CONFIG_MESH_AP_PASSWD)+1);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_set_config(&cfg));

    printf("Mesh starting on ID %d Channel %d...\n",MESH_ID[5],cfg.channel);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_start());

//esp_mesh_set_self_organized(true, false);
    ESP_LOGI("DHS", "mesh starts successfully, heap:%ld, %s\n",  esp_get_free_heap_size(),
             esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed");

    mesh_addr_t GroupID = { .addr = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF }};
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_set_group_id(&GroupID,1)); // set group ID for broadcast messages

	DHS::event_handler <char> MeshoutHandler([this](char c)  { // mesh char output handler function
		if (c == 255) return; // no character given
		ESP_LOGI("DHS", "Send %c to mesh",c);
		broadcast(&c,1); // send char to all lokal clients
	});
	MeshoutHandler.setType(DHS_RemoteHandler); // remote handler type
	outHandlerId = Controller->outHandlers += MeshoutHandler;
}
*/

void DHS_Mesh::recv_cb(mesh_addr_t *from, mesh_data_t *data) {
	mesh_addr_t own_addr;
	esp_wifi_get_mac(WIFI_IF_STA, own_addr.addr);

	ESP_LOGI("DHS", "Recieved %c from " MACSTR,data->data[0], MAC2STR(from->addr));

    if (MAC_ADDR_EQUAL(from->addr, own_addr.addr)) return; //Skip own Brodcast recieve !!!
    else ESP_LOGI("DHS", "Processing...");

    if (data->size == 1) DHS_Mesh::Controller->inHandler(data->data[0],DHS_LocalHandler | DHS_WebClientHandler | DHS_CommandHandler); // call tickRecHandler if single char recieved
}

#ifndef SENDROOTER
esp_err_t DHS_Mesh::broadcast(void *buffer, size_t len)
{
    static const uint8_t eth_broadcast[MAC_ADDR_LEN] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
    mesh_addr_t dest_addr;
    mesh_data_t data;
    memcpy(dest_addr.addr, eth_broadcast, MAC_ADDR_LEN);
    data.data = (uint8_t*) buffer;
    data.size = len;
    data.proto = MESH_PROTO_BIN;
    data.tos = MESH_TOS_P2P;

    esp_err_t err = esp_mesh_send(&dest_addr, &data, MESH_DATA_GROUP, NULL, 0); // broadcast character

    return ESP_OK;
}
#else
esp_err_t DHS_Mesh::broadcast(void *buffer, size_t len)
{
    mesh_data_t data;
    data.data = (uint8_t*) buffer;
    data.size = len;
    data.proto = MESH_PROTO_BIN;
    data.tos = MESH_TOS_P2P;

    esp_err_t err = esp_mesh_send(0, &data, MESH_DATA_TODS, NULL, 1); // send to root

    return ESP_OK;
}
#endif

void mesh_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    mesh_addr_t id = {0,};
    static uint8_t last_layer = 0;

    switch (event_id) {
    case MESH_EVENT_STARTED: {
        esp_mesh_get_id(&id);
        ESP_LOGI("DHS", "<MESH_EVENT_MESH_STARTED>ID:"MACSTR"", MAC2STR(id.addr));
        DHS_Mesh::mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_STOPPED: {
        ESP_LOGI("DHS", "<MESH_EVENT_STOPPED>");
        DHS_Mesh::mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_CHILD_CONNECTED: {
        mesh_event_child_connected_t *child_connected = (mesh_event_child_connected_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_CHILD_CONNECTED>aid:%d, "MACSTR"",
                 child_connected->aid,
                 MAC2STR(child_connected->mac));
    }
    break;
    case MESH_EVENT_CHILD_DISCONNECTED: {
        mesh_event_child_disconnected_t *child_disconnected = (mesh_event_child_disconnected_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_CHILD_DISCONNECTED>aid:%d, "MACSTR"",
                 child_disconnected->aid,
                 MAC2STR(child_disconnected->mac));
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_ADD: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_ROUTING_TABLE_ADD>add %d, new:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new);
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_REMOVE: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_ROUTING_TABLE_REMOVE>remove %d, new:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new);
    }
    break;
    case MESH_EVENT_NO_PARENT_FOUND: {
        mesh_event_no_parent_found_t *no_parent = (mesh_event_no_parent_found_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_NO_PARENT_FOUND>scan times:%d",
                 no_parent->scan_times);
    }
    /* TODO handler for the failure */
    break;
    case MESH_EVENT_PARENT_CONNECTED: {
        mesh_event_connected_t *connected = (mesh_event_connected_t *)event_data;
        esp_mesh_get_id(&id);
        DHS_Mesh::mesh_layer = connected->self_layer;
        memcpy(&DHS_Mesh::mesh_parent_addr.addr, connected->connected.bssid, 6);
        ESP_LOGI("DHS",
                 "<MESH_EVENT_PARENT_CONNECTED>layer:%d-->%d, parent:"MACSTR"%s, ID:"MACSTR"",
                 last_layer, DHS_Mesh::mesh_layer, MAC2STR(DHS_Mesh::mesh_parent_addr.addr),
                 esp_mesh_is_root() ? "<ROOT>" :
                 (DHS_Mesh::mesh_layer == 2) ? "<layer2>" : "", MAC2STR(id.addr));
        last_layer = DHS_Mesh::mesh_layer;
        mesh_netifs_start(esp_mesh_is_root());

    }
    break;
    case MESH_EVENT_PARENT_DISCONNECTED: {
        mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event_data;
        ESP_LOGI("DHS",
                 "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d",
                 disconnected->reason);
        DHS_Mesh::mesh_layer = esp_mesh_get_layer();
        mesh_netifs_stop();
        if (esp_mesh_is_root()) DHS_WebsocketClient::stop(); // stop websocket client
    }
    break;
    case MESH_EVENT_LAYER_CHANGE: {
        mesh_event_layer_change_t *layer_change = (mesh_event_layer_change_t *)event_data;
        DHS_Mesh::mesh_layer = layer_change->new_layer;
        ESP_LOGI("DHS", "<MESH_EVENT_LAYER_CHANGE>layer:%d-->%d%s",
                 last_layer, DHS_Mesh::mesh_layer,
                 esp_mesh_is_root() ? "<ROOT>" :
                 (DHS_Mesh::mesh_layer == 2) ? "<layer2>" : "");
        last_layer = DHS_Mesh::mesh_layer;
    }
    break;
    case MESH_EVENT_ROOT_ADDRESS: {
        mesh_event_root_address_t *root_addr = (mesh_event_root_address_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_ROOT_ADDRESS>root address:"MACSTR"",
                 MAC2STR(root_addr->addr));
    }
    break;
    case MESH_EVENT_VOTE_STARTED: {
        mesh_event_vote_started_t *vote_started = (mesh_event_vote_started_t *)event_data;
        ESP_LOGI("DHS",
                 "<MESH_EVENT_VOTE_STARTED>attempts:%d, reason:%d, rc_addr:"MACSTR"",
                 vote_started->attempts,
                 vote_started->reason,
                 MAC2STR(vote_started->rc_addr.addr));
    }
    break;
    case MESH_EVENT_VOTE_STOPPED: {
        ESP_LOGI("DHS", "<MESH_EVENT_VOTE_STOPPED>");
        break;
    }
    case MESH_EVENT_ROOT_SWITCH_REQ: {
        mesh_event_root_switch_req_t *switch_req = (mesh_event_root_switch_req_t *)event_data;
        ESP_LOGI("DHS",
                 "<MESH_EVENT_ROOT_SWITCH_REQ>reason:%d, rc_addr:"MACSTR"",
                 switch_req->reason,
                 MAC2STR( switch_req->rc_addr.addr));
    }
    break;
    case MESH_EVENT_ROOT_SWITCH_ACK: {
        /* new root */
        DHS_Mesh::mesh_layer = esp_mesh_get_layer();
        esp_mesh_get_parent_bssid(&DHS_Mesh::mesh_parent_addr);
        ESP_LOGI("DHS", "<MESH_EVENT_ROOT_SWITCH_ACK>layer:%d, parent:"MACSTR"", DHS_Mesh::mesh_layer, MAC2STR(DHS_Mesh::mesh_parent_addr.addr));
    }
    break;
    case MESH_EVENT_TODS_STATE: {
        mesh_event_toDS_state_t *toDs_state = (mesh_event_toDS_state_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_TODS_REACHABLE>state:%d", *toDs_state);
    }
    break;
    case MESH_EVENT_ROOT_FIXED: {
        mesh_event_root_fixed_t *root_fixed = (mesh_event_root_fixed_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_ROOT_FIXED>%s",
                 root_fixed->is_fixed ? "fixed" : "not fixed");
    }
    break;
    case MESH_EVENT_ROOT_ASKED_YIELD: {
        mesh_event_root_conflict_t *root_conflict = (mesh_event_root_conflict_t *)event_data;
        ESP_LOGI("DHS",
                 "<MESH_EVENT_ROOT_ASKED_YIELD>"MACSTR", rssi:%d, capacity:%d",
                 MAC2STR(root_conflict->addr),
                 root_conflict->rssi,
                 root_conflict->capacity);
    }
    break;
    case MESH_EVENT_CHANNEL_SWITCH: {
        mesh_event_channel_switch_t *channel_switch = (mesh_event_channel_switch_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_CHANNEL_SWITCH>new channel:%d", channel_switch->channel);
    }
    break;
    case MESH_EVENT_SCAN_DONE: {
        mesh_event_scan_done_t *scan_done = (mesh_event_scan_done_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_SCAN_DONE>number:%d",
                 scan_done->number);
    }
    break;
    case MESH_EVENT_NETWORK_STATE: {
        mesh_event_network_state_t *network_state = (mesh_event_network_state_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_NETWORK_STATE>is_rootless:%d",
                 network_state->is_rootless);
    }
    break;
    case MESH_EVENT_STOP_RECONNECTION: {
        ESP_LOGI("DHS", "<MESH_EVENT_STOP_RECONNECTION>");
    }
    break;
    case MESH_EVENT_FIND_NETWORK: {
        mesh_event_find_network_t *find_network = (mesh_event_find_network_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_FIND_NETWORK>new channel:%d, router BSSID:"MACSTR"",
                 find_network->channel, MAC2STR(find_network->router_bssid));
    }
    break;
    case MESH_EVENT_ROUTER_SWITCH: {
        mesh_event_router_switch_t *router_switch = (mesh_event_router_switch_t *)event_data;
        ESP_LOGI("DHS", "<MESH_EVENT_ROUTER_SWITCH>new router:%s, channel:%d, "MACSTR"",
                 router_switch->ssid, router_switch->channel, MAC2STR(router_switch->bssid));
    }
    break;
    default:
        ESP_LOGI("DHS", "unknown id:%ld", event_id);
        break;
    }
}

void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    printf("Device IP is: " IPSTR "\n", IP2STR(&event->ip_info.ip));
    DHS_Mesh::s_current_ip.addr = event->ip_info.ip.addr;
#if !CONFIG_MESH_USE_GLOBAL_DNS_IP
    esp_netif_t *netif = event->esp_netif;
    esp_netif_dns_info_t dns;
    ESP_ERROR_CHECK(esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));
    mesh_netif_start_root_ap(esp_mesh_is_root(), dns.ip.u_addr.ip4.addr);
#endif

    if (esp_mesh_is_root()) DHS_WebsocketClient::start(); // start websocket client
}

void ip_event_handler2(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
   esp_wifi_connect(); // try reconnection
}
