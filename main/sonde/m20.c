#include "sonde/m20.h"

#include <math.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

#include "sx127x.h"
#include "utils.h"

#define TAG "ttsonde-m20"

/*
frame[0x16..0x17]: block check (fwVer < 0x06) ; frame[0x16]: SPI1 P[0] (fwVer >= 0x07), frame[0x17]=0x00

frame[0x44..0x45]: frame check
 */

typedef struct m20_frame {
/* 00 */    uint8_t len;
/* 01 */    uint8_t type;
/* 02 */    uint8_t rh_counts[2];
/* 04 */    uint8_t adc_temp[2];
/* 06 */    uint8_t adc_rh_temp[2];
/* 08 */    uint8_t alt[3];
/* 0b */    uint8_t dlat[2];
/* 0d */    uint8_t dlon[2];
/* 0f */    uint8_t time[3];
/* 12 */    uint8_t serial[3];
/* 15 */    uint8_t seq;
/* 16 */    union {
                uint8_t blk_crc[2];
                uint8_t p_lsb;
            };
/* 18 */    uint8_t climb[2];
/* 1a */    uint8_t week[2];
/* 1c */    uint8_t lat[4];
/* 20 */    uint8_t lon[4];
/* 24 */    uint8_t pressure[2];
/* 26 */    uint8_t vbat;
/* 27 */    uint8_t unk2[8];
/* 2f */    uint8_t rh_ref[2];
/* 31 */    uint8_t unk3[18];
/* 43 */    uint8_t fwver;
/* 44 */    uint8_t frm_crc[2];
/* 36 */    uint8_t unk4[11];
} m20_frame_t;

int m20_setup(float frequency) {
    float agcbw = 20800;
    float rxbw = 12500;

    if(fsk_set_fsk() < 0) {
        ESP_LOGI(TAG, "Setting FSK mode FAILED");
        return -1;
    }

    if(fsk_set_bitrate(9600) < 0) {
        ESP_LOGI(TAG, "Setting bitrate FAILED");
        return -1;
    }

    if(fsk_set_afc_bandwidth(agcbw) < 0) {
        ESP_LOGI(TAG, "Setting AFC bandwidth %f Hz FAILED", agcbw);
        return -1;
    }
    if(fsk_set_rx_bandwidth(rxbw) < 0) {
        ESP_LOGI(TAG, "Setting RX bandwidth to %f Hz FAILED", rxbw);
        return -1;
    }

    if(fsk_set_sync_conf(0x70, 2, (uint8_t*)"\x66\x66") < 0) {
        ESP_LOGI(TAG, "Setting SYNC Config FAILED");
        return -1;
    }

    fsk_set_frequency(frequency);

    fsk_set_rx_conf(0);

    fsk_set_preamble_detect(0x1F);

    fsk_clear_irq_flags();

    fsk_set_packet_config(0x08, 0x40);

    fsk_set_payload_length(0);
    return 0;
}

static uint8_t rxbitc;
static int rxp = 0;
static int headerDetected = 0;

static uint32_t rxdata;
static int rxsearching = 1;

int process_byte(uint8_t dt, uint8_t raw_packet[M20_FRAMELEN*2]) {
    for (int _ = 0; _ < 8; _++) {
        uint8_t d = (dt & 0x80) ? 1 : 0;
        dt <<= 1;
        rxdata = (rxdata << 1) | d;

        if (rxsearching) {
            if (rxdata == 0xcccca64c || rxdata == 0x333359b3) {
                rxsearching = 0;
                rxbitc = 0;
                rxp = 0;
                headerDetected = 1;
                float rssi = fsk_get_rssi();
                float afc = fsk_get_afc();
                ESP_LOGI(TAG, "SYNC!!! Test: RSSI=%f AFC=%f", rssi, afc);
            }
        } else {
            rxbitc = (rxbitc + 1) % 8;
            if (rxbitc == 0) {
                raw_packet[rxp++] = rxdata & 0xff;

                if (rxp == M20_FRAMELEN*2) {
                    rxsearching = 1;
                    return 1;
                }
            }
        }
    }
    return 0;
}

int receive_raw_packet(uint8_t raw_packet[M20_FRAMELEN*2]) {
    unsigned long t0 = millis();
    fsk_enable_rx();

    while (millis() - t0 < 5000) {
        uint8_t irq2 = fsk_get_irq_flag_2();

        if (!(irq2 & 0x40)) {
            uint8_t data = fsk_get_fifo();
            if (process_byte(data, raw_packet)) return 1;
        } else {
            if (headerDetected) {
                t0 = millis();
                headerDetected = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
    return 0;
}

static float m20_tntc2(m20_frame_t *m20) {
	// SMD ntc , RH-Temperature
	float Rs = 22.1e3; // Rs/Ohm P5.6=Vcc
	float R25 = 2.2e3; // R25/Ohm
	float b = 3650.0; // B/Kelvin
	float T25 = 25.0 + 273.15; // T0=25C
	float T = 0.0; // T/Kelvin
	uint16_t ADC_ntc0;
	float x, R;

	ADC_ntc0 = le_i16(m20->adc_rh_temp);
	x = (4095.0 - ADC_ntc0) / ADC_ntc0; // (Vcc-Vout)/Vout
	R = Rs / x;
	if (R > 0) T = 1.0 / (1.0 / T25 + 1.0 / b * log(R / R25));

	return T - 273.15;
}

static float m20_rh(m20_frame_t *m20, float rh_temp) {
	float RH = -1.0f;
	float x;

	uint16_t humval = le_i16(m20->rh_counts);
	uint16_t rh_cal = le_i16(m20->rh_ref);

	float humidityCalibration = 6.4e8f / (rh_cal + 80000.0f);

	x = (humval + 80000.0f) * humidityCalibration * (1.0f - 5.8e-4f * (rh_temp - 25.0f));
	x = 4.16e9f / x;
	x = 10.087f * x * x * x - 211.62f * x * x + 1388.2f * x - 2797.0f;

	if (humval < 48000) {
		if (x > -20.0f && x < 120.f) {
			RH = x;
			if (RH < 0.0f) RH = 0.0f;
			if (RH > 100.0f) RH = 100.0f;
		}
	}

	return RH;
}

static float m20_temp(m20_frame_t *m20) {
	float p0 = 1.07303516e-03,
			p1 = 2.41296733e-04,
			p2 = 2.26744154e-06,
			p3 = 6.52855181e-08;

	float Rs[3] = {12.1e3, 36.5e3, 475.0e3}; // bias/series
	float Rp[3] = {1e20, 330.0e3, 2000.0e3}; // parallel, Rp[0]=inf

	uint8_t scT = 0;
	uint16_t ADC_RT;

	float x, R;
	float T = 0; // T/Kelvin

	ADC_RT = le_i16(m20->adc_temp);

	if (ADC_RT > 8191) {
		scT = 2;
		ADC_RT -= 8192;
	} else if (ADC_RT > 4095) {
		scT = 1;
		ADC_RT -= 4096;
	} else { scT = 0; }

	x = (4095.0 - ADC_RT) / ADC_RT; // (Vcc-Vout)/Vout
	R = Rs[scT] / (x - Rs[scT] / Rp[scT]);

	if (R > 0) {
		T = 1.0 / (
			p0 +
			p1 * log(R) +
			p2 * log(R) * log(R) +
			p3 * log(R) * log(R) * log(R)
		);
	}

	if (T - 273.15 < -120.0 || T - 273.15 > 60.0) T = 0;

	return T - 273.15; // Celsius
}

int m20_parse_packet(uint8_t raw_packet[M20_FRAMELEN], sonde_data_t *sonde) {
    m20_frame_t *m20 = (m20_frame_t *)raw_packet;

    // Position
    sonde->lat = be_i32(m20->lat) * 1e-6;
    sonde->lon = be_i32(m20->lon) * 1e-6;
    sonde->alt = be_i24(m20->alt) * 1e-2;

    // Movement
    sonde->climb = be_i16(m20->climb) * 1e-2;

    float dx = be_i16(m20->dlat) * 1e-2;
    float dy = be_i16(m20->dlon) * 1e-2;
    sonde->speed = sqrtf(dx*dx + dy*dy);
    sonde->heading = atan2f(dy, dx) * 180.0 / M_PI;
    if (sonde->heading < 0) sonde->heading += 360;

    const uint32_t ms = be_i24(m20->time);
    const uint16_t week = be_i16(m20->week);

    sonde->time = (time_t)((86400UL*7)*week) + ms + 315964800; // Starts at 01-01-1980 ...

    // Environment
	sonde->temp = m20_temp(m20);
	sonde->rh_temp = m20_tntc2(m20);
	sonde->rh = m20_rh(m20, sonde->rh_temp);
    uint32_t pressure = le_i16(m20->pressure) << 8;
    if (m20->fwver >= 0x07)
        pressure |= m20->p_lsb;

    if (pressure > 0xA00000)
        sonde->pressure = -1.0f;
    else if (pressure > 0)
        sonde->pressure = pressure / 4096.f;


    // Misc
    sonde->seq = m20->seq;
    sonde->vbat = m20->vbat * (3.3/255);

    strcpy(sonde->model, "M20");
    const uint32_t serial = le_i24(m20->serial);
    snprintf(sonde->serial, 31,
        "M20-%01ld%02ld-%ld-%05ld",
        (serial & 0x3F) / 12, (serial & 0x3F) % 12 + 1,
        (serial >> 6) & 0x0F,
        serial >> 10);

    return 0;
}

static uint8_t raw_packet[M20_FRAMELEN * 2];
static uint8_t packet[M20_FRAMELEN];

int m20_receive(sonde_data_t *sonde) {
	if (receive_raw_packet(raw_packet) == 0)
		return 0;

	manchester_decode(raw_packet, packet, M20_FRAMELEN * 2);

	for (int i = 0; i < M20_FRAMELEN; i++) {
		printf("%02x", packet[i]);
	}
	printf("\n");

	if (m20_parse_packet(packet, sonde) < 0)
		return -1;

	return 1;
}