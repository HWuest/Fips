# Software for ESP32S2 microcontroller

The SW can be build with the Espressif IDF 5.0.1.
It uses several Espressif SW components, manely the Espressif mesh-lite mesh network component which is used for the intercommunication of the devices.

(REQUIRES driver fatfs vfs nvs_flash esp_wifi lwip esp_timer esp_https_server mdns esp_lcd_touch_xpt2046 esp_websocket_client app_update ssd1306 iot_bridge mesh_lite)

The SW is written in C++ using the following architecture:
[SW_Architektur.pdf](https://github.com/HWuest/Fips/files/12486967/SW_Architektur.pdf)
