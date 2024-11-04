/*
 * Controller Airkeys
 * WHowe <github.com/whowechina>
 */

#ifndef AIRKEY_H
#define AIRKEY_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/flash.h"

void airkey_init();
void airkey_update();
unsigned airkey_num();
bool airkey_get(unsigned id);

unsigned airkey_tof_num();
const char *airkey_tof_model();

void airkey_tof_init();
void airkey_tof_update_roi();

typedef enum {
    MIX_PRIMARY = 0,
    MIX_SECONDARY,
    MIX_MAX,
    MIX_MIN,
    MIX_AVG,
} tof_mix_algo_t;

#endif
