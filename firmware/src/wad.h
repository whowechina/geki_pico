/*
 * Controller Wads
 * WHowe <github.com/whowechina>
 */

#ifndef WAD_H
#define WAD_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/flash.h"

void wad_init();
void wad_update();
bool wad_read_left();
bool wad_read_right();

#endif
