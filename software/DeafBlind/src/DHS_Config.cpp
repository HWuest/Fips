/*
 * DHSConfig.cpp
 *
 *  Created on: 13.05.2023
 *      Author: HW
 */

#include "DHS_Config.h"

#include "esp_system.h"
#include "nvs_flash.h"

DHS_Config::DHS_Config(char* name) {
//    ESP_ERROR_CHECK(nvs_flash_erase());
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    // Handle will automatically close when going out of scope or when it's reset.
    handle = nvs::open_nvs_handle(name, NVS_READWRITE, &err);
    if (err != ESP_OK) ESP_LOGW("DHS", "Could not open NVS Storage");
}
