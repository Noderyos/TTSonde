#include "gps.h"

#include <driver/uart.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <string.h>
#include <time.h>

#include "ui/ui.h"
#include "utils.h"

#define TAG "ttsonde-gps"

#define GPS_UART UART_NUM_1

#define BUF_SIZE 128

static char nmea_buf[BUF_SIZE];
static size_t nmea_buf_pos = 0;

size_t nmea_tokenise(char *data, char *fields[], size_t max_fields) {
    size_t counter = 0;
    while (counter < max_fields && (fields[counter] = strsep(&data, ",")) != NULL) {
        counter++;
    }
    return counter;
}

void nmea_parse_time(const char *t, struct tm *tm) {

}

void nmea_parse_date(const char *d, struct tm *tm) {

}

time_t nmea_to_timestamp(const char *date, const char *time) {
    struct tm tm = {0};

    int dd = 1, MM = 1, yy = 0;
    int hh = 0, mm = 0, ss = 0;

    if (date && *date) sscanf(date, "%2d%2d%2d", &dd, &MM, &yy);
    tm.tm_mday = dd;
    tm.tm_mon  = MM - 1;
    tm.tm_year = (yy < 80 ? yy + 100 : yy);

    if (time && *time) sscanf(time, "%2d%2d%2d", &hh, &mm, &ss); // ss is reduced to int
    tm.tm_hour = hh;
    tm.tm_min  = mm;
    tm.tm_sec  = ss;

    tm.tm_isdst = 0;

    return mktime(&tm);
}


esp_err_t parse_nmea(char *buf) {
    if (buf[0] != '$' || strchr(buf, '*') == NULL) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t crc = strtol(strchr(buf, '*')+1, NULL, 16);
    for (char *c = &buf[1]; *c != '*'; c++) crc ^= *c;
    if (crc) return ESP_ERR_INVALID_CRC;

    char *tlk = &buf[1];
    char *sentence = &tlk[2];

    struct gps_data_t gps_data;

    char *fields[32];
    size_t fields_count = nmea_tokenise(sentence, fields, 32);

    if (strcmp(fields[0], "RMC") == 0) {
        if (fields_count != 14) return ESP_ERR_INVALID_RESPONSE;

        gps_data.time = nmea_to_timestamp(fields[9], fields[1]);

        float lat = strtof(fields[3], NULL);
        int lat_deg = ((int)lat/100);
        int lat_min = lat-(lat_deg*100);
        int lat_sec = 100*(lat-(lat_deg*100+lat_min));
        gps_data.lat = lat_deg + lat_min/60.f + lat_sec/3600.f;

        if (fields[4][0] == 'S') gps_data.lat = -gps_data.lat;

        float lon = strtof(fields[5], NULL);
        int lon_deg = ((int)lon/100);
        int lon_min = lat-(lon_deg*100);
        int lon_sec = 100*(lon-(lon_deg*100+lon_min));
        gps_data.lon = lon_deg + lon_min/60.f + lon_sec/3600.f;

        if (fields[6][0] == 'W') gps_data.lon = -gps_data.lon;

        gps_data.speed = strtof(fields[7], NULL) / 1.944f;
        gps_data.heading = strtof(fields[8], NULL);
        gps_data.magvar = strtof(fields[10], NULL);

        if (fields[11][0] == 'W') gps_data.magvar = -gps_data.magvar;

    } else if (strcmp(fields[0], "GGA") == 0) {
        if (fields_count != 15) return ESP_ERR_INVALID_RESPONSE;

        gps_data.dop = strtof(fields[8], NULL);
        gps_data.alt = strtof(fields[9], NULL);

        gps_data.sep = strtof(fields[11], NULL);

    } else if (strcmp(fields[0], "GSV") == 0) {
        //if (fields_count != 20) return ESP_ERR_INVALID_RESPONSE;

    }

    ui_event_send(UI_EVENT_GPS_DATA, &gps_data, sizeof(gps_data));

    return ESP_OK;
}

static void gps_task(void *) {
    uint8_t uart_buffer[256];

    while (1) {
        int len = uart_read_bytes(GPS_UART, uart_buffer, sizeof(uart_buffer) - 1, pdMS_TO_TICKS(1000));
        if (len <= 0) continue;

        for (ssize_t i = 0; i < len; i++) {
            if (i > 0 && uart_buffer[i] == '\n') {
                nmea_buf[nmea_buf_pos-1] = '\0';
                nmea_buf[nmea_buf_pos] = '\0';

                esp_err_t ret = parse_nmea(nmea_buf);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "parse_nmea(nmea_buf): %s", esp_err_to_name(ret));
                }

                nmea_buf_pos = 0;
            } else if (nmea_buf_pos < BUF_SIZE - 1) {
                nmea_buf[nmea_buf_pos++] = uart_buffer[i];
            } else {
                nmea_buf_pos = 0;
                ESP_LOGW(TAG, "Buffer overflow detected");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void gps_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 38400,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    uart_driver_install(GPS_UART, 1024, 0, 0, NULL, 0);
    uart_param_config(GPS_UART, &uart_config);

    uart_set_pin(GPS_UART,
        33, 34,
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);
}