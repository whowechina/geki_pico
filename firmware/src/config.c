/*
 * Controller Config and Runtime Data
 * WHowe <github.com/whowechina>
 * 
 * Config is a global data structure that stores all the configuration
 * Runtime is something to share between files.
 */

#include "config.h"
#include "save.h"

geki_cfg_t *geki_cfg;

static geki_cfg_t default_cfg = {
    .lever = {
         2000, 2500, 0,
    },
    .light = {
        .level = 128,
        .reserved = { 0 },
    },
    .tof = {
        .roi = 12,
        .reserved = { 0 },
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
