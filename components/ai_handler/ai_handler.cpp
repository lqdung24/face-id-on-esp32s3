#include "ai_handler.h"

#include <atomic>
#include <cstdlib>
#include <new>
#include <vector>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "human_face_detect.hpp"
#include "dl_image_define.hpp"
#include "img_converters.h"
#include "esp_timer.h"

static const char *TAG = "ai_handler";

/* ── State ───────────────────────────────────────────────── */
static std::atomic<bool> s_ai_running{false};
static HumanFaceDetect  *s_detector = nullptr;
static HumanFaceRecognizer *s_embedding = nullptr;

/* ── Public API ──────────────────────────────────────────── */
esp_err_t ai_init(void)
{
    /* Lazy-load model: chỉ allocate wrapper, model load khi run() lần đầu */
    s_detector = new (std::nothrow) HumanFaceDetect(HumanFaceDetect::MSRMNP_S8_V1, true);
    if (!s_detector) {
        ESP_LOGE(TAG, "Failed to create HumanFaceDetect");
        return ESP_ERR_NO_MEM;
    }
    s_embedding = new (std::nothrow) HumanFaceRecognizer(
        "/spiffs/face_id.bin",
        HumanFaceFeat::MFN_S8_V1, 
        true
    );
    if(!s_embedding){
        ESP_LOGE(TAG, "Failed to create HumanFaceEmbedding");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "AI face detection model initialized (lazy load)");
    return ESP_OK;
}

int convert_2_rgb888(camera_fb_t *fb, uint8_t *rgb_buf){
    if (!rgb_buf) {
        ESP_LOGE(TAG, "RGB buffer is not allocated");
        return 0;
    }

    bool ok = fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buf);

    if (!ok) {
        ESP_LOGE(TAG, "RGB → RGB888 convert failed (format=%d)", fb->format);
        free(rgb_buf);
        return 0;
    }
    return 1;
}

int ai_detect_faces(camera_fb_t *fb, std::__cxx11::list<dl::detect::result_t> &results)
{
    if (!s_ai_running.load() || !s_detector || !fb || !fb->buf) {
        return 0;
    }

    int w = fb->width;
    int h = fb->height;
    if (w <= 0 || h <= 0) return 0;

    size_t rgb_len = w * h * 3;

    uint8_t *rgb_buf = (uint8_t *)heap_caps_malloc(
        rgb_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!rgb_buf) {
        ESP_LOGE(TAG, "Failed to allocate RGB buffer");
        return 0;
    }

    bool ok = fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buf);

    if (!ok) {
        ESP_LOGE(TAG, "RGB → RGB888 convert failed (format=%d)", fb->format);
        free(rgb_buf);
        return 0;
    }

    dl::image::img_t img = {};
    img.data     = rgb_buf;
    img.width    = w;
    img.height   = h;
    img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

    int64_t t0 = esp_timer_get_time();
    results = s_detector->run(img);
    int64_t t1 = esp_timer_get_time();

    ESP_LOGI(TAG, "AI detect time: %.2f ms, Detected %d face(s)", (t1 - t0) / 1000.0, results.size());

    t0 = esp_timer_get_time();
    auto reco_results = s_embedding->recognize(img, results);
    t1 = esp_timer_get_time();

    ESP_LOGI(TAG, "AI recognize time: %.2f ms, size: %d", (t1 - t0) / 1000.0, reco_results.size());
    for (auto &r : reco_results) {
        ESP_LOGI(TAG, "id=%d similarity=%.4f", r.id, r.similarity);
    }

    for (auto &r : results) {
                // 🔹 log bbox
        ESP_LOGI(TAG, "box=[%d,%d,%d,%d] score=%.2f",
                 r.box[0], r.box[1], r.box[2], r.box[3], r.score);

        // 🔹 log keypoints
        int kp_size = r.keypoint.size();

        if (kp_size % 2 != 0) {
            ESP_LOGW(TAG, "Invalid keypoint size: %d", kp_size);
            continue;
        }

        printf("  keypoints (%d): ", kp_size / 2);

        for (int i = 0; i < kp_size; i += 2) {
            printf("(%d,%d) ", r.keypoint[i], r.keypoint[i + 1]);
        }
        printf("\n");
    }

    free(rgb_buf);
    return results.size();
}

void enroll_task(void *arg){
    EnrollCtx* ctx = (EnrollCtx*) arg;

    ESP_LOGI("ENROLL", "User: %s, imgs: %d", ctx->name, ctx->imgs.size());

    for (int i = 0; i < ctx->imgs.size(); i++) {

        uint8_t* psram_buf = ctx->imgs[i];
        size_t len = ctx->lens[i];

        uint8_t* buf = (uint8_t*) malloc(len);
        if (!buf) {
            ESP_LOGE("ENROLL", "malloc failed");
            continue;
        }
        memcpy(buf, psram_buf, len);

        free(psram_buf); // giải phóng PSRAM sớm

        // =========================
        // 1. Decode JPEG
        // =========================
        dl::image::jpeg_img_t jpeg_img;
        jpeg_img.data = buf;
        jpeg_img.data_len = len;
        dl::image::img_t img = dl::image::sw_decode_jpeg(
            jpeg_img,
            dl::image::DL_IMAGE_PIX_TYPE_RGB888
        );
        if (img.data == NULL) {
            ESP_LOGW("ENROLL", "Decode failed img %d", i);
            free(buf);
            continue;
        }

        // =========================
        // 2. Detect face
        // =========================
        auto detect_res = s_detector->run(img);
        ESP_LOGI(TAG, "enroll detected: %d",detect_res.size());
        // =========================
        // 3. Extract feature
        // =========================
        auto res = s_embedding->enroll(img, detect_res);

        if (res == ESP_FAIL) {
            ESP_LOGW("ENROLL", "Feature fail img %d", i);
            continue;
        }
    }

    ESP_LOGI("ENROLL", "Done user: %s", ctx->name);

    delete ctx;
    vTaskDelete(NULL);
}

void enroll_face(){

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
