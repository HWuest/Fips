/*
 * DHSFileSystem.cpp
 *
 *  Created on: 12.05.2023
 *      Author: HW
 */

#include "DHS_FileSystem.h"

DHS_FileSystem::DHS_FileSystem() {

    ESP_LOGI("DHS", "Mounting FAT filesystem");
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formatted before
    s_wl_handle = WL_INVALID_HANDLE;
    currentFile = NULL;

    const esp_vfs_fat_mount_config_t mount_config = {
            .format_if_mount_failed = true,
			.max_files = 5,
            .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
			.disk_status_check_enable = false
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE("DHS", "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}

DHS_FileSystem::~DHS_FileSystem() {

    ESP_LOGI("DHS", "Unmounting FAT filesystem");
    ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount_rw_wl(base_path, s_wl_handle));
}

FILE* DHS_FileSystem::open_write(char* Filename) {
    char full_fn[128];
    strlcpy( full_fn, base_path, strlen(base_path)+1); // add base_path
    full_fn[strlen(base_path)]='/'; // with slash
    strlcpy( &full_fn[strlen(base_path)+1], Filename, 128-strlen(base_path)); // to Filename

    ESP_LOGI("DHS", "Writing file %s", full_fn);
    currentFile = fopen(full_fn, "wb");
    if (currentFile == NULL) {
        ESP_LOGE("DHS", "Failed to open file for writing");
        return NULL;
    }
    return currentFile;
}

FILE* DHS_FileSystem::open_read(char* Filename) {
    char full_fn[128];

    strlcpy( full_fn,base_path,strlen(base_path)+1); // add base_path
    full_fn[strlen(base_path)]='/'; // with slash
    strlcpy( &full_fn[strlen(base_path)+1],Filename,128-strlen(base_path)); // to Filename

    ESP_LOGI("DHS", "Reading file %s",full_fn);
    currentFile = fopen(full_fn, "rb");
	if (currentFile == NULL) {
		ESP_LOGE("DHS", "Failed to open file for reading");
        return NULL;
	}
    return currentFile;
}
void DHS_FileSystem::read_line(char* str, size_t max) {

	fgets(str, max, currentFile);

	// remove /n character from line
	size_t sl = strlen(str);
    if(sl > 0 && str[sl - 1] == '\n') str[sl - 1] = '\0';
}

