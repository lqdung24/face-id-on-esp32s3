#include "json_builder.hpp"
#include <stdio.h>
#include "cJSON.h"
#include <vector>

void json_build_faces_event(const std::__cxx11::list<dl::detect::result_t>  &faces,
                            char **out_str) {
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "type", "esp_event");
    cJSON_AddStringToObject(root, "name", "face_detected");
    cJSON_AddNumberToObject(root, "count", faces.size());

    cJSON *arr = cJSON_CreateArray();

    for (auto &f : faces) {
        cJSON *item = cJSON_CreateObject();

        cJSON_AddNumberToObject(item, "category", f.category);
        cJSON_AddNumberToObject(item, "score", f.score);

        cJSON_AddItemToObject(
            item, "box",
            cJSON_CreateIntArray(f.box.data(), f.box.size()));

        cJSON_AddItemToObject(
            item, "keypoint",
            cJSON_CreateIntArray(f.keypoint.data(), f.keypoint.size()));

        cJSON_AddItemToArray(arr, item);
    }

    cJSON_AddItemToObject(root, "faces", arr);

    *out_str = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);
}