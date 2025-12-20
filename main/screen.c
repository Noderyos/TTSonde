#include "screen.h"

#define TAG "ttsonde-screen"

SSD1306_t scr_dev;

void screen_init() {
    i2c_master_init(&scr_dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&scr_dev, 128, 64);
    ssd1306_clear_screen(&scr_dev, 0);
    ssd1306_contrast(&scr_dev, 0xff);
}