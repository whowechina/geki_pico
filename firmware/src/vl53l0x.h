/*
 * VL53L0X Distance measurement sensor
 * WHowe <github.com/whowechina>
 * 
 * Most of this code is from https://github.com/pololu/vl53l0x-arduino
 */

#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdint.h>
#include <stdbool.h>

#include "hardware/i2c.h"

void vl53l0x_init(unsigned instance, i2c_inst_t *i2c_port);
bool vl53l0x_change_addr(uint8_t i2c_addr);

void vl53l0x_use(unsigned instance);
bool vl53l0x_is_present();
bool vl53l0x_init_tof();

bool setMeasurementTimingBudget(uint32_t budget_us);
uint32_t getMeasurementTimingBudget();

typedef enum {
    VcselPeriodPreRange, VcselPeriodFinalRange
} vcselPeriodType;

bool setVcselPulsePeriod(vcselPeriodType type, uint8_t period_pclks);
uint8_t getVcselPulsePeriod(vcselPeriodType type);

void vl53l0x_start_continuous();
void vl53l0x_stop_continuous();

uint16_t readRangeContinuousMillimeters();
uint16_t readRangeSingleMillimeters();

#endif