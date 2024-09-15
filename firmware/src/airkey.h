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
bool airkey_get(int id);

#endif
