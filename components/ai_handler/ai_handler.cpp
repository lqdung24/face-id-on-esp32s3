#include "ai_handler.h"

#include <atomic>
#include <cstdlib>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "human_face_detect.hpp"
#include "dl_image_define.hpp"
#include "img_converters.h"

static const char *TAG = "ai_handler";

/* ── State ───────────────────────────────────────────────── */
static std::atomic<bool> s_ai_running{false};
static HumanFaceDetect  *s_detector = nullptr;

/* ── Public API ──────────────────────────────────────────── */
esp_err_t ai_init(void)
{
    /* Lazy-load model: chỉ allocate wrapper, model load khi run() lần đầu */
    s_detector = new (std::nothrow) HumanFaceDetect(HumanFaceDetect::MSRMNP_S8_V1, true);
    if (!s_detector) {
        ESP_LOGE(TAG, "Failed to create HumanFaceDetect");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "AI face detection model initialized (lazy load)");
    return ESP_OK;
}

int ai_detect_faces(camera_fb_t *fb)
{
    if (!s_ai_running.load() || !s_detector || !fb) {
        return 0;
    }

    /* ── Decode JPEG → RGB888 ──────────────────────────── */
    uint8_t *rgb_buf = nullptr;
    size_t rgb_len = 0;

    /* fmt2rgb888 allocates into rgb_buf on PSRAM */
    bool ok = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, nullptr);

    /* Dùng cách allocate thủ công + decode */
    int w = fb->width;
    int h = fb->height;
    rgb_len = w * h * 3;  // RGB888

    rgb_buf = static_cast<uint8_t *>(
        heap_caps_malloc(rgb_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!rgb_buf) {
        ESP_LOGE(TAG, "Failed to allocate RGB buffer (%zu bytes)", rgb_len);
        return 0;
    }

    ok = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb_buf);
    if (!ok) {
        ESP_LOGE(TAG, "JPEG → RGB888 decode failed");
        free(rgb_buf);
        return 0;
    }

    /* ── Chạy face detection ───────────────────────────── */
    dl::image::img_t img = {};
    img.data     = rgb_buf;
    img.width    = static_cast<uint16_t>(w);
    img.height   = static_cast<uint16_t>(h);
    img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

    auto &results = s_detector->run(img);
    int face_count = static_cast<int>(results.size());

    if (face_count > 0) {
        ESP_LOGI(TAG, "Detected %d face(s)", face_count);
        for (auto &r : results) {
            ESP_LOGI(TAG, "  → box=[%d,%d,%d,%d] score=%.2f",
                     r.box[0], r.box[1], r.box[2], r.box[3], r.score);
        }
    }

    free(rgb_buf);
    return face_count;
}

void ai_set_running(bool running)
{
    s_ai_running.store(running);
    ESP_LOGI(TAG, "AI %s", running ? "ENABLED" : "DISABLED");
}

bool ai_is_running(void)
{
    return s_ai_running.load();
}
