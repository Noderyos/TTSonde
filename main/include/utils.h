#ifndef UTILS_H
#define UTILS_H

#include <ssd1306.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ERROR_CHECK(x) \
    do { \
        int ret = x; \
        if (ret != ESP_OK) { \
            ESP_LOGE(TAG, #x": %s", esp_err_to_name(ret)); \
            return ESP_FAIL;\
        } \
    } while(0)

#define SEASON_OBJ_GET(parent, e_type, key, ret) \
    ({ \
    season *target = season_object_get(parent, key); \
    if (target == NULL) { ESP_LOGE(TAG, "Missing %s", key); return ret;} \
    if (target->type != e_type) {ESP_LOGE(TAG, "Unexpected type for %s", key); return ret;} \
    target; \
    })

#define SEASON_STRCMP(s_obj, str2) s_obj->string.len == strlen(str2) && strncmp(s_obj->string.str, str2, s_obj->string.len)

long millis();
void manchester_decode(uint8_t *in, uint8_t *out, size_t len);

inline int32_t be_i32(uint8_t d[4]){return d[3]|(d[2]<<8)|(d[1]<<16)|(d[0]<<24);}
inline int32_t be_i24(uint8_t d[3]){return d[2]|(d[1]<<8)|(d[0]<<16);}
inline int16_t be_i16(uint8_t d[2]){return d[1]|(d[0]<<8);}

inline int32_t le_i32(uint8_t d[4]){return d[0]|(d[1]<<8)|(d[2]<<16)|(d[3]<<24);}
inline int32_t le_i24(uint8_t d[3]){return d[0]|(d[1]<<8)|(d[2]<<16);}
inline int16_t le_i16(uint8_t d[2]){return d[0]|(d[1]<<8);}

void ssd1306_display_text_pro(SSD1306_t * dev, int x, int y,
    const char *text, int text_len, bool invert);

#endif
