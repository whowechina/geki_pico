/*
 * Controller Airkeys
 * WHowe <github.com/whowechina>
 * 
 */

#include "airkey.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"

#include "vl53l0x.h"
#include "vl53l1x.h"

#include "config.h"
#include "board_defs.h"

static i2c_inst_t *tof_ports[] = TOF_PORT_DEF;
#define TOF_NUM (count_of(tof_ports))
enum { TOF_VL53L0X = 1, TOF_VL53L1X = 2 };
uint8_t tof_models[TOF_NUM] = { 0 };

static struct {
    uint8_t port_id;
    uint16_t in_low;
    uint16_t in_high;
    uint16_t out_low;
    uint16_t out_high;
} key_defs[] = {
    { 0, 50, 200, 30, 220 },
    { 1, 50, 200, 30, 220 },
    { 0, 300, 400, 280, 430 },
    { 1, 300, 400, 280, 430 }
};

#define AIRKEY_NUM (count_of(key_defs))

static bool sw_val[AIRKEY_NUM]; /* true if triggered */
static uint64_t sw_freeze_time[AIRKEY_NUM];

void airkey_init()
{
    uint8_t tof_gpios[] = TOF_GPIO_DEF;
    static_assert(count_of(tof_gpios) == TOF_NUM * 2);

    for (int i = 0; i < TOF_NUM; i++) {
        uint8_t scl = tof_gpios[i * 2];
        uint8_t sda = tof_gpios[i * 2 + 1];

        gpio_init(scl);
        gpio_init(sda);

        i2c_init(tof_ports[i], TOF_I2C_FREQ);
        gpio_set_function(scl, GPIO_FUNC_I2C);
        gpio_set_function(sda, GPIO_FUNC_I2C);

        gpio_pull_up(scl);
        gpio_pull_up(sda);

        vl53l0x_init(i, tof_ports[i], 0);
        vl53l1x_init(i, tof_ports[i], 0);
        vl53l0x_use(i);
        vl53l1x_use(i);

        if (vl53l0x_is_present()) {
            tof_models[i] = TOF_VL53L0X;
            vl53l0x_init_tof();
            vl53l0x_start_continuous();
        } else if (vl53l1x_is_present()) {
            tof_models[i] = TOF_VL53L1X;
            vl53l1x_init_tof();
            vl53l1x_startContinuous(20);
        } else {
            tof_models[i] = 0;
        }
    }
}

static uint16_t tof_dist[count_of(tof_ports)];
static bool readings[AIRKEY_NUM];

static void tof_read()
{
    for (int i = 0; i < TOF_NUM; i++) {
        if (tof_models[i] == TOF_VL53L0X) {
            vl53l0x_use(i);
            tof_dist[i] = readRangeContinuousMillimeters();
        } else if (tof_models[i] == TOF_VL53L1X) {
            vl53l1x_use(i);
            tof_dist[i] = vl53l1x_readContinuousMillimeters();
        }
    }
}

#define BETWEEN(x, a, b) (((x) >= (a)) && ((x) <= (b)))
static bool airkey_read(unsigned index)
{
    uint16_t dist = tof_dist[key_defs[index].port_id];

    if (readings[index]) {
        return BETWEEN(dist, key_defs[index].out_low, key_defs[index].out_high);
    } else {
        return BETWEEN(dist, key_defs[index].in_low, key_defs[index].in_high);
    }
}

/* If a switch flips, it freezes for a while */
#define DEBOUNCE_FREEZE_TIME_US 30000
void airkey_update()
{
    uint64_t now = time_us_64();

    tof_read();

    for (int i = 0; i < AIRKEY_NUM; i++) {
        bool triggered = airkey_read(i);
        if (now >= sw_freeze_time[i]) {
            if (triggered != sw_val[i]) {
                sw_val[i] = triggered;
                sw_freeze_time[i] = now + DEBOUNCE_FREEZE_TIME_US;
            }
        }
        readings[i] = sw_val[i];
    }
}

unsigned airkey_num()
{
    return AIRKEY_NUM;
}

bool airkey_get(unsigned id)
{
    if (id >= AIRKEY_NUM) {
        return false;
    }

    return readings[id];
}

unsigned airkey_tof_num()
{
    return TOF_NUM;
}

const char *airkey_tof_model(unsigned tof_id)
{
    if (tof_id >= TOF_NUM) {
        return "Unknown";
    }

    if (tof_models[tof_id] == TOF_VL53L0X) {
        return "VL53L0X";
    } else if (tof_models[tof_id] == TOF_VL53L1X) {
        return "VL53L1X";
    } else {
        return "Unknown";
    }
}
