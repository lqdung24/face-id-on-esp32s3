void enroll_task(void *arg) {

    ESP_LOGI("ENROLL", "Start...");

    for (int i = 0; i < s_imgs.size(); i++) {

        uint8_t *psram_buf = s_imgs[i];
        size_t len = s_lens[i];

        // ⚠️ nhiều lib không thích PSRAM → copy sang DRAM
        uint8_t *buf = (uint8_t*) malloc(len);
        if (!buf) {
            ESP_LOGE("ENROLL", "malloc failed");
            continue;
        }
        memcpy(buf, psram_buf, len);

        free(psram_buf); // free sớm PSRAM

        // 👉 decode JPEG
        auto img = decode_jpeg(buf, len);
        free(buf);

        if (!img) {
            ESP_LOGW("ENROLL", "decode fail %d", i);
            continue;
        }

        // 👉 detect
        auto detect_res = m_detect.run(img);
        if (detect_res.empty()) {
            ESP_LOGW("ENROLL", "no face %d", i);
            continue;
        }

        // 👉 feature
        auto feat = m_feat.run(img, detect_res.back().keypoint);

        // 👉 lưu DB
        db.enroll_feat(feat);

        ESP_LOGI("ENROLL", "done %d/%d", i+1, s_imgs.size());
    }

    s_imgs.clear();
    s_lens.clear();

    ESP_LOGI("ENROLL", "Done!");

    s_enrolling = false;
    vTaskDelete(NULL);
}