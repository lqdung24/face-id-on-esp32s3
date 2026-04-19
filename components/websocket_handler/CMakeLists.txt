idf_component_register(
    SRCS "websocket_handler.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        espressif__esp_websocket_client
        esp_event
        freertos
        log
        json
)
