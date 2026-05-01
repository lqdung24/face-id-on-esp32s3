#include "websocket_handler.h"

#include "cJSON.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <cstring>
#include <vector>
#include <stdint.h>


static const char *TAG = "ws_handler";

static esp_websocket_client_handle_t s_ws_client = nullptr;
static ws_command_cb_t s_cmd_callback = nullptr;
static SemaphoreHandle_t s_send_mutex = nullptr;
static bool s_registering = false;
static char s_reg_name[32] = {0};
static int s_reg_expected = 0;
static int s_reg_received = 0;
static uint8_t *img_buf = NULL;
static int img_len = 0;
static std::vector<uint8_t*> s_imgs;
static std::vector<size_t> s_lens;
static bool s_enrolling = false;


/* ── WebSocket Event Handler ─────────────────────────────── */
static void websocket_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data) {
  auto *data = static_cast<esp_websocket_event_data_t *>(event_data);

  switch (event_id) {
  case WEBSOCKET_EVENT_CONNECTED:
    ESP_LOGI(TAG, "WebSocket connected to server");
    // Gửi hello message để server nhận diện đây là ESP32
    {
      const char *hello = "{\"type\":\"esp_status\",\"name\":\"hello\",\"faces_enrolled\":0}";
      esp_websocket_client_send_text(s_ws_client, hello,
                                     strlen(hello), pdMS_TO_TICKS(1000));
      ESP_LOGI(TAG, "Sent esp_status hello to server");
    }
    break;

  case WEBSOCKET_EVENT_DISCONNECTED:
    ESP_LOGW(TAG, "WebSocket disconnected");
    break;

  case WEBSOCKET_EVENT_DATA:
    /* Chỉ xử lý text frame (opcode 0x01) hoặc final fragment */
    if (data->op_code == 0x01 && data->data_len > 0) {
        /* Parse JSON command từ server */
        cJSON *root = cJSON_ParseWithLength(data->data_ptr, data->data_len);
        if (root) {
            cJSON *type = cJSON_GetObjectItem(root, "type");
            cJSON *action = cJSON_GetObjectItem(root, "action");

            if (type && cJSON_IsString(type) &&
                strcmp(type->valuestring, "cmd_to_esp") == 0 && action &&
                cJSON_IsString(action)) {
                
                // Đọc thêm name và count
                cJSON *name_item = cJSON_GetObjectItem(root, "name");
                cJSON *count_item = cJSON_GetObjectItem(root, "count");

                const char* name_str = nullptr;
                int count_val = 0;

                if (name_item && cJSON_IsString(name_item)) {
                    name_str = name_item->valuestring;
                }
                if (count_item && cJSON_IsNumber(count_item)) {
                    count_val = count_item->valueint;
                }

                ESP_LOGI(TAG, "Command received: %s, name: %s, count: %d", 
                        action->valuestring, name_str ? name_str : "N/A", count_val);
                
                if (s_cmd_callback) {
                    // Truyền tất cả vào callback
                    s_cmd_callback(action->valuestring, name_str, count_val);
                    if(strcmp(action->valuestring, "register") ==0){
                        if (s_enrolling) {
                            ESP_LOGW(TAG, "Busy enrolling, ignore new request");
                            return;
                        }
                        s_registering = true;

                        strncpy(s_reg_name, name_str, sizeof(s_reg_name) - 1);

                        s_reg_expected = count_val;
                        s_reg_received = 0;
                    }
                }
            }

            cJSON_Delete(root);
        }
    }
    if (data->op_code == 0x02 && s_registering) {

        if (s_enrolling) {
            ESP_LOGW("WS", "Busy enrolling, ignore new request");
            return;
        }

        if (data->payload_offset == 0) {
            // frame đầu tiên
            img_buf = (uint8_t*) malloc(data->payload_len);
            img_len = 0;
        }

        memcpy(img_buf + data->payload_offset, data->data_ptr, data->data_len);
        img_len += data->data_len;

        if (data->payload_offset + data->data_len == data->payload_len) {
            uint8_t* buf = (uint8_t*) heap_caps_malloc(data->payload_len, MALLOC_CAP_SPIRAM);
            memcpy(buf, img_buf, data->payload_len);

            s_imgs.push_back(buf);
            s_lens.push_back(data->payload_len);
            
            free(img_buf);
            img_buf = NULL;

            s_reg_received++;

            ESP_LOGI("WS", "Saved full image (%d/%d) (len %d)", s_reg_received, s_reg_expected, img_len);

            if (s_reg_received == s_reg_expected) {
                s_registering = false;
                ESP_LOGI("WS", "All images received → enroll");
                
                if (!s_enrolling) {
                    s_enrolling = true;
                    EnrollCtx* ctx = new EnrollCtx;

                    auto imgs = s_imgs;
                    auto lens = s_lens;

                    s_imgs.clear();
                    s_lens.clear();

                    ctx->imgs = imgs;
                    ctx->lens = lens;
                    strcpy(ctx->name, s_reg_name);

                    xTaskCreate(enroll_task, "enroll", 10000, ctx, 5, NULL);
                }
            }
        }
    }
    break;

  case WEBSOCKET_EVENT_ERROR:
    ESP_LOGE(TAG, "WebSocket error");
    break;

  default:
    break;
  }
}

/* ── Public API ──────────────────────────────────────────── */
esp_err_t websocket_init(const char *uri) {
    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri = uri;
    ws_cfg.buffer_size = 16 * 1024; // 64 KB buffer cho JPEG
    ws_cfg.task_stack = 16 * 1024;
    ws_cfg.task_prio = 5;

    s_send_mutex = xSemaphoreCreateMutex();
    if (!s_send_mutex) {
        ESP_LOGE(TAG, "Failed to create send mutex");
        return ESP_FAIL;
    }

    s_ws_client = esp_websocket_client_init(&ws_cfg);

    if (!s_ws_client) {
        ESP_LOGE(TAG, "WebSocket client init failed");
        return ESP_FAIL;
    }

    esp_websocket_register_events(s_ws_client, WEBSOCKET_EVENT_ANY,
                                    websocket_event_handler, nullptr);

    ESP_LOGI(TAG, "WebSocket client initialized – URI: %s", uri);
    return ESP_OK;
}

esp_err_t websocket_start(void) {
  if (!s_ws_client) {
    ESP_LOGE(TAG, "WebSocket client not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t err = esp_websocket_client_start(s_ws_client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "WebSocket start failed: 0x%x", err);
    return err;
  }

  ESP_LOGI(TAG, "WebSocket client started");
  return ESP_OK;
}

esp_err_t websocket_stop(void) {
  if (!s_ws_client) {
    return ESP_ERR_INVALID_STATE;
  }
  return esp_websocket_client_close(s_ws_client, pdMS_TO_TICKS(2000));
}

int websocket_send_bin(const uint8_t *data, size_t len) {
  if (!s_ws_client || !esp_websocket_client_is_connected(s_ws_client)) {
    return -1;
  }
  if (xSemaphoreTake(s_send_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
    return -1;  // busy, skip this frame
  }
  int ret = esp_websocket_client_send_bin(
      s_ws_client, reinterpret_cast<const char *>(data), static_cast<int>(len),
      pdMS_TO_TICKS(1000));
  xSemaphoreGive(s_send_mutex);
  return ret;
}


int websocket_send_text(const char *json_str) {
  if (!s_ws_client || !esp_websocket_client_is_connected(s_ws_client)) {
    return -1;
  }
  if (xSemaphoreTake(s_send_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
    return -1;  // busy, skip
  }
  int ret = esp_websocket_client_send_text(s_ws_client, json_str,
                                           static_cast<int>(strlen(json_str)),
                                           pdMS_TO_TICKS(1000));
  xSemaphoreGive(s_send_mutex);
  return ret;
}

bool websocket_is_connected(void) {
  if (!s_ws_client)
    return false;
  return esp_websocket_client_is_connected(s_ws_client);
}

void websocket_set_command_callback(ws_command_cb_t cb) { s_cmd_callback = cb; }
