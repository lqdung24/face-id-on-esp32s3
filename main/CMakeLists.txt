idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        wifi_handler
        camera_handler
        websocket_handler
        ai_handler
        nvs_flash
        esp_wifi
        esp_event
        freertos
        common
        json_builder
        storage
)