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
    .gimbal = {
         2000, 2500, 0, 80, 1,
    },
    .light = {
        .level = 128,
        .base = {
            {
                { 1, { 20, 150, 10 }, },
                { 1, { 147, 150, 10 }, },
            },
            {
                { 1, { 20, 150, 30 }, },
                { 1, { 147, 150, 30 }, },
            },
        },
        .button = {
            { 1, { 0, 0, 120 } },
            { 1, { 0, 0, 120 } },
        },
        .boost = {
            { 1, { 20, 255, 255 } },
            { 1, { 147, 255, 255 } },
        },
        .steer = {
            { 1, { 80, 255, 255 } },
            { 1, { 80, 255, 255 } },
        },
        .aux_on = { 0, { 100, 100, 100 } },
        .aux_off = { 0, { 8, 8, 8 } },
        .reserved = { 0 },
    },
    .sound = {
        .enabled = true,
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
