#include "websocket_handler.h"

#include "cJSON.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include <cstring>

static const char *TAG = "ws_handler";

static esp_websocket_client_handle_t s_ws_client = nullptr;
static ws_command_cb_t s_cmd_callback = nullptr;

/* ── WebSocket Event Handler ─────────────────────────────── */
static void websocket_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data) {
  auto *data = static_cast<esp_websocket_event_data_t *>(event_data);

  switch (event_id) {
  case WEBSOCKET_EVENT_CONNECTED:
    ESP_LOGI(TAG, "WebSocket connected to server");
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
          ESP_LOGI(TAG, "Command received: %s", action->valuestring);
          if (s_cmd_callback) {
            s_cmd_callback(action->valuestring);
          }
        }
        cJSON_Delete(root);
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
  ws_cfg.buffer_size = 64 * 1024; // 64 KB buffer cho JPEG
  ws_cfg.task_stack = 6 * 1024;
  ws_cfg.task_prio = 5;

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
  return esp_websocket_client_send_bin(
      s_ws_client, reinterpret_cast<const char *>(data), static_cast<int>(len),
      pdMS_TO_TICKS(1000));
}

int websocket_send_text(const char *json_str) {
  if (!s_ws_client || !esp_websocket_client_is_connected(s_ws_client)) {
    return -1;
  }
  return esp_websocket_client_send_text(s_ws_client, json_str,
                                        static_cast<int>(strlen(json_str)),
                                        pdMS_TO_TICKS(1000));
}

bool websocket_is_connected(void) {
  if (!s_ws_client)
    return false;
  return esp_websocket_client_is_connected(s_ws_client);
}

void websocket_set_command_callback(ws_command_cb_t cb) { s_cmd_callback = cb; }
