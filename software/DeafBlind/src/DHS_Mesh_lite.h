/*
 * DHSMesh.h
 *
 *  Created on: 14.05.2023
 *      Author: HW
 *
 *  Mesh-Network class to build and control Espressif Mesh-Network
 */

#ifndef DHS_MESH_LITE_H_
#define DHS_MESH_LITE_H_

#include "DHS_1Tick_Control.h"
#include "DHS_Config.h"

#include "esp_mesh_lite.h"

class DHS_Mesh_lite {
public:
    static DHS_1Tick_Control *Controller; // 1Tick_Controller pointer for callback
    DHS_Config *Config;
    static int id;

    DHS_Mesh_lite(DHS_1Tick_Control *Cont, DHS_Config *Conf);

    esp_err_t broadcast(char* buffer);
    void send_empty(void);

private:
    int outHandlerId;
};

#endif /* DHS_MESH_LITE_H_ */
