#include "wifi.h"


#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <nvs_flash.h>
#include <string.h>

#include "storage.h"
#include "ui/ui.h"
#include "utils.h"

#define TAG "ttsonde-wifi"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define MAX_RETRIES 2

static EventGroupHandle_t wifi_event_group;
static int wifi_retry = 0;
static int current_network = 0;

static wifi_config_t known_networks[32];
static size_t network_count = 0;

static void wifi_event_handler(void *,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ui_event_send(UI_EVENT_WIFI_CONNECT,
            known_networks[current_network].sta.ssid,
            strlen((char*)known_networks[current_network].sta.ssid));
        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry < MAX_RETRIES) {
            wifi_retry++;
            ESP_LOGW(TAG, "Retrying %s (%d/%d)",
                     (char*)known_networks[current_network].sta.ssid,
                     wifi_retry, MAX_RETRIES);
            ui_event_send(UI_EVENT_WIFI_RETRY, NULL, 0);
            esp_wifi_connect();
        } else {
            wifi_retry = 0;
            current_network++;

            if (current_network < network_count) {
                ESP_LOGI(TAG, "Trying next network: %s",
                         (char*)known_networks[current_network].sta.ssid);

                esp_wifi_set_config(WIFI_IF_STA,
                                    &known_networks[current_network]);
                ui_event_send(UI_EVENT_WIFI_CONNECT,
                    known_networks[current_network].sta.ssid,
                    strlen((char*)known_networks[current_network].sta.ssid));
                esp_wifi_connect();
            } else {
                ESP_LOGW(TAG, "All networks failed, switching to AP mode");
                xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            }
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = event_data;
        ESP_LOGI(TAG, "Connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ui_event_send(UI_EVENT_WIFI_SUCCESS, &event->ip_info.ip, sizeof(event->ip_info.ip));
        wifi_retry = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t start_sta_mode(void) {
    ESP_LOGI(TAG, "Starting STA mode ...");

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ERROR_CHECK(esp_wifi_init(&cfg));

    ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler,
                                               NULL));
    ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler,
                                               NULL));

    ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &known_networks[current_network]));
    ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

static esp_err_t load_wifi_ap(wifi_config_t *ap_conf) {
    season *wifi, *ap;
    season *ssid_obj, *passwd_obj, *type_obj;

    wifi = SEASON_OBJ_GET(get_config(), SEASON_OBJ, "wifi", ESP_FAIL);
    ap = SEASON_OBJ_GET(wifi, SEASON_OBJ, "ap", ESP_FAIL);

    ssid_obj = SEASON_OBJ_GET(ap, SEASON_STR, "ssid", ESP_FAIL);

    memcpy(ap_conf->ap.ssid, ssid_obj->string.str, ssid_obj->string.len);
    ap_conf->ap.ssid_len = strlen((char*)ap_conf->ap.ssid);


    type_obj = SEASON_OBJ_GET(ap, SEASON_STR, "type", ESP_FAIL);

    if (SEASON_STRCMP(type_obj, "open") == 0) {
        ap_conf->ap.authmode = WIFI_AUTH_OPEN;
    } else {
        passwd_obj = SEASON_OBJ_GET(ap, SEASON_STR, "password", ESP_FAIL);
        memcpy(ap_conf->ap.password, passwd_obj->string.str, passwd_obj->string.len);

        if (SEASON_STRCMP(type_obj, "wpa") == 0) {
            ap_conf->ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        } else if (SEASON_STRCMP(type_obj, "wep") == 0) {
            ap_conf->ap.authmode = WIFI_AUTH_WEP;
        } else {
            ESP_LOGE(TAG, "Invalid network type");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static esp_err_t start_ap_mode(void) {
    ESP_LOGI(TAG, "Starting AP mode ...");

    esp_netif_create_default_wifi_ap();

    wifi_config_t ap_conf = {
        .ap = {
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK
        }
    };

    ERROR_CHECK(load_wifi_ap(&ap_conf));

    ERROR_CHECK(esp_wifi_stop());
    ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_conf));
    ERROR_CHECK(esp_wifi_start());

    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "esp_netif_get_handle_from_ifkey(WIFI_AP_DEF) failed.");
        return ESP_FAIL;
    }

    esp_netif_ip_info_t ip_info;
    ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ip_info));

    ESP_LOGI(TAG, "AP started. SSID: %s, IP: " IPSTR,
        ap_conf.ap.ssid, IP2STR(&ip_info.ip));

    ui_event_send(UI_EVENT_WIFI_CONNECT, ap_conf.ap.ssid, strlen((char*)ap_conf.ap.ssid));
    ui_event_send(UI_EVENT_WIFI_SUCCESS, &ip_info.ip, sizeof(ip_info.ip));

    return ESP_OK;
}

esp_err_t wifi_init(void) {
    ERROR_CHECK(nvs_flash_init());
    ERROR_CHECK(esp_netif_init());
    ERROR_CHECK(esp_event_loop_create_default());

    season *networks = get_config_dot(SEASON_ARR, "wifi.networks");
    if (networks == NULL) {
        ESP_LOGE(TAG, "get_config_dot(wifi.networks) failed.");
        return ESP_FAIL;
    }

    size_t count = season_arr_len(networks);
    if (count > 32) count = 32;

    for (int i = 0; i < count; i++) {
        season *network = season_arr_get(networks, i);

        wifi_sta_config_t *sta = &known_networks[network_count++].sta;

        season *ssid_obj = SEASON_OBJ_GET(network, SEASON_STR, "ssid", ESP_FAIL);

        memcpy(sta->ssid, ssid_obj->string.str, ssid_obj->string.len);

        season *type_obj = SEASON_OBJ_GET(network, SEASON_STR, "type", ESP_FAIL);

        if (SEASON_STRCMP(type_obj, "open") == 0) {
            sta->threshold.authmode = WIFI_AUTH_OPEN;
        } else {
            season *passwd_obj = SEASON_OBJ_GET(network, SEASON_STR, "password", ESP_FAIL);

            memcpy(sta->password, passwd_obj->string.str, passwd_obj->string.len);

            if (SEASON_STRCMP(type_obj, "wpa") == 0) {
                sta->threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
            } else if (SEASON_STRCMP(type_obj, "wep") == 0) {
                sta->threshold.authmode = WIFI_AUTH_WEP;
            } else {
                ESP_LOGE(TAG, "Invalid network type");
                return ESP_FAIL;
            }
        }
    }

    wifi_event_group = xEventGroupCreate();

    EventBits_t bits = 0;

    if (network_count > 0) {
        start_sta_mode();

        bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE, portMAX_DELAY);
    }

    if (network_count == 0 || bits & WIFI_FAIL_BIT) {
        ERROR_CHECK(start_ap_mode());
    }

    return ESP_OK;
}