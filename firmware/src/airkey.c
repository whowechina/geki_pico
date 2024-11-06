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

#define AIRKEY_NUM 3
static bool airkeys[AIRKEY_NUM];
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
static uint16_t tof_mix[2];

static void tof_read()
{
    for (int i = 0; i < TOF_NUM; i++) {
        if (!tofs[i].init_ok) {
            tof_dist[i] = 0;
            continue;
        }
    
        if (tofs[i].model == TOF_VL53L0X) {
            vl53l0x_use(i);
            tof_dist[i] = readRangeContinuousMillimeters();
        } else if (tofs[i].model == TOF_VL53L1X) {
            vl53l1x_use(i);
            tof_dist[i] = vl53l1x_readContinuousMillimeters();
        }
        if (tof_dist[i] >= 1000) { // treat >= 1M as invalid
            tof_dist[i] = 0;
        }
    }
}

static uint16_t mix_dist(airkey_side_t side, uint16_t primary, uint16_t secondary)
{
    tof_mix_algo_t algo = geki_cfg->tof.mix[side].algo;
    bool strict = geki_cfg->tof.mix[side].strict;

    if (algo == MIX_PRIMARY) {
        return (strict || (primary != 0)) ? primary : secondary;
    }

    if (algo == MIX_SECONDARY) {
        return (strict || (secondary != 0)) ? secondary : primary;
    }

    if (strict && (primary == 0 || secondary == 0)) {
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
    
        window *= 5; // *5 percentage
        if ((window == 0) || (window >= delta * 100 / max)) {
            return (primary + secondary) / 2;
        }
    }
    return 0;
}

static void calc_mix()
{
    for (int side = 0; side < 2; side++) {
        int a = side * 2;
        int b = side * 2 + 1;
        if ((tofs[a].init_ok && tofs[b].init_ok) && 
            (tofs[a].model == tofs[b].model)) {
            tof_mix[side] = mix_dist(side, tof_dist[a], tof_dist[b]);
        } else if (tofs[a].init_ok) {
            tof_mix[side] = tof_dist[a];
        } else if (tofs[b].init_ok) {
            tof_mix[side] = tof_dist[b];
        } else {
            tof_mix[side] = 0;
        }
    }
}

static inline bool in_bound(uint16_t value, uint16_t low, uint16_t high)
{
    return ((value >= low) && (value <= high));
}

static bool airkey_read(unsigned index)
{
    uint16_t dist = (index == 0) ? tof_mix[SIDE_LEFT] : tof_mix[SIDE_RIGHT];
    if (dist == 0) {
        return false;
    }

    typeof(geki_cfg->tof.trigger[0]) trigger = geki_cfg->tof.trigger[index];
    if (airkeys[index]) { // currently triggered
        return in_bound(dist, trigger.out_low, trigger.out_high);
    } else {
        return in_bound(dist, trigger.in_low, trigger.in_high);
    }
}

static void tof_diag()
{
    if (!geki_runtime.tof_diag) {
        return;
    }

    const char *models[] = { "NA", "0X", "1X" };
    printf("TOF:");
    for (int i = 0; i < 2; i++) {
        printf("\t%s-%3d, %s-%3d->%4d", models[tofs[i * 2].model], tof_dist[i * 2],
               models[tofs[i * 2 + 1].model], tof_dist[i * 2 + 1], tof_mix[i]);
    }
    printf("\n");
}

/* If a switch flips, it freezes for a while */
#define DEBOUNCE_FREEZE_TIME_US 30000
void airkey_update()
{
    uint64_t now = time_us_64();

    tof_read();
    calc_mix();
    tof_diag();

    for (int i = 0; i < AIRKEY_NUM; i++) {
        bool triggered = airkey_read(i);
        if (now >= sw_freeze_time[i]) {
            if (triggered != sw_val[i]) {
                sw_val[i] = triggered;
                sw_freeze_time[i] = now + DEBOUNCE_FREEZE_TIME_US;
            }
        }
        airkeys[i] = sw_val[i];
    }
}

bool airkey_get_left()
{
    return airkeys[0];
}

bool airkey_get_right()
{
    return airkeys[1];
}

bool airkey_get_shift()
{
    return airkeys[2];
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
