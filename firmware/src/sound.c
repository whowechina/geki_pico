/*
 * Sound Feedback
 * WHowe <github.com/whowechina>
 * 
 */

#include "sound.h"

#include <stdint.h>
#include <stdbool.h>

#include "hardware/gpio.h"

#include "config.h"
#include "board_defs.h"

static const uint8_t sound_gpio[2] = SOUND_DEF;

void sound_init()
{
    for (int i = 0; i < 2; i++)
    {
        uint8_t gpio = sound_gpio[i];
        gpio_init(gpio);
        gpio_set_dir(gpio, GPIO_OUT);
        gpio_put(gpio, false);
    }
}

void sound_set(int id, bool on)
{
    if (!geki_cfg->sound.enabled) {
        gpio_put(sound_gpio[id], false);
        return;
    }

    if (id >= 2) {
        return;
    }

    gpio_put(sound_gpio[id], on);
}
