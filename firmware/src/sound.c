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

#include "config.h"
#include "board_defs.h"

static const uint8_t sound_gpio[2] = SOUND_DEF;
static int slice_num[2];
static int wav_pos[2];
static const int wad_sound_len = sizeof(RING_DATA);
static const uint8_t *wad_sound = RING_DATA;
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

        int pos = wav_pos[i] >> 3;
        if (pos >= wad_sound_len) {
            pos = 0;
            wav_pos[i] = 0;
        }
    
        static int amplitude = 0;
        if (wav_pos[i] & 0x07) {
           amplitude = wad_sound[pos] * geki_cfg->sound.volume / 200;
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
    pwm_config_set_clkdiv(&cfg, 4.0f); // 8.0f: 11kHz, 4.0f: 22kHz, 2.0f: 44kHz
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
    if (!geki_cfg->sound.volume) {
        active[id] = false;
        return;
    }

    if (id >= 2) {
        return;
    }
    active[id] = on;
}
