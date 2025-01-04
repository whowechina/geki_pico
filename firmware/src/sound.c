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

#include "wad_on_wav.h"
#include "wad_off_wav.h"

#include "config.h"
#include "board_defs.h"

static const uint8_t sound_gpio[2] = SOUND_DEF;
static int slice_num[2];
static int wav_pos[2];

typedef struct {
    const int sound_len;
    const uint8_t *sound_data;
} sound_t;

static const sound_t sound_on = { sizeof(wad_on), wad_on };
static const sound_t sound_off = { sizeof(wad_off), wad_off };

static const sound_t *sound_now[2] = { &sound_on, &sound_off };
static bool playing[2];

void pwm_interrupt_handler()
{
    for (int i = 0; i < 2; i++) {
        pwm_clear_irq(slice_num[i]);

        uint8_t gpio = sound_gpio[i];

        if (!playing[i]) {
            wav_pos[i] = 0;
            pwm_set_gpio_level(gpio, 0);
            continue;
        }

        int pos = wav_pos[i] >> 3;
        if (pos >= sound_now[i]->sound_len) {
            pos = 0;
            wav_pos[i] = 0;
            playing[i] = false;
        }
    
        static int amplitude = 0;
        if (wav_pos[i] & 0x07) {
           amplitude = sound_now[i]->sound_data[pos] * geki_cfg->sound.volume / 200;
           if (amplitude > 250) {
               amplitude = 250;
           }
        }
        pwm_set_gpio_level(gpio, amplitude);
        wav_pos[i]++;
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
    pwm_config_set_clkdiv(&cfg, 5.5f); // 8.0f: 11kHz, 5.5f: 16kHz, 4.0f: 22kHz, 2.0f: 44kHz
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
    static bool was_on[2] = { false, false };
    if (!geki_cfg->sound.volume) {
        playing[id] = false;
        return;
    }

    if (id >= 2) {
        return;
    }
    
    bool changed = on != was_on[id];
    was_on[id] = on;
    if (changed) {
        playing[id] = true;
        wav_pos[id] = 0;
        sound_now[id] = on ? &sound_on : &sound_off;
    }
}
