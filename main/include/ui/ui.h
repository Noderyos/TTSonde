#ifndef UI_EVENTS_H
#define UI_EVENTS_H

#include <stddef.h>

typedef enum {
    UI_EVENT_NONE = 0,
    UI_EVENT_BUTTON,
    UI_EVENT_SCREEN_CHANGED,

    // Decode
    UI_EVENT_SONDE_DATA,
    UI_EVENT_SONDE_LOST,
    UI_EVENT_SONDE_NAME,

    // Wifi
    UI_EVENT_WIFI_CONNECT,
    UI_EVENT_WIFI_SUCCESS,
    UI_EVENT_WIFI_RETRY,

    UI_EVENT_GPS_DATA,
} ui_event_type_t;

typedef struct {
    ui_event_type_t type;
    void *payload[128];
    size_t size;
} ui_event_t;


typedef enum {
    UI_SCREEN_WIFI_SCAN = 0,
    UI_SCREEN_DECODING,
    UI_SCREEN_ERROR,
    _UI_SCREEN_COUNT
} ui_screen_t;

void ui_set_screen(ui_screen_t next);
void ui_set_screen_deferred(ui_screen_t next, unsigned long ms);

int ui_event_send(ui_event_type_t type, const void *data, size_t size);
void ui_init(void);

#endif
