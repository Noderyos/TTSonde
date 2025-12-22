#ifndef STORAGE_H
#define STORAGE_H

#include <esp_err.h>

#define SEASON_SHORT
#include "external/season.h"

#define LITTLEFS_PATH "/fs"

esp_err_t storage_init();
char *read_entire_file(char *filename);
esp_err_t backup_file(const char *src_path, const char *dst_path);

esp_err_t load_config();
season *get_config();

season *get_config_dot(enum season_type type, const char *fmt, ...);

#endif