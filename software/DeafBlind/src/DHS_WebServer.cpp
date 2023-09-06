/*
 * DHS_WebServer.cpp
 *
 *  Created on: 16.05.2023
 *      Author: HW
 */

#include "DHS_WebServer.h"
#include "DHS_GPIO.h"
#include "DHS_WebsocketClient.h"
#include "DHS_TickCommand.h"
#include "DHS_LCD.h"
#include "DHS_LCDklein.h"
#include "DHS_Mesh_lite.h"

#include <cstddef>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include <dirent.h>
#include "mdns.h"
#include "esp_mesh.h"
#include "esp_wifi_types.h"

#include "esp_ota_ops.h"

DHS_1Tick_Control *DHS_WebServer::Controler;
DHS_TickCommand   *DHS_WebServer::Command;

struct file_server_data {
    char base_path[ESP_VFS_PATH_MAX + 1]; //	    /* Base path of file storage */
    char scratch[SCRATCH_BUFSIZE];
    DHS_1Tick_Control *Controler;
};

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (2*1024*1024) // 2 MB
#define MAX_FILE_SIZE_STR "2MB"


/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
static esp_err_t redirect_handler(httpd_req_t *req, const char *path)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", path);
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path.
 * In case of SPIFFS this returns empty list when path is any
 * string other than '/', since SPIFFS doesn't support directories */
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    const char *entrytype;

    struct dirent *entry;
    struct stat entry_stat;

    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    /* Retrieve the base path of file storage to construct the full path */
    strlcpy(entrypath, dirpath, sizeof(entrypath));

    if (!dir) {
        ESP_LOGE("DHS", "Failed to stat dir : %s", dirpath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

    /* Send HTML file header */
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

    /* Get handle to embedded file upload script */
    extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);

    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);

    /* Send file-list table definition and column labels */
    httpd_resp_sendstr_chunk(req,
        "<table class=\"fixed\" border=\"1\">"
        "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
        "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
        "<tbody>");

    /* Iterate over all files / folders and fetch their names and sizes */
    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE("DHS", "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        ESP_LOGI("DHS", "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

        /* Send chunk of HTML file containing table entries with file name and size */
        httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
//        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        if (entry->d_type == DT_DIR) {
            httpd_resp_sendstr_chunk(req, "/");
        }
        httpd_resp_sendstr_chunk(req, "\">");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "</a></td><td>");
        httpd_resp_sendstr_chunk(req, entrytype);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, entrysize);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete/");
//        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");
        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
    }
    closedir(dir);

    /* Finish the file list table */
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</body></html>");

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
/*
	  else if (path.endsWith(".json")) _contentType = "application/json";
	  else if (path.endsWith(".xml")) _contentType = "text/xml";
	  else if (path.endsWith(".zip")) _contentType = "application/zip";
	  else if(path.endsWith(".gz")) _contentType = "application/x-gzip";
*/

	if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".htm")) {
            return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".jpg")) {
            return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    } else if (IS_FILE_EXT(filename, ".gif")) {
            return httpd_resp_set_type(req, "image/gif");
    } else if (IS_FILE_EXT(filename, ".png")) {
            return httpd_resp_set_type(req, "image/png");
    } else if (IS_FILE_EXT(filename, ".js")) {
            return httpd_resp_set_type(req, "application/javascript");
    } else if (IS_FILE_EXT(filename, ".pdf")) {
            return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".css")) {
        return httpd_resp_set_type(req, "text/css");
    } else if (IS_FILE_EXT(filename, ".ttf")) {
            return httpd_resp_set_type(req, "font/ttf");
    } else if (IS_FILE_EXT(filename, ".svg")) {
         return httpd_resp_set_type(req, "image/svg+xml");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/* Handler to download a file kept on the server */
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
    if (!filename) {
        ESP_LOGE("DHS", "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

	ESP_LOGI("DHS", "Requested file : %s %s", filepath, filename);

    /* If name has trailing '/', respond with directory contents */
    if (filename[strlen(filename) - 1] == '/') {
    	if (strlen(filename) == 1) return redirect_handler(req, "/index.html"); // filename = "/", root folder, redirect to index.html
 //       return http_resp_dir_html(req, filepath); // to be changed
    }

    set_content_type_from_file(req, filename);

    if (stat(filepath, &file_stat) == -1) {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
        if (strcmp(filename, "/Config.html") == 0) {
            return http_resp_dir_html(req, "/spiflash/"); // file up/download on /config.html
        } else if (strcmp(filename, "/index.html") == 0) {
            return http_resp_dir_html(req, "/spiflash/"); // file up/download on /index.html
        } else if (strcmp(filename, "/favicon.ico") == 0) {
            return favicon_get_handler(req);
        }
        strcat(filepath,(char*)".gz");
        if (stat(filepath, &file_stat) == -1) {
        	ESP_LOGW("DHS", "Failed to open file (with or without .gz): %s", filepath);
        	/* Respond with 404 Not Found */
        	httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        	return ESP_FAIL;
        }
        else httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE("DHS", "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI("DHS", "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE("DHS", "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
               return ESP_FAIL;
           }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI("DHS", "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler to upload a file onto the server */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE("DHS", "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGI("DHS", "File %s already exists, deleting...", filepath);
        unlink(filepath);
    }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE("DHS", "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE("DHS", "Failed to create file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI("DHS", "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        ESP_LOGI("DHS", "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            ESP_LOGE("DHS", "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            ESP_LOGE("DHS", "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    ESP_LOGI("DHS", "File reception complete");

    /* No data response ok*/
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
//    httpd_resp_set_hdr(req, "Location", "/index.html?Config");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send(req, filename, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Handler to delete a file from the server */
static esp_err_t delete_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    /* Skip leading "/delete" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri  + sizeof("/delete") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE("DHS", "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE("DHS", "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI("DHS", "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}

/* Handler to upload a file onto the server */
static esp_err_t fw_update_handler(httpd_req_t *req)
{
    ESP_LOGW("DHS", "Receiving fw-update");

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;
    if (remaining == 0) return ESP_FAIL;

    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t handle;
    esp_err_t err = esp_ota_begin(partition, remaining, &handle);
    if (err) return ESP_FAIL;
    printf("Download running...\n");
    while (remaining > 0) {
    	char* buf;
    	buf = (char*) malloc(2048);
    	const int received = httpd_req_recv(req, buf, MIN(remaining, 2048));
    	if (received <= 0) {
    		if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
    		esp_ota_abort(handle);
    		return ESP_FAIL;
    	}
    	err = esp_ota_write(handle, buf, received);
    	if (err) {
    		esp_ota_abort(handle);
    		return ESP_FAIL;
    	}
    	remaining -= received;
    }
    esp_ota_end(handle);
    if (err) return ESP_FAIL;

    ESP_LOGW("DHS", "FW reception complete, restart to activate new FW!");
    err = esp_ota_set_boot_partition(partition);
    if (err) return ESP_FAIL;

    /* No data response ok*/
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_set_type(req, HTTPD_TYPE_TEXT);

#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send(req, "FW Update Ok", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t reset_handler(httpd_req_t *req)
{
    ESP_LOGW("DHS", "Resetting in 1 sec...");
    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send(req, "Reset done, reopen page after about 20 seconds...", HTTPD_RESP_USE_STRLEN);

    vTaskDelay(1000/portTICK_PERIOD_MS);

    esp_restart();

    return ESP_OK;
}

/////////////////////////////////
//WEBSocket functions
/////////////////////////////////

httpd_handle_t 	DHS_WebServer::server;
httpd_handle_t 	DHS_WebServer::sserver;
/*
#define DHS_max_ringbuf 4
httpd_ws_frame_t ws_pkt[DHS_max_ringbuf]; // defined global to not be discarded at end of async function call
uint8_t ws_buf[DHS_max_ringbuf][100]; // defined global to not be discarded at end of async function call
uint8_t ws_pos = 0;     // ring buf pos to allow several async sends without override of buffer!


struct async_broadcast {
	httpd_ws_frame_t* ws_pkt;
	uint8_t* ws_buf;
	int sourcefd;
} ws_d[DHS_max_ringbuf];

static void ws_async_broadcast(void *arg)
{
	async_broadcast *data = (async_broadcast*) arg;

	memset(data->ws_pkt, 0, sizeof(httpd_ws_frame_t));

	data->ws_pkt->payload = (uint8_t*) data->ws_buf;
	data->ws_pkt->len = strlen((char*)data->ws_buf);
	data->ws_pkt->type = HTTPD_WS_TYPE_TEXT;

    static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[max_clients];
    esp_err_t ret = httpd_get_client_list(DHS_WebServer::server, &fds, client_fds);
    if (ret != ESP_OK) {
        return;
    }
    for (int i = 0; i < fds; i++) {
  	//ESP_LOGI("DHS","async fd%d: %d",i,client_fds[i]);
        int client_info = httpd_ws_get_fd_info(DHS_WebServer::server, client_fds[i]);
        if ((client_info == HTTPD_WS_CLIENT_WEBSOCKET) && (data->sourcefd != client_fds[i]) && (data->clientfd != client_fds[i])) { // send if not own source (prevent send/recieve loop)
            ESP_LOGI("DHS", "Send to client %d: %s", data->sourcefd, data->ws_pkt->payload);
            httpd_ws_send_frame_async(DHS_WebServer::server, client_fds[i], data->ws_pkt);
            httpd_ws_send_frame_async(DHS_WebServer::server, data->sourcefd, data->ws_pkt); // send to open websockets
        }
    }
}
*/

int8_t DHS_WebServer::getNumConnections() {
	static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
	size_t fds = max_clients;
	int client_fds[max_clients];
	esp_err_t ret = httpd_get_client_list(DHS_WebServer::sserver, &fds, client_fds);
	if (ret != ESP_OK) return -1;
	return fds;
}

esp_err_t DHS_WebServer::broadcast(char* data)
{
//    ws_pos = (ws_pos + 1) % DHS_max_ringbuf; // next ringbuf pos.
//    memcpy(ws_buf[ws_pos],data,100); // copy data to persistent buffer
//    ws_d[ws_pos] = { &ws_pkt[ws_pos], ws_buf[ws_pos], Controller->sourcefd }; // DHS, can be initialized earlier
//    return httpd_queue_work(DHS_WebServer::server, ws_async_broadcast, (void*) &ws_d[ws_pos]);
	ESP_LOGI("DHS", "WebServer Broadcast");
	static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[max_clients];
    esp_err_t ret = httpd_get_client_list(DHS_WebServer::sserver, &fds, client_fds);
    if (ret != ESP_OK) return ret;

    for (int i = 0; i < fds; i++) { // https
        int client_info = httpd_ws_get_fd_info(DHS_WebServer::sserver, client_fds[i]);
        if ((client_info == HTTPD_WS_CLIENT_WEBSOCKET) && (Controler->sourcefd != client_fds[i]) && (Controler->serverfd != client_fds[i])) { // send if not own source (prevent send/recieve loop)
            ESP_LOGI("DHS", "Send to https client %d: %s", client_fds[i],data);
            if (ssend(data,client_fds[i])!=ESP_OK) ESP_LOGW("DHS","SSend failed\n");
        }
    }
    fds = max_clients;
    ret = httpd_get_client_list(DHS_WebServer::server, &fds, client_fds);
    if (ret != ESP_OK) return ret;
    for (int i = 0; i < fds; i++) { // http
        int client_info = httpd_ws_get_fd_info(DHS_WebServer::server, client_fds[i]);
        if ((client_info == HTTPD_WS_CLIENT_WEBSOCKET) && (Controler->sourcefd != client_fds[i]) && (Controler->serverfd != client_fds[i])) { // send if not own source (prevent send/recieve loop)
            ESP_LOGI("DHS", "Send to http client %d: %s", client_fds[i],data);
            if (send(data,client_fds[i])!=ESP_OK) ESP_LOGW("DHS","Send failed\n");
        }
    }
    Controler->sourcefd = -1; // clear source fd
    return ret;
}

esp_err_t DHS_WebServer::send(char* data, int fd) {
	httpd_ws_frame f;

	memset(&f, 0, sizeof(httpd_ws_frame_t));

	f.payload = (uint8_t*) data;
	f.len = strlen((char*)data); // no +1 because type string!!!

    for (uint8_t i=0; i< f.len; i++) if (f.payload[i]>128) {
    	for (uint8_t u=i+1; u<f.len; u++) f.payload[u+1]=f.payload[u];
    	f.len++;
    	switch (f.payload[i]) { // handle Umlaute, to be improved
	  	  case '\x84': f.payload[i] = 'a'; f.payload[i+1]='e'; break; //ä
	  	  case '\x94': f.payload[i] = 'o'; f.payload[i+1]='e'; break; //ö
	  	  case '\x81': f.payload[i] = 'u'; f.payload[i+1]='e'; break; //ü
	  	  case '\x8E': f.payload[i] = 'A'; f.payload[i+1]='E'; break; //Ä
	  	  case '\x99': f.payload[i] = 'O'; f.payload[i+1]='E'; break; //Ö
	  	  case '\x9A': f.payload[i] = 'U'; f.payload[i+1]='E'; break; //Ü
    	}
    }

	f.type = HTTPD_WS_TYPE_TEXT;
	ESP_LOGI("DHS","Websocket http send %s to %d\n",f.payload,fd);
    return httpd_ws_send_frame_async(DHS_WebServer::server, fd, &f); // send answer back to http client
}

esp_err_t DHS_WebServer::ssend(char* data, int fd) {
	httpd_ws_frame f;

	memset(&f, 0, sizeof(httpd_ws_frame_t));

	f.payload = (uint8_t*) data;
	f.len = strlen((char*)data); // no +1 because type string!!!

    for (uint8_t i=0; i< f.len; i++) if (f.payload[i]>128) { // handle Umlaute, to be improved
    	for (uint8_t u=i+1; u<f.len; u++) f.payload[u+1]=f.payload[u];
    	f.len++;
    	switch (f.payload[i]) {
	  	  case '\x84': f.payload[i] = 'a'; f.payload[i+1]='e'; break; //ä
	  	  case '\x94': f.payload[i] = 'o'; f.payload[i+1]='e'; break; //ö
	  	  case '\x81': f.payload[i] = 'u'; f.payload[i+1]='e'; break; //ü
	  	  case '\x8E': f.payload[i] = 'A'; f.payload[i+1]='E'; break; //Ä
	  	  case '\x99': f.payload[i] = 'O'; f.payload[i+1]='E'; break; //Ö
	  	  case '\x9A': f.payload[i] = 'U'; f.payload[i+1]='E'; break; //Ü
	  	  case '\xDF': f.payload[i] = 's'; f.payload[i+1]='s'; break; //ß
    	}
    }

    f.type = HTTPD_WS_TYPE_TEXT;
	ESP_LOGI("DHS","Websocket https send %s to %d\n",f.payload,fd);
    return httpd_ws_send_frame_async(DHS_WebServer::sserver, fd, &f); // send answer back to https client
}

int replacechar(char *str, char orig, char rep) {
    char *ix = str;
    int n = 0;
    while((ix = strchr(ix, orig)) != NULL) {
        *ix++ = rep;
        n++;
    }
    return n;
}

extern uint8_t MESH_ID[6];

bool DHS_WebServer::process(char* payload) {
  char s[200]="",tmp[50]="";

  if (strncmp(payload,"get ",4)== 0) { // get command?
      int16_t p=4;
	  while ((int16_t) strlen(payload)-p > 0) { // loop over requests
		  char c = payload[p];
	      switch (c) {
	        case 's': sprintf(tmp,"s %d",DHS_GPIO::stabTime/1000);
	        		  strcat(s,tmp);
	                  break;
	        case 'd': sprintf(tmp,"d %d",DHS_GPIO::pulseDuration);
	        		  strcat(s,tmp);
	                  break;
	        case 't': sprintf(tmp,"t %d",DHS_GPIO::tickMode);
	        		  strcat(s,tmp);
	                  break;
	        case 'x': sprintf(tmp,"x %d",DHS_TickCommand::outSpeed);
	        		  strcat(s,tmp);
	                  break;
	        case 'L': sprintf(tmp,"L %d",DHS_LCD::lcdActive);
	        		  strcat(s,tmp);
	                  break;
	        case 'l': sprintf(tmp,"l %d",DHS_LCDklein::lcdActive);
	        		  strcat(s,tmp);
	                  break;
	        case 'i': sprintf(tmp,"i %s",&DHS_WebsocketClient::uri[6]); // uri without wss://
	        		  tmp[strlen(tmp)-3]=0; // remove /ws
	        		  strcat(s,tmp);
	                  break;
	        case 'I': sprintf(tmp,"I %d",DHS_WebsocketClient::serverConnect);
	        		  strcat(s,tmp);
	        		  break;
	        case 'w': wifi_config_t cfg;
	            	  Command->Config->read_string("ssid",(char*)cfg.sta.ssid, CONFIG_MESH_ROUTER_SSID);
	            	  Command->Config->read_string("pw", (char*)cfg.sta.password, CONFIG_MESH_ROUTER_PASSWD);
	   				  sprintf(tmp,"w %s,%s",cfg.sta.ssid,cfg.sta.password); // wlan credentials ssid,password
	   				  replacechar(tmp+2,' ','~'); // replace space with ~ in content for transfer as one data-item
	        		  strcat(s,tmp);
	                  break;
	        case 'm': sprintf(tmp,"m %d",DHS_Mesh_lite::id);
	        		  strcat(s,tmp);
	        		  break;
	        default:  strcat(s,"c ?"); // not found responde ?
	      }
	      p+=2;
	      strcat(s," ");
	    }
      ESP_LOGI("DHS", "Report to WebSocket command %s: %s", payload,s);
      send(s, Controler->sourcefd); // send answer back to client
      ssend(s, Controler->sourcefd); // send answer back to client
      //      broadcast(s);
	  return true; // no further procesing of data
  }
  return false;
}

#define HTTPD_SCRATCH_BUF  MAX(HTTPD_MAX_REQ_HDR_LEN, HTTPD_MAX_URI_LEN)
struct DHS_aux {
    struct sock_db *sd;                             /*!< Pointer to socket database */
    char            scratch[HTTPD_SCRATCH_BUF + 1]; /*!< Temporary buffer for our operations (1 byte extra for null termination) */
    size_t          remaining_len;                  /*!< Amount of data remaining to be fetched */
    char           *status;                         /*!< HTTP response's status code */
    char           *content_type;                   /*!< HTTP response's content type */
    bool            first_chunk_sent;               /*!< Used to indicate if first chunk sent */
    unsigned        req_hdrs_count;                 /*!< Count of total headers in request packet */
    unsigned        resp_hdrs_count;                /*!< Count of additional headers in response packet */
    struct resp_hdr {
        const char *field;
        const char *value;
    } *resp_hdrs;                                   /*!< Additional headers in response packet */
    struct http_parser_url url_parse_res;           /*!< URL parsing result, used for retrieving URL elements */
    bool ws_handshake_detect;                       /*!< WebSocket handshake detection flag */
    httpd_ws_type_t ws_type;                        /*!< WebSocket frame type */
    bool ws_final;                                  /*!< WebSocket FIN bit (final frame or not) */
    uint8_t mask_key[4];                            /*!< WebSocket mask key for this payload */
};

struct DHS_aux2 {
    int fd;                             /*!< Pointer to fd */
};

esp_err_t wss_open_fd(httpd_handle_t hd, int sockfd)
{
    ESP_LOGW("DHS", "New client connected %d", sockfd);
    return ESP_OK;
}

void wss_close_fd(httpd_handle_t hd, int sockfd)
{
    ESP_LOGW("DHS", "Client disconnected %d", sockfd);
    close(sockfd);
}

static esp_err_t websocket_handler(httpd_req_t *req)
{   struct DHS_aux *r = (DHS_aux*) req->aux; // helper struct to access internal aux element
	struct DHS_aux2 *sd = (DHS_aux2*) r->sd; // helper struct to access internal socket database element
    ESP_LOGI("DHS", "Req-Method: %d aux: %d",req->method,r->ws_type);

    if (req->method == HTTP_GET) {
        ESP_LOGI("DHS", "Handshake done, the new connection was opened FD: %d",sd->fd);
        return ESP_OK;
    }
    if (r->ws_type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGW("DHS", "Close, Client disconnected");
    	//    	if (((file_server_data*)req->user_ctx)->Controller->iClients) ((file_server_data*)req->user_ctx)->Controller->iClients--; // DHS to be extended to check if internet client was closed...
//        ESP_LOGI("DHS", "Connection %d was closed (%d)",sd->fd,((file_server_data*)req->user_ctx)->Controller->iClients);
        /* Read the rest of the CLOSE frame and response */
         /* Please refer to RFC6455 Section 5.5.1 for more details */
         httpd_ws_frame_t frame;
         uint8_t frame_buf[128] = { 0 };
         memset(&frame, 0, sizeof(httpd_ws_frame_t));
         frame.payload = frame_buf;
         if (httpd_ws_recv_frame(req, &frame, 126) != ESP_OK) {
             ESP_LOGD("DHS", "Cannot receive the full CLOSE frame");
             return ESP_ERR_INVALID_STATE;
         }
         frame.len = 0;
         frame.type = HTTPD_WS_TYPE_CLOSE;
         frame.payload = NULL;
//         close(sd->fd);
         return httpd_ws_send_frame(req, &frame);
    }

    if (r->ws_type == HTTPD_WS_TYPE_PING) { // PING Message
    	ESP_LOGI("DHS","Websocket send PONG\n");
    	httpd_ws_frame_t frame;
        uint8_t frame_buf[128] = { 0 };
        memset(&frame, 0, sizeof(httpd_ws_frame_t));
        frame.payload = frame_buf;

        if (httpd_ws_recv_frame(req, &frame, 126) != ESP_OK) {
            ESP_LOGW("DHS", "Cannot receive the full PING frame");
            return ESP_ERR_INVALID_STATE;
        }
        frame.type = HTTPD_WS_TYPE_PONG;
        return httpd_ws_send_frame(req, &frame);
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE("DHS", "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    if (ws_pkt.len) {
      /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
      buf = (uint8_t*) calloc(1, ws_pkt.len + 1);
      if (buf == NULL) {
            ESP_LOGE("DHS", "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
      }
      ws_pkt.payload = buf;
      /* Set max_len = ws_pkt.len to get the frame payload */
      ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
      if (ret != ESP_OK) {
            ESP_LOGE("DHS", "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
      }
      ESP_LOGI("DHS", "WebSocket recieved message: %s", ws_pkt.payload);

      if (ws_pkt.len == 19) { // DHS: to be changed to add and check special command instead of length!
        uint8_t addr[6];
        esp_wifi_get_mac(WIFI_IF_STA, addr);
        char r[20];
        sprintf(r,"%3d%3d%3d%3d%3d%3d",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
        if (strcmp(r, (char*)ws_pkt.payload)==0) {
        	ESP_LOGW("DHS","Own client connection, disable sending on fd %d\n",sd->fd);
            ((file_server_data*)req->user_ctx)->Controler->serverfd = sd->fd; // store aux->sd->fd for disable server broadcast
            DHS_WebsocketClient::stop(); // stop client connection
        }
        else {
            ((file_server_data*)req->user_ctx)->Controler->iClients++;
        	ESP_LOGW("DHS","New client connected, send on fd %d (%d)",sd->fd,((file_server_data*)req->user_ctx)->Controler->iClients);
            DHS_WebServer::send((char*)"tick +", sd->fd); // connected
            DHS_WebServer::ssend((char*)"tick +", sd->fd); // connected
            }
        free(buf);
        return ret;
      }
      ((file_server_data*)req->user_ctx)->Controler->sourcefd = sd->fd; // store aux->sd->fd for disable self broadcast
      if (ws_pkt.len == 2) { // special 2-byte character recieved

    	  if (ws_pkt.payload[0]==195) switch (ws_pkt.payload[1]) {
    	  	  case 164: ((char*)ws_pkt.payload)[0] = '\x84'; break; //ä
    	  	  case 182: ((char*)ws_pkt.payload)[0] = '\x94'; break; //ö
    	  	  case 188: ((char*)ws_pkt.payload)[0] = '\x81'; break; //ü
    	  	  case 132: ((char*)ws_pkt.payload)[0] = '\x8E'; break; //Ä
    	  	  case 150: ((char*)ws_pkt.payload)[0] = '\x99'; break; //Ö
    	  	  case 156: ((char*)ws_pkt.payload)[0] = '\x9A'; break; //Ü
    	  	  case 159: ((char*)ws_pkt.payload)[0] = '\xDF'; break; //ß
    	  	  default: ESP_LOGW("DHS","Unhandled special character %d %d recieved",ws_pkt.payload[0],ws_pkt.payload[1]);
    	  }
	  	  ws_pkt.payload[1] = 0;
	  	  ws_pkt.len = 1;
      }
      if (ws_pkt.len == 1) DHS_WebServer::Controler->inHandler(ws_pkt.payload[0],255); // call Controller inHandler if 'tick ' char recieved
      else if (!DHS_WebServer::process((char*)ws_pkt.payload)) { // process line
    	DHS_WebServer::Command->processLine(std::string((char*)ws_pkt.payload)); // or call line Command
      }
/*  ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE("DHS", "httpd_ws_send_frame failed with %d", ret);
    }
*/
      free(buf);
    }
    return ret;
}

/////////////////////////////
// mdns
/////////////////////////////

void DHS_WebServer::initialise_mdns()
{
    char *hostname = CONFIG_MDNS_HOSTNAME; // to be changed to config parameter

    //initialize mDNS
    ESP_ERROR_CHECK_WITHOUT_ABORT( mdns_init() );
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK_WITHOUT_ABORT( mdns_hostname_set(hostname) );
    ESP_LOGI("DHS", "mdns hostname set to: [%s]", hostname);
    ESP_LOGW("DHS", "Access Device with %s.local", hostname);
    //set default mDNS instance name
    ESP_ERROR_CHECK_WITHOUT_ABORT( mdns_instance_name_set(CONFIG_MDNS_INSTANCE) );

    //structure with TXT records
    mdns_txt_item_t serviceTxtData[3] = {
        {"board", "esp32s2"},
        {"u", "user"},
        {"p", "password"}
    };

    //initialize service
    ESP_ERROR_CHECK_WITHOUT_ABORT( mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData, 3) );
    ESP_ERROR_CHECK_WITHOUT_ABORT( mdns_service_subtype_add_for_host("ESP32-WebServer", "_http", "_tcp", NULL, "_server"));
    //    ESP_ERROR_CHECK_WITHOUT_ABORT( mdns_service_subtype_add_for_host("DeafBlind-WebServer", "_http", "_tcp", NULL, "_server") );
}

/////////////////////////////
// WebServer constructor
/////////////////////////////

DHS_WebServer::DHS_WebServer(DHS_1Tick_Control *Cont) {
	/* Function to start the Web- and File server */
    static struct file_server_data *server_data = NULL;

 //   esp_log_level_set("httpd", ESP_LOG_DEBUG);

	Controler = Cont; // set one tick controler

    if (server_data) {
        ESP_LOGE("DHS", "WebServer already started");
        return;
    }

    /* Allocate memory for server data */
    server_data = (file_server_data* )calloc(1, sizeof(struct file_server_data));
	if (!server_data) {
	       ESP_LOGE("DHS", "Failed to allocate memory for server data");
	       return;
	}
	strlcpy(server_data->base_path, base_path, sizeof(server_data->base_path));
    server_data->Controler = Cont; // store controller pointer for access of sourcefd in broadcast

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

//     Use the URI wildcard matching function in order to
//     * allow the same handler to respond to multiple different
//     * target URIs which match the wildcard scheme
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;
    config.ctrl_port = 32769;
    config.stack_size=8096;
    config.open_fn = wss_open_fd;
//    config.close_fn = wss_close_fd;
//    config.max_open_sockets = 5;

    ESP_LOGI("DHS", "Starting HTTP Server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE("DHS", "Failed to start HTTP server!");
        return;
    }

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

    extern const unsigned char servercert_start[] asm("_binary_Fips_crt_start");
    extern const unsigned char servercert_end[]   asm("_binary_Fips_crt_end");
    conf.servercert = servercert_start;
    conf.servercert_len = servercert_end - servercert_start;

    extern const unsigned char prvtkey_start[] asm("_binary_Fips_key_start");
    extern const unsigned char prvtkey_end[]   asm("_binary_Fips_key_end");
    conf.prvtkey_pem = prvtkey_start;
    conf.prvtkey_len = prvtkey_end - prvtkey_start;

    conf.httpd.uri_match_fn = httpd_uri_match_wildcard;
    conf.httpd.lru_purge_enable = true;
    conf.httpd.open_fn = wss_open_fd;
    conf.httpd.close_fn = wss_close_fd;
//    conf.httpd.max_open_sockets = 5;


    esp_err_t ret = httpd_ssl_start(&sserver, &conf);
    if (ESP_OK != ret) {
        ESP_LOGE("DHS", "Error starting HTTPS server!");
    }
    else ESP_LOGI("DHS", "Starting HTTPS Server");


    /* URI handler for websocket server */
    httpd_uri_t websocket = {
        .uri       = "/ws",
        .method    = HTTP_GET,
        .handler   = websocket_handler,
        .user_ctx  = server_data,    // Pass server data as context
		.is_websocket = true,         // Mandatory: set to `true` to handler websocket protocol
//        .handle_ws_control_frames = true
    };
    httpd_register_uri_handler(server, &websocket);
    httpd_register_uri_handler(sserver, &websocket);

    httpd_uri_t fw_reset = {
        .uri       = "/reset",   // Match URIs of type /reset
        .method    = HTTP_GET,
        .handler   = reset_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &fw_reset);
    httpd_register_uri_handler(sserver, &fw_reset);

    /* URI handler for getting uploaded files */
    httpd_uri_t file_download = {
        .uri       = "/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = download_get_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_download);
    httpd_register_uri_handler(sserver, &file_download);

    /* URI handler for uploading files to server */
    httpd_uri_t file_upload = {
        .uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_upload);
    httpd_register_uri_handler(sserver, &file_upload);

    /* URI handler for deleting files from server */
    httpd_uri_t file_delete = {
        .uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_POST,
        .handler   = delete_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_delete);
    httpd_register_uri_handler(sserver, &file_delete);

    /* URI handler for FW-Update from server */
    httpd_uri_t fw_update = {
        .uri       = "/fwupdate/*",   // Match all URIs of type /fwupdate/path/to/file
        .method    = HTTP_POST,
        .handler   = fw_update_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &fw_update);
    httpd_register_uri_handler(sserver, &fw_update);

	DHS::event_handler <char> SocketOutHandler([this](char c)  { // WebSocket char output handler function
		if (c == 255) return; // no character given
		char l[7];
		sprintf(l,"tick %c",c);
		DHS_WebServer::Controler->sourcefd = -1;
		DHS_WebServer::broadcast(l);
	});

//	initialise_mdns();

	SocketOutHandler.setType(DHS_WebsocketHandler); // WebSocket (Browser) connections are local on root device (web client connections skipped!)
	outHandlerId = Controler->outHandlers += SocketOutHandler;
    return;
}
