#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo WiFi STA mode và kết nối đến AP.
 *
 * Hàm này sẽ init netif, event loop, WiFi driver và bắt đầu kết nối.
 * Hàm KHÔNG block — dùng wifi_wait_connected() để đợi có IP.
 *
 * @param ssid     Tên WiFi (max 31 ký tự)
 * @param password Mật khẩu WiFi (max 63 ký tự)
 * @return ESP_OK nếu init thành công
 */
esp_err_t wifi_init_sta(const char *ssid, const char *password);

/**
 * @brief Block cho đến khi WiFi nhận được IP address.
 *
 * Sử dụng FreeRTOS EventGroup, timeout vô hạn.
 */
void wifi_wait_connected(void);

/**
 * @brief Lấy chuỗi IP address hiện tại.
 *
 * @return Con trỏ đến chuỗi IP tĩnh (vd "192.168.1.100"), hoặc "0.0.0.0" nếu chưa có IP.
 */
const char *wifi_get_ip_str(void);

#ifdef __cplusplus
}
#endif
