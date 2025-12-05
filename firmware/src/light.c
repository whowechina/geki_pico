/*
 * WS2812B Lights Control (Base + Left and Right Gimbals)
 * WHowe <github.com/whowechina>
 * 
 */

#include "light.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "hardware/pio.h"
#include "hardware/timer.h"

#include "ws2812.pio.h"

#include "board_defs.h"
#include "config.h"

#define HID_TIMEOUT 30*1000*1000

static uint32_t int_buf[37]; // left 3 + right 3 + button 4 * 7 + indicator 5
static uint32_t ext_buf[2][count_of(geki_cfg->extled.map[0])];

static uint32_t pio_int[count_of(int_buf)];
static uint32_t pio_ext[2][count_of(ext_buf[0])];

uint32_t rgb32(uint32_t r, uint32_t g, uint32_t b, bool gamma_fix)
{
    if (gamma_fix) {
        r = ((r + 1) * (r + 1) - 1) >> 8;
        g = ((g + 1) * (g + 1) - 1) >> 8;
        b = ((b + 1) * (b + 1) - 1) >> 8;
    }

    return (r << 16) | (g << 8) | (b << 0);
}

uint32_t rgb32_from_hsv(uint8_t h, uint8_t s, uint8_t v)
{
    uint32_t region, remainder, p, q, t;

    if (s == 0) {
        return v << 16 | v << 8 | v;
    }

    region = h / 43;
    remainder = (h % 43) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:
            return v << 16 | t << 8 | p;
        case 1:
            return q << 16 | v << 8 | p;
        case 2:
            return p << 16 | v << 8 | t;
        case 3:
            return p << 16 | q << 8 | v;
        case 4:
            return t << 16 | p << 8 | v;
        default:
            return v << 16 | p << 8 | q;
    }
}

uint32_t load_color(const rgb_hsv_t *color)
{
    if (color->rgb_hsv == 0) {
        return rgb32(color->val[0], color->val[1], color->val[2], false);
    } else {
        return rgb32_from_hsv(color->val[0], color->val[1], color->val[2]);
    }
}

static inline uint32_t fix_order(uint32_t color, uint8_t order)
{
    uint8_t c1 = (color >> 16) & 0xff;
    uint8_t c2 = (color >> 8) & 0xff;
    uint8_t c3 = color & 0xff;

    if (order == 0) { // GRB
        return c2 << 16 | c1 << 8 | c3;
    } else {
        return color;
    }
}

static inline uint32_t apply_level(uint32_t color)
{
    unsigned r = (color >> 16) & 0xff;
    unsigned g = (color >> 8) & 0xff;
    unsigned b = color & 0xff;

    r = r * geki_cfg->light.level / 255;
    g = g * geki_cfg->light.level / 255;
    b = b * geki_cfg->light.level / 255;

    return r << 16 | g << 8 | b;
}

static void prepare_pio_buffer()
{
    for (int i = 0; i < count_of(pio_int); i++) {
        uint32_t color = fix_order(int_buf[i], geki_cfg->light.rgb_order);
        pio_int[i] = apply_level(color) << 8;
    }
    for (int i = 0; i < count_of(pio_ext[0]); i++) {
        uint32_t color = fix_order(ext_buf[0][i], geki_cfg->light.ext_rgb_order);
        pio_ext[0][i] = apply_level(color) << 8;
        color = fix_order(ext_buf[1][i], geki_cfg->light.ext_rgb_order);
        pio_ext[1][i] = apply_level(color) << 8;
    }
}

static void drive_led()
{
    for (int i = 0; i < count_of(pio_int); i++) {
        pio_sm_put_blocking(pio0, 0, pio_int[i]);
    }
    for (int i = 0; i < count_of(pio_ext[0]); i++) {
        pio_sm_put_blocking(pio0, 1, pio_ext[0][i]);
        pio_sm_put_blocking(pio0, 2, pio_ext[1][i]);
    }
}

void light_init()
{
    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, RGB_INT_PIN, 800000, false);
    ws2812_program_init(pio0, 1, offset, RGB_EXT_A_PIN, 800000, false);
    ws2812_program_init(pio0, 2, offset, RGB_EXT_B_PIN, 800000, false);
}

void light_update()
{
    static uint64_t last = 0;
    uint64_t now = time_us_64();
    if (now - last < 5000) { // 200Hz
        return;
    }

    last = now;

    prepare_pio_buffer();
    drive_led();
}

void light_set(uint8_t index, uint32_t color)
{
    if (index >= count_of(int_buf)) {
        return;
    }
    int_buf[index] = color;
}

static uint64_t hid_timeout = 0;

void light_hid_heartbeat()
{
    hid_timeout = time_us_64() + HID_TIMEOUT;
}

static bool hid_is_active()
{
    return (hid_timeout > 0) && (time_us_64() < hid_timeout);
}

void light_set_main(uint8_t index, uint32_t color, bool hid)
{
    if (hid) {
        light_hid_heartbeat();
    } else if (hid_is_active()) {
        return;
    }

    if (index < 3) {
        light_set(index * 4 + 4, color);
        light_set(index * 4 + 5, color);
        light_set(index * 4 + 6, color);
        light_set(index * 4 + 7, color);
    } else if (index < 6) {
        light_set(index * 4 + 9, color);
        light_set(index * 4 + 10, color);
        light_set(index * 4 + 11, color);
        light_set(index * 4 + 12, color);
    }
}

void light_set_aux(uint8_t index, uint32_t color)
{
    if (index == 0) {
        light_set(0, color);
    } else if (index == 1) {
        light_set(36, color);
    }
}

void light_set_wad(uint8_t index, uint32_t color)
{
    if (index == 0) {
        light_set(1, color);
        light_set(2, color);
        light_set(3, color);
    } else if (index == 1) {
        light_set(33, color);
        light_set(34, color);
        light_set(35, color);
    }
}

void light_set_pos(uint8_t pos, uint32_t color)
{
    pos = pos * 5 / 256;
    for (int i = 0; i < 5; i++) {
        light_set(16 + i, (i == pos) ? color : 0);
    }
}

void light_set_aime(uint32_t color)
{
    for (int i = 0; i < 5; i++) {
        light_set(16 + i, color);
    }
}

void light_set_ext(uint8_t chn, uint8_t index, uint32_t color)
{
    if ((chn >= 2) || (index >= count_of(ext_buf[0]))) {
        return;
    }
    ext_buf[chn][index] = color;
}

void light_set_ext_all(uint8_t chn, uint32_t color)
{
    for (int i = 0; i < count_of(ext_buf[0]); i++) {
        ext_buf[chn][i] = color;
    }
}
