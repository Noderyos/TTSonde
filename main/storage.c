#include "storage.h"

#include <esp_littlefs.h>
#include <esp_log.h>

#define TAG "ttsonde-storage"

season config = {.type = _SEASON_ERROR};

esp_err_t storage_init() {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = LITTLEFS_PATH,
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    return esp_vfs_littlefs_register(&conf);
}

esp_err_t backup_file(const char *src_path, const char *dst_path) {
    FILE *src = fopen(src_path, "rb");
    if (!src) {
        ESP_LOGE(TAG, "Could not open %s", src_path);
        return ESP_ERR_NOT_FOUND;
    }

    FILE *dst = fopen(dst_path, "wb");
    if (!dst) {
        ESP_LOGE(TAG, "Could not open %s", dst_path);
        fclose(src);
        return ESP_ERR_NOT_FOUND;
    }

    char buffer[512];
    size_t read;
    while ((read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, read, dst);
    }

    fclose(src);
    fclose(dst);

    return ESP_OK;
}

char *read_entire_file(char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, size, file);
    buffer[size] = '\0';

    fclose(file);
    return buffer;
}

esp_err_t load_config() {
    if (config.type != _SEASON_ERROR)
        ESP_LOGW(TAG, "Config is already loaded, overwriting it");

    char *buf = read_entire_file(LITTLEFS_PATH"/config.json");
    if (buf == NULL) {
        ESP_LOGE(TAG, "read_entire_file() failed.");
        return ESP_FAIL;
    }

    if (season_load(&config, buf) < 0) {
        ESP_LOGE(TAG, "season_load() failed.");
        free(buf);
        return ESP_FAIL;
    }
    free(buf);
    return ESP_OK;
}

season *get_config() {
    if (config.type == _SEASON_ERROR) return NULL;
    return &config;
}

season *get_config_dot(enum season_type type, const char *fmt, ...) {
    char path[128];

    va_list args;
    va_start(args, fmt);
    vsnprintf(path, sizeof(path), fmt, args);
    va_end(args);

    season *current = &config;

    char *tok = strtok(path, ".");
    while (tok != NULL) {
        if (current->type != SEASON_OBJ) {
            ESP_LOGE(TAG, "Unexpected type for %s", path);
            return NULL;
        }
        current = season_object_get(current, tok);
        if (current == NULL) {
            ESP_LOGE(TAG, "Missing %s", path);
            return NULL;
        }
        tok = strtok(NULL, ".");
    }

    if (current->type != type) {
        ESP_LOGE(TAG, "Unexpected final type for %s", path);
    }

    return current;
}