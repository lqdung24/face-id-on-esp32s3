#include "storage.hpp"
#include <sys/dirent.h>
#include <esp_log.h>

void init_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "face",
      .max_files = 5,
      .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);

    DIR* dir = opendir("/spiffs");
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI("SPIFFS", "file: %s", entry->d_name);
    }
    closedir(dir);
}