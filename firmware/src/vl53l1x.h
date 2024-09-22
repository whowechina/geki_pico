/*
 * VL53L1X Distance measurement sensor
 * WHowe <github.com/whowechina>
 * 
 * Most of this code is from https://github.com/pololu/vl53l0x-arduino
 */

#ifndef VL53L1X_H
#define VL53L1X_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

void vl53l1x_init(unsigned instance, i2c_inst_t *i2c_port, uint8_t i2c_addr);
void vl53l1x_use(unsigned instance);
bool vl53l1x_is_present();
bool vl53l1x_init_tof();

typedef enum { Short, Medium, Long, Unknown } DistanceMode;
bool vl53l1x_setDistanceMode(DistanceMode mode);
DistanceMode vl53l1x_getDistanceMode();

bool vl53l1x_setMeasurementTimingBudget(uint32_t budget_us); 
uint32_t vl53l1x_getMeasurementTimingBudget();

void vl53l1x_setROISize(uint8_t width, uint8_t height);
void vl53l1x_getROISize(uint8_t * width, uint8_t * height);
void vl53l1x_setROICenter(uint8_t spadNum);
uint8_t vl53l1x_getROICenter();

void vl53l1x_startContinuous(uint32_t period_ms);
void vl53l1x_stopContinuous();
uint16_t vl53l1x_readContinuousMillimeters();

//bool dataReady(); { return (readReg(GPIO__TIO_HV_STATUS) & 0x01) == 0; }

#endif