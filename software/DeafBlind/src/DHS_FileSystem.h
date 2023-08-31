/*
 * DHSFileSystem.h
 *
 *  Created on: 12.05.2023
 *      Author: HW
 */

#ifndef DHS_FILESYSTEM_H_
#define DHS_FILESYSTEM_H_

#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include "esp_vfs_fat.h"

class DHS_FileSystem {
public:
	char *base_path = (char*)"/spiflash";
	FILE *currentFile;

	DHS_FileSystem();
	virtual ~DHS_FileSystem();

	FILE* open_write(char* Filename);
	FILE* open_read(char* Filename);
	inline void close() { fclose(currentFile);};
	void read_line(char* str, size_t max);
    inline char read_char() { char c; if (!read(fileno(currentFile), &c, 1)) c=255; return c; };

private:
	wl_handle_t s_wl_handle;
};

#endif /* DHS_FILESYSTEM_H_ */
