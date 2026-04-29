idf_component_register(
    SRCS json_builder.cpp
    INCLUDE_DIRS "."
    REQUIRES
        json
        common
        espressif__human_face_recognition
)
