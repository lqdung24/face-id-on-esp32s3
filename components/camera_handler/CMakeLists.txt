idf_component_register(
    SRCS "camera_handler.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        espressif__esp32-camera
        freertos
        log
)
