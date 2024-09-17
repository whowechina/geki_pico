/*
 * Lever Input
 * WHowe <github.com/whowechina>
 * 
 */

#include "lever.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"

#include "config.h"
#include "board_defs.h"

void lever_init()
{
    gpio_init(AXIS_MUX_PIN_A);
    gpio_set_dir(AXIS_MUX_PIN_A, GPIO_OUT);

    gpio_init(AXIS_MUX_PIN_B);
    gpio_set_dir(AXIS_MUX_PIN_B, GPIO_OUT);

    adc_init();
    adc_gpio_init(26 + ADC_CHANNEL);
    adc_select_input(ADC_CHANNEL);
}

uint8_t lever_read()
{
    uint16_t val = lever_average();
    const uint16_t min = geki_cfg->lever.min;
    const uint16_t max = geki_cfg->lever.max;

    if (val < min) {
        val = min;
    } else if (val > max) {
        val = max;
    }

    uint16_t range = max - min;
    if (!range) {
        range = 100;
    }
    
    uint8_t result = (val - min) * 255 / range;
    if (geki_cfg->lever.invert) {
        result = 255 - result;
    }

    return result;
}

uint16_t lever_raw()
{
    static uint16_t last_read = 2048;
    const uint16_t rate_limit = 5;

    uint16_t val = adc_read();
    if (val > last_read + rate_limit) {
        last_read += rate_limit;
    } else if (val < last_read - rate_limit) {
        last_read -= rate_limit;
    } else {
        last_read = val;
    }

    return last_read;
}

#define LEVER_AVERAGE_COUNT 32
uint16_t lever_average()
{
    static uint16_t buf[LEVER_AVERAGE_COUNT] = {0};
    static int index = 0;
    index = (index + 1) % LEVER_AVERAGE_COUNT;
    buf[index] = lever_raw();

    uint32_t sum = 0;
    for (int i = 0; i < LEVER_AVERAGE_COUNT; i++) {
        sum += buf[i];
    }

    return sum / LEVER_AVERAGE_COUNT;
}