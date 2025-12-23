#include "vbat.h"

#include <esp_adc/adc_oneshot.h>

#define BAT_ADC_CHAN ADC_CHANNEL_7

static adc_oneshot_unit_handle_t bat_adc_handle;
static adc_cali_handle_t bat_adc_cal_handle = NULL;

esp_err_t init_vbat() {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config, &bat_adc_handle);

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(bat_adc_handle, BAT_ADC_CHAN, &config);

    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    esp_err_t ret = adc_cali_create_scheme_line_fitting(&cali_config, &bat_adc_cal_handle);
    return ret;
}

float get_vbat() {
    int adc_raw;
    int voltage_mv;

    adc_oneshot_read(bat_adc_handle, BAT_ADC_CHAN, &adc_raw);
    adc_cali_raw_to_voltage(bat_adc_cal_handle, adc_raw, &voltage_mv);

    return (float)voltage_mv * 2.0 / 1000.0;
}

int get_bat_percentage() {
    float vbat = get_vbat();
    float a = 1308.61759,
            b = -19750.76645,
            c = 111507.84393,
            d = -278956.60397,
            e = 260830.7429;

    // 100% ~ 4.12V, 0% ~ 3.4V
    float percentage =
            a * vbat * vbat * vbat * vbat +
            b * vbat * vbat * vbat +
            c * vbat * vbat +
            d * vbat +
            e;


    return percentage > 100 ? 100 : percentage+0.5; // Upper-bound round
}
