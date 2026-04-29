#pragma once
#include <stddef.h>
#include <event.hpp>
#include "human_face_recognition.hpp"

#ifdef __cplusplus
extern "C" {
#endif


void json_build_faces_event(const std::__cxx11::list<dl::detect::result_t> &faces,
                            char **out_str);

#ifdef __cplusplus
}
#endif