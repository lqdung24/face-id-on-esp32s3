/**
 * @file main.cpp
 * @brief ESP32-S3 Smart Lock – Main Application
 *
 * Luồng khởi tạo:
 *   1. NVS Flash init
 *   2. WiFi STA init → đợi có IP
 *   3. Camera OV3660 init
 *   4. AI face detection init
 *   5. WebSocket init → connect đến server
 *   6. Tạo stream_task (Core 0) và ai_task (Core 1)
 */

#include <cstring>
#include <atomic>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "wifi_handler.h"
#include "camera_handler.h"
#include "websocket_handler.h"
#include "ai_handler.h"
#include "dl_detect_define.hpp"
#include "event.hpp"
#include "json_builder.hpp"

static const char *TAG = "main";

/* ── Cấu hình ───────────────────────────────────────────── */
#define WIFI_SSID       "B13-405"
#define WIFI_PASS       "12346789"
#define WS_SERVER_URI   "ws://192.168.1.15:5000/ws"

/* ── Shared state ────────────────────────────────────────── */
static std::atomic<bool> s_streaming{false};

/* ── Command callback từ WebSocket ───────────────────────── */
static void on_ws_command(const char *action)
{
    if (strcmp(action, "start_stream") == 0) {
        s_streaming.store(true);
        ESP_LOGI(TAG, ">>> Stream STARTED");
    }
    else if (strcmp(action, "stop_stream") == 0) {
        s_streaming.store(false);
        ESP_LOGI(TAG, ">>> Stream STOPPED");
    }
    else if (strcmp(action, "start_ai") == 0) {
        ai_set_running(true);
        ESP_LOGI(TAG, ">>> AI STARTED");
    }
    else if (strcmp(action, "stop_ai") == 0) {
        ai_set_running(false);
        ESP_LOGI(TAG, ">>> AI STOPPED");
    }
    else {
        ESP_LOGW(TAG, "Unknown command: %s", action);
    }
}

/* ── Stream Task: chụp JPEG + gửi qua WebSocket ─────────── */
static void stream_task(void *arg)
{
    ESP_LOGI(TAG, "stream_task running on core %d", xPortGetCoreID());

    while (true) {
        if (!s_streaming.load() || !websocket_is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        size_t jpg_len = 0;
        uint8_t *buf = capture_jpeg(&jpg_len);

        if (!buf) {
            ESP_LOGW(TAG, "capture_jpeg failed");
            continue;
        }

        int sent = websocket_send_bin(buf, jpg_len);

        if (sent < 0) {
            ESP_LOGW(TAG, "Failed to send frame (%zu bytes)", jpg_len);
        }

        free(buf);  // 🔥 luôn free (success hay fail)

        /* ~15 FPS */
        vTaskDelay(pdMS_TO_TICKS(66));
    }
}

/* ── AI Task: face detection (chỉ khi enabled) ──────────── */
static void ai_task(void *arg)
{
    ESP_LOGI(TAG, "ai_task running on core %d", xPortGetCoreID());

    while (true) {
        if (!ai_is_running()) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        camera_fb_t *fb = camera_capture();

        if (fb) {
            std::vector<face_event_t> result;
            int faces = ai_detect_faces(fb, result);

            /* Gửi kết quả detect về server */
            if (faces > 0 && websocket_is_connected()) {
                char *json;
                json_build_faces_event(result, &json);

                websocket_send_text(json);

                free(json);
            }

            camera_release(fb);
        }

        /* AI chạy chậm hơn stream, ~5 FPS */
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* ── Entry point ─────────────────────────────────────────── */
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " ESP32-S3 Smart Lock – Starting...");
    ESP_LOGI(TAG, "========================================");

    /* 1. NVS Flash */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. WiFi – init và ĐỢI cho đến khi có IP */
    ESP_LOGI(TAG, "[1/5] Initializing WiFi...");
    ESP_ERROR_CHECK(wifi_init_sta(WIFI_SSID, WIFI_PASS));
    wifi_wait_connected();
    ESP_LOGI(TAG, "WiFi connected! IP: %s", wifi_get_ip_str());

    /* 3. Camera OV3660 */
    ESP_LOGI(TAG, "[2/5] Initializing Camera...");
    ESP_ERROR_CHECK(camera_init());

    /* 4. AI Face Detection */
    ESP_LOGI(TAG, "[3/5] Initializing AI model...");
    ESP_ERROR_CHECK(ai_init());

    /* 5. WebSocket – init SAU khi WiFi ready */
    ESP_LOGI(TAG, "[4/5] Initializing WebSocket...");
    ESP_ERROR_CHECK(websocket_init(WS_SERVER_URI));
    websocket_set_command_callback(on_ws_command);
    ESP_ERROR_CHECK(websocket_start());

    /* 6. Tạo tasks */
    ESP_LOGI(TAG, "[5/5] Creating tasks...");

    xTaskCreatePinnedToCore(
        stream_task,        // Function
        "stream_task",      // Name
        4096,               // Stack size
        nullptr,            // Parameter
        5,                  // Priority
        nullptr,            // Handle
        0                   // Core 0
    );

    xTaskCreatePinnedToCore(
        ai_task,            // Function
        "ai_task",          // Name
        8192,               // Stack size (AI cần nhiều hơn)
        nullptr,            // Parameter
        4,                  // Priority (thấp hơn stream)
        nullptr,            // Handle
        1                   // Core 1
    );

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " System ready! Waiting for commands...");
    ESP_LOGI(TAG, "========================================");
}
