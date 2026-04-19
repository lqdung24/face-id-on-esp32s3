idf_component_register(
    SRCS "ai_handler.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        espressif__human_face_detect
        espressif__esp-dl
        espressif__esp32-camera
        espressif__esp_jpeg
        freertos
        log
        esp_psram
)
