#include "ui/ui.h"

#include <esp_log.h>
#include <esp_netif_ip_addr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

#define SEASON_SHORT
#include <vbat.h>

#include "external/season.h"
#include "gps.h"
#include "screen.h"
#include "sonde.h"
#include "storage.h"

#include "utils.h"

#define TAG "ttsonde-ui"

static ui_screen_t current_screen;
static QueueHandle_t ui_queue;

void ui_on_exit(void);
void ui_on_enter(void);

void ui_set_screen(ui_screen_t next) {
    ui_on_exit();
    current_screen = next;
    ui_on_enter();

    ui_event_send(UI_EVENT_SCREEN_CHANGED, NULL, 0);
}

struct gps_data_t gps_data;

sonde_data_t sonde_data;
int has_sonde;

static char ssid[32];
static char ip[16];
int wifi_retry = 0;
ui_event_type_t last_wifi_event;

// Used to keep "previous" screen displayed for some time
static TimerHandle_t screen_timer;
static ui_screen_t pending_screen;
static void screen_timer_cb(TimerHandle_t){ui_set_screen(pending_screen);}

void ui_set_screen_deferred(ui_screen_t next, unsigned long ms) {
    pending_screen = next;

    if (!screen_timer) {
        screen_timer = xTimerCreate(
            "screen_timer",
            pdMS_TO_TICKS(ms),
            pdFALSE,
            NULL,
            screen_timer_cb
        );
    }

    xTimerStop(screen_timer, 0);
    xTimerChangePeriod(screen_timer, pdMS_TO_TICKS(ms), 0);
    xTimerStart(screen_timer, 0);
}


#define TEXT_LEN 64
#define fmt(format, ...) snprintf(text, TEXT_LEN, format, ##__VA_ARGS__)

static void render_field(season *el) {
    int x = SEASON_OBJ_GET(el, SEASON_NUM, "x", /*void*/)->number;
    int y = SEASON_OBJ_GET(el, SEASON_NUM, "y", /*void*/)->number;

    season *field_obj = SEASON_OBJ_GET(el, SEASON_STR, "field", /*void*/);

    char text[TEXT_LEN];
    size_t text_len = 0;

    if (SEASON_STRCMP(field_obj, "text") == 0) {
        season *value_obj = SEASON_OBJ_GET(el, SEASON_STR, "value", /*void*/);
        text_len = fmt("%.*s", value_obj->string.len, value_obj->string.str);
    }

    else if (SEASON_STRCMP(field_obj, "lat") == 0) {
        text_len = fmt("%.5fN", sonde_data.lat);
    } else if (SEASON_STRCMP(field_obj, "lon") == 0) {
        text_len = fmt("%.5fE", sonde_data.lon);
    } else if (SEASON_STRCMP(field_obj, "alt") == 0) {
        text_len = fmt("%.1fm", sonde_data.alt);
    } else if (SEASON_STRCMP(field_obj, "climb") == 0) {
        text_len = fmt("%.1fm/s", sonde_data.climb);
    } else if (SEASON_STRCMP(field_obj, "speed") == 0) {
        text_len = fmt("%.1fm/s", sonde_data.speed);
    } else if (SEASON_STRCMP(field_obj, "heading") == 0) {
        text_len = fmt("%d'", (int)sonde_data.heading);
    } else if (SEASON_STRCMP(field_obj, "temp") == 0) {
        text_len = fmt("%.2f'C", sonde_data.temp);
    } else if (SEASON_STRCMP(field_obj, "rh_temp") == 0) {
        text_len = fmt("%.2f'C", sonde_data.rh_temp);
    } else if (SEASON_STRCMP(field_obj, "rh") == 0) {
        text_len = fmt("%.1f%%", sonde_data.rh);
    } else if (SEASON_STRCMP(field_obj, "pressure") == 0) {
        text_len = fmt("%.1fhPa", sonde_data.pressure);
    } else if (SEASON_STRCMP(field_obj, "vbat") == 0) {
        text_len = fmt("%.2fV", sonde_data.vbat);
    } else if (SEASON_STRCMP(field_obj, "seq") == 0) {
        text_len = fmt("%d", sonde_data.seq);
    } else if (SEASON_STRCMP(field_obj, "serial") == 0) {
        text_len = fmt("%s", sonde_data.serial);
    } else if (SEASON_STRCMP(field_obj, "time") == 0) {
        text_len = strftime(text, TEXT_LEN, "%Y-%m-%d %H:%M:%S", localtime(&sonde_data.time));
    } else if (SEASON_STRCMP(field_obj, "model") == 0) {
        text_len = fmt("%s", sonde_data.model);
    }

    else if (SEASON_STRCMP(field_obj, "dev_vbat") == 0) {
        text_len = fmt("%.2f", get_vbat());
    } else if (SEASON_STRCMP(field_obj, "dev_percent") == 0) {
        text_len = fmt("%3d%%", get_bat_percentage());
    } else if (SEASON_STRCMP(field_obj, "dev_ssid") == 0) {
        text_len = fmt("%s", ssid);
    } else if (SEASON_STRCMP(field_obj, "dev_ip") == 0) {
        text_len = fmt("%s", ip);
    } else if (SEASON_STRCMP(field_obj, "dev_lat") == 0) {
        text_len = fmt("%.5fN", gps_data.lat);
    } else if (SEASON_STRCMP(field_obj, "dev_lon") == 0) {
        text_len = fmt("%.5fE", gps_data.lon);
    } else if (SEASON_STRCMP(field_obj, "dev_alt") == 0) {
        text_len = fmt("%.1fm", gps_data.alt);
    } else if (SEASON_STRCMP(field_obj, "dev_speed") == 0) {
        text_len = fmt("%.1fm/s", gps_data.speed);
    } else if (SEASON_STRCMP(field_obj, "dev_heading") == 0) {
        text_len = fmt("%d'", (int)gps_data.heading);
    } else if (SEASON_STRCMP(field_obj, "dev_dop") == 0) {
        text_len = fmt("%.1f", gps_data.dop);
    } else if (SEASON_STRCMP(field_obj, "dev_time") == 0) {
        text_len = strftime(text, TEXT_LEN, "%Y-%m-%d %H:%M:%S", localtime(&gps_data.time));
    } else if (SEASON_STRCMP(field_obj, "dev_magvar") == 0) {
        text_len = fmt("%.1f", gps_data.magvar);
    } else if (SEASON_STRCMP(field_obj, "dev_sep") == 0) {
        text_len = fmt("%.1f", gps_data.sep);
    }

    ssd1306_display_text_pro(&scr_dev, x, y, text, text_len, 0);
}

void ui_on_exit(void) {

}

void ui_on_enter(void) {
    if (current_screen == UI_SCREEN_DECODING) has_sonde = 0;
}

int ui_event_send(ui_event_type_t type, const void *data, size_t size) {
    ui_event_t evt = {
        .type = type,
        .size = size
    };

    if (size > sizeof(evt.payload))
        return -1;

    if (data && size)
        memcpy(evt.payload, data, size);

    return xQueueSend(ui_queue, &evt, 0) == pdTRUE;
}

int ui_on_event(ui_event_t evt) {
    if (evt.type == UI_EVENT_SCREEN_CHANGED)
        return 1;

    if (evt.type == UI_EVENT_SONDE_DATA) {
        memcpy(&sonde_data, evt.payload, evt.size);
        has_sonde = 1;
        return 1;
    }
    if (evt.type == UI_EVENT_SONDE_LOST) {
        int reload = has_sonde;
        has_sonde = 0;
        return reload;
    }

    if (evt.type == UI_EVENT_WIFI_CONNECT) {
        last_wifi_event = UI_EVENT_WIFI_CONNECT;
        memcpy(ssid, evt.payload, evt.size+1);
        wifi_retry = 0;
        return 1;
    }
    if (evt.type == UI_EVENT_WIFI_RETRY) {
        last_wifi_event = UI_EVENT_WIFI_RETRY;
        wifi_retry++;
        return 1;
    }
    if (evt.type == UI_EVENT_WIFI_SUCCESS) {
        last_wifi_event = UI_EVENT_WIFI_SUCCESS;
        snprintf(ip, sizeof(ip), IPSTR, IP2STR((esp_ip4_addr_t*)evt.payload));
        return 1;
    }

    if (evt.type == UI_EVENT_GPS_DATA) {
        memcpy(&gps_data, evt.payload, evt.size);
    }

    return 0;
}

season *layout = NULL;
esp_err_t ui_render() {

    if (!screen_timer || !xTimerIsTimerActive(screen_timer)) {
        layout = NULL;
    }

    switch (current_screen) {
        case UI_SCREEN_WIFI_SCAN: {
            if (last_wifi_event == UI_EVENT_WIFI_CONNECT) {
                layout = get_config_dot(SEASON_ARR, "layout.wifi.connect");
            }
            else if (last_wifi_event == UI_EVENT_WIFI_RETRY) {
                layout = get_config_dot(SEASON_ARR, "layout.wifi.retry");
            }
            else if (last_wifi_event == UI_EVENT_WIFI_SUCCESS) {
                layout = get_config_dot(SEASON_ARR, "layout.wifi.success");
            }
        } break;

        case UI_SCREEN_DECODING: {
            if (!has_sonde) {
                layout = get_config_dot(SEASON_ARR, "layout.decode.no_data");
            } else {
                layout = get_config_dot(SEASON_ARR, "layout.decode.%s", sonde_data.model);
            }
        } break;

        case UI_SCREEN_ERROR:
        default:
    }

    if (layout == NULL) {
        ESP_LOGE(TAG, "Get UI layout failed.");
        return ESP_FAIL;
    }
    
    ssd1306_clear_screen(&scr_dev, 0);
    for (int i = 0; i < season_array_len(layout); i++) {
        season *el = season_array_get(layout, i);
        render_field(el);
    }
    return ESP_OK;
}

static void ui_task(void *) {
    ui_event_t evt;

    current_screen = UI_SCREEN_WIFI_SCAN;
    ui_on_enter();

    while (1) {
        if (!xQueueReceive(ui_queue, &evt, portMAX_DELAY))
            continue;

        if (ui_on_event(evt) == 0) continue;

        if (ui_render(evt) != ESP_OK) {
            ESP_LOGE(TAG, "ui_render() failed.");
        }
    }
}

void ui_init(void) {
    ui_queue = xQueueCreate(8, sizeof(ui_event_t));
    xTaskCreate(ui_task, "ui", 4096, NULL, 5, NULL);
}