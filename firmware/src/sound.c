/*
 * Sound Feedback
 * WHowe <github.com/whowechina>
 * 
 */

#include "sound.h"

#include <stdint.h>
#include <stdbool.h>

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"

#include "ring.h"
#include "music.h"

#include "config.h"
#include "board_defs.h"

static const uint8_t sound_gpio[2] = SOUND_DEF;
static int slice_num[2];
static int wav_pos[2];
static const int wav_len[2] = { sizeof(MUSIC_DATA), sizeof(RING_DATA)};
static const uint8_t *wav_data[2] = { MUSIC_DATA, RING_DATA};
static bool active[2];

void pwm_interrupt_handler()
{
    for (int i = 0; i < 2; i++) {
        pwm_clear_irq(slice_num[i]);

        uint8_t gpio = sound_gpio[i];

        if (!active[i]) {
            wav_pos[i] = 0;
            pwm_set_gpio_level(gpio, 0);
            continue;
        }

        int len = wav_len[i];
        const uint8_t *data = wav_data[i];
        if (wav_pos[i] < (len << 3) - 1) { 
            pwm_set_gpio_level(gpio, data[wav_pos[i] >> 3]);
            wav_pos[i]++;
        } else {
            wav_pos[i] = 0;
        }
    }
}

void sound_init()
{
    for (int i = 0; i < 2; i++)
    {
        uint8_t gpio = sound_gpio[i];
        gpio_init(gpio);
        gpio_set_function(gpio, GPIO_FUNC_PWM);
        slice_num[i] = pwm_gpio_to_slice_num(gpio);
        pwm_clear_irq(slice_num[i]);
        pwm_set_irq_enabled(slice_num[i], true);
    }
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_interrupt_handler); 
    irq_set_enabled(PWM_IRQ_WRAP, true);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 8.0f); 
    pwm_config_set_wrap(&cfg, 250); 

    for (int i = 0; i < 2; i++) {
        pwm_init(slice_num[i], &cfg, true);
        pwm_set_gpio_level(sound_gpio[i], 0);
    }
}

void sound_toggle(bool on)
{
    for (int i = 0; i < 2; i++) {
        pwm_set_irq_enabled(slice_num[i], on);
    }
}

void sound_set(int id, bool on)
{
    if (!geki_cfg->sound.enabled) {
        active[id] = false;
        return;
    }

    if (id >= 2) {
        return;
    }
    active[id] = on;
}
