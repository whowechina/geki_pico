/*
 * Controller Config and Runtime Data
 * WHowe <github.com/whowechina>
 * 
 * Config is a global data structure that stores all the configuration
 * Runtime is something to share between files.
 */

#include "config.h"
#include "save.h"

#include "airkey.h"

geki_cfg_t *geki_cfg;

geki_cfg_t default_cfg = {
    .lever = {
         2000, 2500, 0,
    },
    .light = {
        .level = 128,
        .reserved = { 0 },
    },
    .tof = {
        .roi = 12,
        .mix = {
            { .strict = 1, .algo = MIX_MAX, .window = 0 },
            { .strict = 1, .algo = MIX_MAX, .window = 0 },
        },
        .trigger = {
            { 100, 260, 60, 300 },
            { 100, 260, 60, 300 },
            { 400, 500, 380, 530 },
        },
    },
    .sound = {
        .volume = 127,
        .reserved = { 0 },
    },
    .hid = {
        .joy = 1,
        .nkro = 0,
    },
};

geki_runtime_t geki_runtime;

static void config_loaded()
{
    for (int i = 0; i < 2; i++) {
        if (geki_cfg->tof.mix[i].algo > MIX_AVG) {
            geki_cfg->tof.mix[i].algo = default_cfg.tof.mix[i].algo;
            config_changed();
        }
    }

    for (int i = 0; i < 3; i++) {
        typeof(geki_cfg->tof.trigger[0]) trigger = geki_cfg->tof.trigger[i];
        if ((trigger.in_low == 0) || (trigger.in_high == 0) ||
            (trigger.out_low == 0) || (trigger.out_high == 0)) {
            geki_cfg->tof.trigger[i] = default_cfg.tof.trigger[i];
            config_changed();
        }
    }
}

void config_changed()
{
    save_request(false);
}

void config_factory_reset()
{
    *geki_cfg = default_cfg;
    save_request(true);
}

void config_init()
{
    geki_cfg = (geki_cfg_t *)save_alloc(sizeof(*geki_cfg), &default_cfg, config_loaded);
}
