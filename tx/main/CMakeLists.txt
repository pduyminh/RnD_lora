idf_component_register(
    SRCS "main.c" "tx_server.c"
    INCLUDE_DIRS "."
    REQUIRES common driver esp_driver_gpio esp_wifi nvs_flash esp_event esp_netif esp_http_server
)
