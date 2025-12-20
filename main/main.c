#include <stdio.h>
#include <string.h>

#include <esp_event.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <lwip/sockets.h>
#include <sx127x.h>
#include <wifi.h>

#define SEASON_IMPLEMENTATION
#define SEASON_SHORT
#include "include/external/season.h"

#include "gps.h"
#include "screen.h"
#include "sonde/m20.h"
#include "storage.h"
#include "http.h"
#include "ui/ui.h"
#include "utils.h"
#include "vbat.h"

#define TAG "ttsonde"

static const sonde_t sonde_m20 = {
    .name = "M20",
    .setup = m20_setup,
    .receive = m20_receive,
};

static const sonde_t *sondes[_SONDE_COUNT] = {
    [SONDE_M20] = &sonde_m20,
};

static sonde_model_t current_sonde = SONDE_M20;

void display_sonde_data(const sonde_data_t *data) {
    ESP_LOGI(TAG, "--- Sonde Data Report ---");
    ESP_LOGI(TAG, "Sequence number (seq):  %d", data->seq);
    ESP_LOGI(TAG, "Serial (ser):           %s", data->serial);

    ESP_LOGI(TAG, "--- Position ---");
    ESP_LOGI(TAG, "Latitude (lat):         %.4f", data->lat);
    ESP_LOGI(TAG, "Longitude (lon):        %.4f", data->lon);
    ESP_LOGI(TAG, "Altitude (alt):         %.1f m", data->alt);
    ESP_LOGI(TAG, "Vert. Speed (climb):    %.2f m/s", data->climb);
    ESP_LOGI(TAG, "Horiz. Speed (speed):   %.2f m/s", data->speed);
    ESP_LOGI(TAG, "Heading (heading):      %.1f deg", data->heading);

    ESP_LOGI(TAG, "--- Time ---");
    ESP_LOGI(TAG, "GPS Time (time):        %llu", data->time);

    ESP_LOGI(TAG, "--- Sensor Data ---");
    ESP_LOGI(TAG, "Temperature (temp):     %.2f C", data->temp);
    ESP_LOGI(TAG, "RH Temp (rh_temp):      %.2f C", data->rh_temp);
    ESP_LOGI(TAG, "Relative Humidity (rh): %.1f %%", data->rh);
    ESP_LOGI(TAG, "Pressure (pressure):    %.1f hPa", data->pressure);
    ESP_LOGI(TAG, "Battery Voltage (vbat): %.2f V", data->vbat);

    ESP_LOGI(TAG, "---------------------------");
}

void radio_receiver_task(void *) {
    ui_set_screen_deferred(UI_SCREEN_DECODING, 5000);

    const sonde_t *sonde = sondes[current_sonde];

    fsk_set_lna_gain(0);
    int gain = fsk_get_lna_gain();
    ESP_LOGI(TAG, "RX LNA Gain is %d", gain);

    if (sonde->setup(405002000) < 0) {
        ESP_LOGE(TAG, "sonde.setup(%s) failed.", sonde->name);
        vTaskDelete(NULL);
        return;
    }

    sonde_data_t sonde_data;

    int synced = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));

        ESP_LOGI(TAG, "sonde.receive() start at %ld", millis());
        int ret = sonde->receive(&sonde_data);
        ESP_LOGI(TAG, "sonde.receive() exited");

        if (ret < 0) {
            ESP_LOGE(TAG, "Invalid packet parsed");
        } else if (ret > 0) {
            ESP_LOGI(TAG, "Packet parsed successfully");
            display_sonde_data(&sonde_data);
            if (!synced) {
                ui_event_send(UI_EVENT_SONDE_NAME,
                    sondes[current_sonde]->name, sizeof(sondes[current_sonde]->name));
                synced = 1;
            }
            ui_event_send(UI_EVENT_SONDE_DATA, &sonde_data, sizeof(sonde_data));
        } else {
            ESP_LOGI(TAG, "sonde.receive() timed out.");
            ui_event_send(UI_EVENT_SONDE_LOST, NULL, 0);
            synced = 0;
        }
    }
}

esp_err_t init(void) {

    ESP_LOGI(TAG, "Initializing VBAT ...");
    ERROR_CHECK(init_vbat());
    ESP_LOGI(TAG, "Battery voltage: %f", get_vbat());

    ESP_LOGI(TAG, "Initializing Screen ...");
    screen_init();

    ESP_LOGI(TAG, "Initializing LittleFS ...");
    int ret = storage_init();
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Initializing config ..."); // Requires LittleFS
    ERROR_CHECK(load_config());

    ESP_LOGI(TAG, "Initializing UI ...");
    ui_init();

    ESP_LOGI(TAG, "Initializing wireless ...");
    ERROR_CHECK(wifi_init());

    ESP_LOGI(TAG, "Initializing HTTP ...");
    http_init();

    ESP_LOGI(TAG, "Initializing SX127x ...");
    ERROR_CHECK(sx127x_init());

    ESP_LOGI(TAG, "Initializing GPS ...");
    gps_init();

    return ESP_OK;
}

void app_main(void) {
    if (init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize TTSonde");
        return;
    }

    xTaskCreatePinnedToCore(
        radio_receiver_task, "radio_rx",
        4096, NULL, 5, NULL, 1
    );

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}