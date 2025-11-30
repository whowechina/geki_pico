/*
 * Extended LED Signals from USB Serial
 * WHowe <github.com/whowechina>
 * 
 */

#include "extled.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "tusb.h"

#include "light.h"

#include "config.h"
#include "board_defs.h"
 
#define LED_DATA_TIMEOUT_US 2*1000*1000
const int led_intf = 2;

static uint8_t extled_buf[61 * 3];
static uint64_t extled_timeout = 0;

static struct {
    bool idle;
    bool escaping;
    int pos;
    uint8_t buf[count_of(extled_buf) + 1];
} reader;

static void proc_frame()
{
    if (reader.buf[0] != 0) { // we only need board id 0
        return;
    }
    memcpy(extled_buf, &reader.buf[1], sizeof(extled_buf));
    extled_timeout = time_us_64() + LED_DATA_TIMEOUT_US;
}

static inline void buf_feed(uint8_t byte)
{
    if (reader.idle) {
        if (byte == 0xe0) {
            reader.idle = false;
        }
        return;
    }

    if ((byte == 0xd0) && !reader.escaping) {
        reader.escaping = true;
        return;
    }

    if (reader.escaping) {
        reader.escaping = false;
        byte++;
    }

    reader.buf[reader.pos] = byte;
    reader.pos++;

    if (reader.pos >= sizeof(reader.buf)) {
        reader.pos = 0;
        reader.idle = true;
        reader.escaping = false;
        proc_frame();
    }
}

static void reader_poll_data()
{
    static uint8_t buf[240];
    if (!tud_cdc_n_available(led_intf)) {
        return;
    }

    int count = tud_cdc_n_read(led_intf, buf, sizeof(buf));
    for (int i = 0; i < count; i++) {
        buf_feed(buf[i]);
    }
}

static uint32_t extract_rgb(uint8_t index)
{
    if (index >= (sizeof(reader.buf) - 1) / 3) {
        return 0;
    }
    uint8_t *rgb = reader.buf + index * 3 + 1;
    return rgb[0] << 16 | rgb[1] << 8 | rgb[2];
}

void extled_init()
{
    reader.pos = 0;
}

bool extled_is_active()
{
    return time_us_64() < extled_timeout;
}

static void update_lights()
{
    if (!extled_is_active()) {
        return;
    }

    for (int chn = 0; chn < 2; chn++) {
        for (int i = 0; i < count_of(geki_cfg->extled.map[0]); i++) {
            uint32_t color = extract_rgb(geki_cfg->extled.map[chn][i]);
            light_set_ext(chn, i, color);
        }
    }

    light_set_wad(0, extract_rgb(0));
    light_set_wad(1, extract_rgb(60));
}

void extled_update()
{
    reader_poll_data();
    update_lights();
}
