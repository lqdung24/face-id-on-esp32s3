#pragma once

#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo camera OV3660 với output JPEG.
 *
 * Cấu hình pin cho ESP32-S3-EYE, JPEG format, QVGA resolution.
 * Frame buffer nằm trong PSRAM.
 *
 * @return ESP_OK nếu init thành công
 */
esp_err_t camera_init(void);

/**
 * @brief Chụp một frame JPEG từ camera.
 *
 * @return Con trỏ đến camera_fb_t chứa dữ liệu JPEG.
 *         Trả về NULL nếu lỗi. Sau khi dùng xong PHẢI gọi camera_release().
 */
camera_fb_t *camera_capture(void);

/**
 * @brief Trả lại frame buffer cho camera driver.
 *
 * @param fb Con trỏ frame buffer nhận từ camera_capture()
 */
void camera_release(camera_fb_t *fb);

#ifdef __cplusplus
}
#endif
