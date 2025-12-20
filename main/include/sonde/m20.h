#ifndef M10_H
#define M10_H

#include "sonde.h"

#define M20_FRAMELEN 69 // 65 + bit more just to be sure

int m20_setup(float frequency);
int m20_receive(sonde_data_t *sonde);

#endif
