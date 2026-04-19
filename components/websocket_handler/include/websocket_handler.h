#pragma once

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Kiểu callback khi nhận command từ server.
 *
 * @param action Chuỗi action nhận được, vd: "start_stream", "stop_stream",
 *               "start_ai", "stop_ai"
 */
typedef void (*ws_command_cb_t)(const char *action);

/**
 * @brief Khởi tạo WebSocket client (chưa kết nối).
 *
 * @param uri URI WebSocket, vd: "ws://192.168.1.100:5000/ws"
 * @return ESP_OK nếu init thành công
 */
esp_err_t websocket_init(const char *uri);

/**
 * @brief Bắt đầu kết nối WebSocket đến server.
 *
 * Phải gọi SAU khi WiFi đã connected và có IP.
 *
 * @return ESP_OK nếu start thành công
 */
esp_err_t websocket_start(void);

/**
 * @brief Dừng kết nối WebSocket.
 *
 * @return ESP_OK nếu stop thành công
 */
esp_err_t websocket_stop(void);

/**
 * @brief Gửi dữ liệu binary (JPEG frame) qua WebSocket.
 *
 * Server nhận raw JPEG bytes và forward cho browser clients.
 *
 * @param data Con trỏ đến dữ liệu JPEG
 * @param len  Kích thước dữ liệu (bytes)
 * @return Số bytes đã gửi, hoặc -1 nếu lỗi
 */
int websocket_send_bin(const uint8_t *data, size_t len);

/**
 * @brief Gửi chuỗi JSON text qua WebSocket.
 *
 * @param json_str Chuỗi JSON kết thúc '\0'
 * @return Số bytes đã gửi, hoặc -1 nếu lỗi
 */
int websocket_send_text(const char *json_str);

/**
 * @brief Kiểm tra WebSocket có đang connected hay không.
 *
 * @return true nếu connected
 */
bool websocket_is_connected(void);

/**
 * @brief Đăng ký callback để xử lý command từ server.
 *
 * Callback sẽ được gọi từ context của WS task khi nhận được
 * message JSON có type = "cmd_to_esp".
 *
 * @param cb Hàm callback
 */
void websocket_set_command_callback(ws_command_cb_t cb);

#ifdef __cplusplus
}
#endif
