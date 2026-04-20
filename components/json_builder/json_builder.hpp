#pragma once
#include <stddef.h>
#include <event.hpp>

#ifdef __cplusplus
extern "C" {
#endif


void json_build_faces_event(const std::vector<face_event_t> &faces,
                            char **out_str);

#ifdef __cplusplus
}
#endif