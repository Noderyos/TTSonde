#ifndef VBAT_H
#define VBAT_H

#include <esp_err.h>

esp_err_t init_vbat();
float get_vbat();
int get_bat_percentage();

#endif