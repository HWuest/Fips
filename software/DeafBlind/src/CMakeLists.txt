idf_component_register(SRCS "DHS_1Tick_Control.cpp" "DHS_Console.cpp" "DHS_GPIO.cpp" "DHS_FileSystem.cpp" "DHS_Config.cpp" "DHS_TickCommand.cpp" 
    "DHS_WebServer.cpp" "DHS_LCD.cpp" "DHS_WebsocketClient.cpp" "DHS_LCDklein.cpp" "DHS_Mesh_lite.cpp" "DHS_Train_Control.cpp"
    "DB_LCD.cpp"
    "mesh_netif.cpp" "st7789.cpp" "fontx.cpp" "pngle.cpp"
	REQUIRES driver fatfs vfs nvs_flash esp_wifi lwip esp_timer esp_https_server mdns esp_lcd_touch_xpt2046 esp_websocket_client app_update ssd1306 iot_bridge mesh_lite
    INCLUDE_DIRS "." ".."
	EMBED_FILES "favicon.ico" "upload_script.html"
	EMBED_TXTFILES "../cert/Fips.crt" "../cert/Fips.key")
	
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
