/*
 * Controller Wads
 * WHowe <github.com/whowechina>
 * 
 */

#include "wad.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"

#include "vl53l0x.h"
#include "config.h"
#include "board_defs.h"

#define WAD_NUM 2
static i2c_inst_t *wad_ports[] = WAD_DEF;
static bool sw_val[WAD_NUM]; /* true if triggered */
static uint64_t sw_freeze_time[WAD_NUM];

void wad_init()
{
    uint8_t wad_gpio[] = WAD_GPIO_DEF;
    for (int i = 0; i < WAD_NUM; i++) {
        sw_val[i] = false;
        sw_freeze_time[i] = 0;
        uint8_t scl = wad_gpio[i * 2];
        uint8_t sda = wad_gpio[i * 2 + 1];

        gpio_init(scl);
        gpio_init(sda);

        gpio_set_function(scl, GPIO_FUNC_I2C);
        gpio_set_function(sda, GPIO_FUNC_I2C);

        gpio_pull_up(scl);
        gpio_pull_up(sda);

        vl53l0x_init(i, wad_ports[i], 0);
        vl53l0x_use(i);
        vl53l0x_init_tof();
        vl53l0x_start_continuous();
    }
}

static bool wad_readings[WAD_NUM];

static bool wad_read(unsigned index)
{
    vl53l0x_use(index);
    uint16_t dist = readRangeContinuousMillimeters(index);
    if (wad_readings[index]) {
        return (dist >= 80) && (dist <= 270);
    }
    return (dist >= 100) && (dist <= 250);
}

/* If a switch flips, it freezes for a while */
#define DEBOUNCE_FREEZE_TIME_US 30000
void wad_update()
{
    uint64_t now = time_us_64();

    for (int i = 0; i < WAD_NUM; i++) {
        bool triggered = wad_read(i);
        if (now >= sw_freeze_time[i]) {
            if (triggered != sw_val[i]) {
                sw_val[i] = triggered;
                sw_freeze_time[i] = now + DEBOUNCE_FREEZE_TIME_US;
            }
        }
        wad_readings[i] = sw_val[i];
    }
}

bool wad_read_left()
{
    return wad_readings[0];
}

bool wad_read_right()
{
    return wad_readings[1];
}
