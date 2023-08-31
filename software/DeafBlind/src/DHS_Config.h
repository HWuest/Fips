/*
 * DHSConfig.h
 *
 *  Created on: 13.05.2023
 *      Author: HW
 *
 * class to handle NVS configuration parameters
 * Usage example:
 * Initializing NVS-Namespace: DHS_Config Conf((char*)"DHS_Conf");
 * Reading parameter, default -1: int8_t p = Conf.read((char*)"Test",(int8_t)-1));
 * Writing parameter: Conf.write((char*)"Test",(int8_t) 123);
 * Erase   prameter:  Conf.erase((char*)"Test");
 * Erase Namspace:    Conf.erase_all();
 *
 */

#ifndef DHS_CONFIG_H_
#define DHS_CONFIG_H_

#include "nvs_handle.hpp"
#include "esp_log.h"
#include <cstring>


class DHS_Config {
public:
	DHS_Config(char* name); // initialize NVS partition with namespace name

	template<typename T> T read(char* param, T def) { // read int32 param from NVS, if not in NVS take def value instead
		T value = def; // value will be def, if not set yet in NVS, def determines type of value!
		esp_err_t err = handle->get_item(param, value);
		if (err == ESP_ERR_NVS_NOT_FOUND) ESP_LOGW("DHS", "Could not read %s from NVS, used default",param);
		return value;
	}
	template<typename T> void write(char* param, T value) { // write int32 param to NVS
		esp_err_t err = handle->set_item(param, value);
		if (err != ESP_OK) ESP_LOGW("DHS", "Could not write %s to NVS Storage",param);
		err = handle->commit();
		if (err != ESP_OK) ESP_LOGW("DHS", "Could not commit NVS Storage change");
	}

	void read_string(const char *key, char* str, char* def) {
		esp_err_t err =  handle->get_string(key, str, 50);
		if (err == ESP_ERR_NVS_NOT_FOUND) {
			ESP_LOGW("DHS", "Could not read %s from NVS, used default",key);
			strcpy(str,def);
		}
	}

	void write_string(const char *key, char* str) {
		esp_err_t err = handle->set_string(key,str);
		if (err != ESP_OK) ESP_LOGW("DHS", "Could not write %s to NVS Storage",key);
		err = handle->commit();
		if (err != ESP_OK) ESP_LOGW("DHS", "Could not commit NVS Storage change");
	}

	void erase(char* param) { esp_err_t err = handle->erase_item(param); if (err != ESP_OK) ESP_LOGW("DHS", "Could not erase %s in NVS Storage",param); }
	void erase_all() { esp_err_t err = handle->erase_all(); if (err != ESP_OK) ESP_LOGW("DHS", "Could not erase NVS Storage"); }

private:
	std::unique_ptr<nvs::NVSHandle> handle;
};

#endif /* DHS_CONFIG_H_ */
