/*
 * Controller Buttons
 * WHowe <github.com/whowechina>
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

void button_init();
uint8_t button_num();
void button_update();
uint16_t button_read();

uint32_t button_stat_keydown(uint8_t id);
void button_clear_stat();
#endif
