/*
 * Lever Input
 * WHowe <github.com/whowechina>
 */

#ifndef LEVER_H
#define LEVER_H

#include <stdint.h>
#include <stdbool.h>

void lever_init();

uint8_t lever_read();
uint16_t lever_raw();
uint16_t lever_average();

#endif
