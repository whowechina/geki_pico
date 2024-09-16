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

static uint32_t buf_rgb[37]; // left 3 + right 3 + button 4 * 7 + indicator 5
static bool bind[37] = { 0 };


static inline uint32_t _rgb32(uint32_t c1, uint32_t c2, uint32_t c3, bool gamma_fix)
{
    if (gamma_fix) {
        c1 = ((c1 + 1) * (c1 + 1) - 1) >> 8;
        c2 = ((c2 + 1) * (c2 + 1) - 1) >> 8;
        c3 = ((c3 + 1) * (c3 + 1) - 1) >> 8;
    }

    return (c1 << 16) | (c2 << 8) | (c3 << 0);
}

uint32_t rgb32(uint32_t r, uint32_t g, uint32_t b, bool gamma_fix)
{
#if RGB_ORDER == GRB
    return _rgb32(g, r, b, gamma_fix);
#else
    return _rgb32(r, g, b, gamma_fix);
#endif
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

static void drive_led()
{
    for (int i = 0; i < count_of(buf_rgb); i++) { \
        pio_sm_put_blocking(pio0, 0, buf_rgb[i] << 8u); \
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

void light_init()
{
    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, RGB_PIN, 800000, false);
}

static void light_effect()
{
    return;
    static uint32_t loop = 0;
    loop++;

    for (int i = 0; i < count_of(buf_rgb); i++) {
        uint32_t hue = (loop + i * 255 / count_of(buf_rgb)) % 255;
        if (!bind[i]) {
            buf_rgb[i] = rgb32_from_hsv(hue, 255, 255);
        }
    }
}

void light_update()
{
    static uint64_t last = 0;
    uint64_t now = time_us_64();
    if (now - last < 5000) { // 200Hz
        return;
    }

    last = now;

    light_effect();
    drive_led();
}

void light_set(uint8_t index, uint32_t color)
{
    if (index >= count_of(buf_rgb)) {
        return;
    }
    buf_rgb[index] = apply_level(color);
    bind[index] = true;
}

void light_set_main(uint8_t index, uint32_t color)
{
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

void light_unset(uint8_t index)
{
    if (index >= count_of(buf_rgb)) {
        return;
    }

    bind[index] = false;
}
