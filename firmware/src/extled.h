/*
 * Extended LED Signals from USB Serial
 * WHowe <github.com/whowechina>
 * 
 */

#ifndef EXTLED_H
#define EXTLED_H

#include <stdint.h>
#include <stdbool.h>

void extled_init();
void extled_update();
bool extled_is_active();

#endif
