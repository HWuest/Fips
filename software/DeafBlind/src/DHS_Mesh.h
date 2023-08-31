/*
 * DHSMesh.h
 *
 *  Created on: 14.05.2023
 *      Author: HW
 *
 *  Mesh-Network class to build and control Espressif Mesh-Network
 */

#ifndef DHS_MESH_H_
#define DHS_MESH_H_

#include "DHS_1Tick_Control.h"
#include "DHS_Config.h"

#include "esp_mesh.h"
#include "esp_log.h"

class DHS_Mesh {
public:
	mesh_addr_t s_route_table[CONFIG_MESH_ROUTE_TABLE_SIZE] = { 0 };
	static uint8_t channel;

	static int mesh_layer;
	static mesh_addr_t mesh_parent_addr;
	static esp_ip4_addr_t s_current_ip;

    static DHS_1Tick_Control *Controller; // 1Tick_Controller pointer for callback
    DHS_Config *Config;

    DHS_Mesh(DHS_1Tick_Control *Cont, DHS_Config *Conf);

	esp_err_t broadcast(void *buffer, size_t len);
	void static recv_cb(mesh_addr_t *from, mesh_data_t *data);

private:
    int outHandlerId;
};

void mesh_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void ip_event_handler2(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

#endif /* DHS_MESH_H_ */
