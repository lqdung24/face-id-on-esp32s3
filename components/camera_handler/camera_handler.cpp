#include "camera_handler.h"
#include "img_converters.h"

#include "esp_log.h"

static const char *TAG = "camera_handler";

/* ── Pin configuration cho ESP32-S3-EYE (OV3660) ───────── */
#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5

#define CAM_PIN_D7 16
#define CAM_PIN_D6 17
#define CAM_PIN_D5 18
#define CAM_PIN_D4 12
#define CAM_PIN_D3 10
#define CAM_PIN_D2 8
#define CAM_PIN_D1 9
#define CAM_PIN_D0 11

#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13

esp_err_t camera_init(void) {
  camera_config_t config = {};

  config.pin_pwdn = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;
  config.pin_xclk = CAM_PIN_XCLK;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;

  config.pin_d7 = CAM_PIN_D7;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d0 = CAM_PIN_D0;

  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;
  config.pin_pclk = CAM_PIN_PCLK;

  config.xclk_freq_hz = 20000000; // 20 MHz XCLK
  config.ledc_timer = LEDC_TIMER_0;
  config.ledc_channel = LEDC_CHANNEL_0;

  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA; // 320×240
//   config.jpeg_quality = 12;           // 0-63, thấp = chất lượng cao
  config.fb_count = 2;                // Double-buffer
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
    return err;
  }

  /* Tinh chỉnh sensor sau khi init */
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 1); // Tăng sáng nhẹ
    s->set_contrast(s, 1);
    s->set_saturation(s, 0);
    s->set_hmirror(s, 1); // Mirror ngang (tuỳ hướng lắp camera)
    s->set_vflip(s, 0);
    ESP_LOGI(TAG, "Sensor PID: 0x%02x", s->id.PID);
  }

  ESP_LOGI(TAG, "Camera init OK – JPEG QVGA, 2 frame buffers");
  return ESP_OK;
}

camera_fb_t *camera_capture(void) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    ESP_LOGE(TAG, "Frame capture failed");
    return nullptr;
  }
  return fb;
}

uint8_t *capture_jpeg(size_t *out_len) {
    camera_fb_t *fb = camera_capture();
    if (!fb) return NULL;

    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;

    bool ok = frame2jpg(fb, 20, &jpg_buf, &jpg_len);
    camera_release(fb);

    if (!ok) {
        printf("JPEG compression failed\n");
        return NULL;
    }

    *out_len = jpg_len;
    return jpg_buf;
}

void camera_release(camera_fb_t *fb) {
  if (fb) {
    esp_camera_fb_return(fb);
  }
}
