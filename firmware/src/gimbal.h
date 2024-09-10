/*
 * Left and Right Gimbal Inputs
 * WHowe <github.com/whowechina>
 */

#ifndef GIMBAL_H
#define GIMBAL_H

#include <stdint.h>
#include <stdbool.h>

void gimbal_init();

uint8_t gimbal_read();
uint16_t gimbal_raw();
uint16_t gimbal_average();

#endif
