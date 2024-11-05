/*
 * Controller Main
 * WHowe <github.com/whowechina>
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

#include "tusb.h"
#include "usb_descriptors.h"

#include "aime.h"
#include "nfc.h"

#include "board_defs.h"

#include "save.h"
#include "config.h"
#include "cli.h"
#include "commands.h"

#include "hid.h"

#include "light.h"
#include "button.h"
#include "lever.h"
#include "airkey.h"
#include "sound.h"

static void run_lights()
{
    light_set_pos(255 - lever_read(), rgb32(0xff, 0, 0, false));

    uint16_t button = button_read();

    light_set_aux(0, button & 0x40 ? 0xc0c0c0 : rgb32(0x60, 0, 0, false));
    light_set_aux(1, button & 0x80 ? 0xc0c0c0 : rgb32(0x50, 0x50, 0, false));

    uint32_t phase = time_us_32() >> 15;

    if (airkey_get_shift()) {
        uint32_t phase = (time_us_32() >> 15) % 3;
        for (int i = 0; i < 3; i++) {
            light_set(1 + i, phase % 3 == i ? 0x808080 : 0);
            light_set(33 + i, phase % 3 == i ? 0x808080 : 0);
        }
    } else {
        light_set_wad(0, airkey_get_left() ? rgb32(0x80, 0, 0xff, false) : 0);
        light_set_wad(1, airkey_get_right() ? rgb32(0x80, 0, 0xff, false) : 0);
    }

    for (int i = 0; i < 6; i++) {
        uint32_t color = rgb32_from_hsv(phase + i * 40, 0xff, 0x80);
        light_set_main(i, button & (1 << i) ? 0xffffff : color, false);
    }
}

static void run_sound()
{
    if (airkey_get_shift()) {
        sound_set(0, false);
        sound_set(1, false);
        return;
    }

    sound_set(0, airkey_get_left());
    sound_set(1, airkey_get_right());
}

const int aime_intf = 1;
static void cdc_aime_putc(uint8_t byte)
{
    tud_cdc_n_write(aime_intf, &byte, 1);
    tud_cdc_n_write_flush(aime_intf);
}

static void aime_run()
{
    if (tud_cdc_n_available(aime_intf)) {
        uint8_t buf[32];
        uint32_t count = tud_cdc_n_read(aime_intf, buf, sizeof(buf));

        for (int i = 0; i < count; i++) {
            aime_feed(buf[i]);
        }
    }
}

static mutex_t core1_io_lock;
static void core1_loop()
{
    while (1) {
        if (mutex_try_enter(&core1_io_lock, NULL)) {
            run_lights();
            run_sound();
            light_update();
            mutex_exit(&core1_io_lock);
        }
        cli_fps_count(1);
        sleep_us(700);
    }
}

static void core0_loop()
{
    while(1) {
        tud_task();

        cli_run();
        aime_run();

        save_loop();
        cli_fps_count(0);

        button_update();
        airkey_update();

        hid_update();

        sleep_us(900);
    }
}

/* if certain key pressed when booting, enter update mode */
static void update_check()
{
    const uint8_t pins[] = BUTTON_DEF;
    int pressed = 0;
    for (int i = 0; i < count_of(pins); i++) {
        uint8_t gpio = pins[i];
        gpio_init(gpio);
        gpio_set_function(gpio, GPIO_FUNC_SIO);
        gpio_set_dir(gpio, GPIO_IN);
        gpio_pull_up(gpio);
        sleep_ms(1);
        if (!gpio_get(gpio)) {
            pressed++;
        }
    }

    if (pressed >= 4) {
        sleep_ms(100);
        reset_usb_boot(0, 2);
        return;
    }
}

void init()
{
    sleep_ms(50);
    board_init();

    update_check();

    tusb_init();
    stdio_init_all();

    config_init();
    mutex_init(&core1_io_lock);
    save_init(0xca44caac, &core1_io_lock);

    light_init();
    button_init();
    lever_init();
    airkey_init();
    sound_init();

    nfc_attach_i2c(PN532_I2C_PORT);
    nfc_init();
    aime_init(cdc_aime_putc);
    aime_virtual_aic(geki_cfg->aime.virtual_aic);
    aime_sub_mode(geki_cfg->aime.mode);

    cli_init("geki_pico>", "\n   << Geki Pico Controller >>\n"
                            " https://github.com/whowechina\n\n");
    
    commands_init();
}

int main(void)
{
    init();
    multicore_launch_core1(core1_loop);
    core0_loop();
    return 0;
}

struct __attribute__((packed)) {
    uint16_t buttons;
    uint8_t  HAT;
    uint32_t axis;
} hid_joy_out = {0};

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
    printf("Get from USB %d-%d\n", report_id, report_type);
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize)
{
    hid_proc(buffer, bufsize);
}
