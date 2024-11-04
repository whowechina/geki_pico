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

typedef enum { TOF_NONE = 0, TOF_VL53L0X = 1, TOF_VL53L1X = 2 } tof_model_t;
typedef enum { SIDE_LEFT = 0, SIDE_RIGHT = 1 } airkey_side_t;

static struct {
    i2c_inst_t *port;
    uint8_t gpio;
    tof_model_t model;
    bool init_ok;
} tofs[4] = {
    { TOF_LEFT_PORT, 0xff, },
    { TOF_LEFT_PORT, TOF_LEFT_SECOND_GPIO, },
    { TOF_RIGHT_PORT, 0xff, },
    { TOF_RIGHT_PORT, TOF_RIGHT_SECOND_GPIO, },
};

#define TOF_NUM (count_of(tofs))

static struct {
    airkey_side_t side;
    uint16_t in_low;
    uint16_t in_high;
    uint16_t out_low;
    uint16_t out_high;
} key_defs[] = {
    { SIDE_LEFT, 50, 200, 20, 230 },
    { SIDE_RIGHT, 50, 200, 20, 230 },
    { SIDE_LEFT, 300, 400, 280, 430 },
    { SIDE_RIGHT, 300, 400, 280, 430 }
};

#define AIRKEY_NUM (count_of(key_defs))

static bool sw_val[AIRKEY_NUM]; /* true if triggered */
static uint64_t sw_freeze_time[AIRKEY_NUM];

void airkey_init()
{
    uint8_t tof_gpios[] = TOF_GPIO_DEF;
    for (int i = 0; i < count_of(tof_gpios); i++) {
        gpio_init(tof_gpios[i]);
        gpio_set_function(tof_gpios[i], GPIO_FUNC_I2C);
        gpio_pull_up(tof_gpios[i]);
    }

    i2c_init(TOF_LEFT_PORT, TOF_I2C_FREQ);
    if (TOF_LEFT_PORT != TOF_RIGHT_PORT) {
        i2c_init(TOF_RIGHT_PORT, TOF_I2C_FREQ);
    }

    for (int i = 0; i < TOF_NUM; i++) {
        if (tofs[i].gpio < 32) { // valid GPIO
            gpio_init(tofs[i].gpio);
            gpio_set_function(tofs[i].gpio, GPIO_FUNC_SIO);
            gpio_set_drive_strength(tofs[i].gpio, GPIO_DRIVE_STRENGTH_12MA);
            gpio_set_dir(tofs[i].gpio, GPIO_OUT);
            gpio_put(tofs[i].gpio, 1);
        }
        sleep_ms(1);
        vl53l0x_init(i, tofs[i].port);
        vl53l1x_init(i, tofs[i].port);
        if (vl53l0x_is_present()) {
            tofs[i].model = TOF_VL53L0X;
            tofs[i].init_ok = vl53l0x_init_tof();
            vl53l0x_start_continuous();
        } else if (vl53l1x_is_present()) {
            tofs[i].model = TOF_VL53L1X;
            tofs[i].init_ok = vl53l1x_init_tof();
            vl53l1x_setROISize(geki_cfg->tof.roi, geki_cfg->tof.roi);
            vl53l1x_setDistanceMode(Short);
            vl53l1x_setMeasurementTimingBudget(20000);
            vl53l1x_startContinuous(20);
        } else {
            tofs[i].model = TOF_NONE;
        }
    }
}

static uint16_t tof_dist[TOF_NUM];
static bool readings[AIRKEY_NUM];

static void print_tof(const char *name, uint16_t mm)
{
    //printf("\t%s: %3d", name, mm > 1000 ? 0 : mm);
}

static void tof_read()
{
    for (int i = 0; i < TOF_NUM; i++) {
        if (tofs[i].model == TOF_VL53L0X) {
            vl53l0x_use(i);
            tof_dist[i] = readRangeContinuousMillimeters();
            print_tof("L0x", tof_dist[i]);
        } else if (tofs[i].model == TOF_VL53L1X) {
            vl53l1x_use(i);
            tof_dist[i] = vl53l1x_readContinuousMillimeters();
            print_tof("L1x", tof_dist[i]);
        }
    }
    //printf("\n");
}

static uint16_t mix_tof(airkey_side_t side, uint16_t primary, uint16_t secondary)
{
    tof_mix_algo_t algo = geki_cfg->tof.mix[side].algo;

    if (algo == MIX_PRIMARY) {
        return primary;
    }

    if (algo == MIX_SECONDARY) {
        return secondary;
    }

    if (geki_cfg->tof.mix[side].strict && (primary == 0 || secondary == 0)) {
        return 0;
    }

    int window = geki_cfg->tof.mix[side].window;
    if ((algo != MIX_AVG) || (window == 0)) {
        if (primary == 0) {
            return secondary;
        }
        if (secondary == 0) {
            return primary;
        }
    }

    if (algo == MIX_MAX) {
        return primary > secondary ? primary : secondary;
    }
    
    if (algo == MIX_MIN) {
        return primary < secondary ? primary : secondary;
    }
    
    if (algo == MIX_AVG) {
        int delta = primary - secondary;
        int max = primary;
        if (delta < 0) {
            delta = -delta;
            max = secondary;
        }
    
        if ((window == 0) || (window >= delta * 100 / max)) {
            return (primary + secondary) / 2;
        }
    }

    return 0;
}

#define BETWEEN(x, a, b) (((x) >= (a)) && ((x) <= (b)))
static bool airkey_read(unsigned index)
{
    airkey_side_t side = key_defs[index].side;
    
    int a_id = side * 2;
    int b_id = side * 2 + 1;

    uint16_t dist;

    if ((tofs[a_id].init_ok && tofs[b_id].init_ok) && 
        (tofs[a_id].model == tofs[b_id].model)) {
        dist = mix_tof(side, tof_dist[a_id], tof_dist[b_id]);
    } else if (tofs[a_id].init_ok) {
        dist = tof_dist[a_id];
    } else if (tofs[b_id].init_ok) {
        dist = tof_dist[b_id];
    } else {
        return false;
    }

    if (readings[index]) { // currently triggered
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
        return "None";
    }

    if (tofs[tof_id].model == TOF_VL53L0X) {
        return "VL53L0X";
    } else if (tofs[tof_id].model == TOF_VL53L1X) {
        return "VL53L1X";
    } else {
        return "None";
    }
}

void airkey_tof_update_roi()
{
    for (int i = 0; i < TOF_NUM; i++) {
        if (tofs[i].model == TOF_VL53L1X) {
            vl53l1x_use(i);
            vl53l1x_setROISize(geki_cfg->tof.roi, geki_cfg->tof.roi);
        }
    }
}
