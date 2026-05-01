#pragma once
#include <vector>
#include "esp_err.h"
#include "esp_camera.h"
#include "dl_detect_define.hpp"
#include "event.hpp"
#include "human_face_recognition.hpp"
#include "dl_image_jpeg.hpp"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
    std::vector<uint8_t*> imgs;
    std::vector<size_t> lens;
    char name[32];
} EnrollCtx;

void enroll_task(void *arg);

void enroll_face();

/**
 * @brief Khởi tạo AI face detection model.
 *
 * Model sử dụng lazy loading – chỉ load vào RAM khi lần đầu chạy detect.
 *
 * @return ESP_OK nếu init thành công
 */
esp_err_t ai_init(void);

/**
 * @brief Chạy face detection trên một JPEG frame từ camera.
 *
 * Hàm này sẽ:
 * 1. Decode JPEG → RGB888
 * 2. Chạy HumanFaceDetect model
 * 3. Trả về số mặt phát hiện được
 *
 * @param fb Frame buffer JPEG nhận từ camera_capture()
 * @return Số lượng face detected (0 nếu không có hoặc lỗi)
 */
int ai_detect_faces(camera_fb_t *fb, std::__cxx11::list<dl::detect::result_t> &out);

/**
 * @brief Bật/tắt AI processing.
 *
 * Khi tắt, ai_detect_faces() sẽ return 0 ngay lập tức.
 *
 * @param running true để bật, false để tắt
 */
void ai_set_running(bool running);

/**
 * @brief Kiểm tra AI có đang enabled hay không.
 *
 * @return true nếu AI đang chạy
 */
bool ai_is_running(void);


#ifdef __cplusplus
}
#endif
