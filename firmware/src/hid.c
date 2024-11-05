#include <stdint.h>
#include <stdbool.h>

#include "board_defs.h"

#include "tusb.h"
#include "usb_descriptors.h"
#include "button.h"
#include "lever.h"
#include "airkey.h"
#include "config.h"
#include "light.h"

#include "hid.h"

struct __attribute__((packed)) {
    uint16_t adcs[8];
    uint16_t spinners[4];
    uint16_t chutes[2];
    uint16_t buttons[2];
    uint8_t system_status;
    uint8_t usb_status;
    uint8_t padding[29];
} hid_joy;

struct __attribute__((packed)) {
    uint8_t modifier;
    uint8_t keymap[15];
} hid_nkro;

static void gen_hid_analogs()
{
    hid_joy.adcs[0] = lever_read() << 8;
}
static void gen_hid_buttons()
{
    const static struct {
        uint8_t group;
        uint8_t bit;
    } button_to_io4_map[] = {
        { 0, 0 }, { 0, 5 }, { 0, 4 }, // Left ABC
        { 0, 1 }, { 1, 0 }, { 0, 15 }, // Right ABC
        { 1, 14 }, { 0, 13 }, // AUX 12
    },
    wad_left = { 1, 15 },
    wad_right =  { 0, 14 },
    key_test = { 0, 9 },
    key_service = { 0, 6 };

    uint16_t buttons = button_read();

    hid_joy.buttons[0] = 0;
    hid_joy.buttons[1] = 0;

    if (airkey_get_shift()) {
        if (buttons & 0x40) {
            hid_joy.buttons[key_test.group] |= (1 << key_test.bit);
        }
        if (buttons & 0x80) {
            hid_joy.buttons[key_service.group] |= (1 << key_service.bit);
        }
        return;
    }

    for (int i = 0; i < button_num(); i++) {
        uint8_t group = button_to_io4_map[i].group;
        uint8_t bit = button_to_io4_map[i].bit;
        if (buttons & (1 << i)) {
            hid_joy.buttons[group] |= (1 << bit);
        }
    }

    if (!airkey_get_left()) {
        hid_joy.buttons[wad_left.group] |= (1 << wad_left.bit);
    }
    if (!airkey_get_right()) {
        hid_joy.buttons[wad_right.group] |= (1 << wad_right.bit);
    }
}

static void gen_hid_coins()
{
    static uint8_t last_lever = 0;
    uint8_t lever = lever_read();
    static int dec_count = 0;

    if (airkey_get_shift()) {
        if (lever < last_lever) {
            dec_count++;
        } else if (lever > last_lever) {
            dec_count = 0;
        }

        if (dec_count > 60) {
            dec_count = 0;
            hid_joy.chutes[0] += 0x100;
        }
    }

    last_lever = lever;
}
static void report_usb_hid()
{
    if (tud_hid_ready()) {
        if (geki_cfg->hid.joy || geki_runtime.key_stuck) {
            gen_hid_analogs();
            gen_hid_buttons();
            gen_hid_coins();
            tud_hid_n_report(0, REPORT_ID_JOYSTICK, &hid_joy, sizeof(hid_joy));
        }
        if (geki_cfg->hid.nkro && !geki_runtime.key_stuck) {
            tud_hid_n_report(1, 0, &hid_nkro, sizeof(hid_nkro));
        }
    }
}

static void gen_nkro_report()
{
    if (!geki_cfg->hid.nkro) {
        return;
    }

    return;
    const char keymap[] = "\x1a\x08\x07\x06\x1b\x1d\x04\x14\x20\x3a\x3b\x3c";
    uint16_t buttons = button_read();
    for (int i = 0; i < button_num(); i++) {
        uint8_t code = keymap[i];
        uint8_t byte = code / 8;
        uint8_t bit = code % 8;
        if (buttons & (1 << i)) {
            hid_nkro.keymap[byte] |= (1 << bit);
        } else {
            hid_nkro.keymap[byte] &= ~(1 << bit);
        }
    }
}

void hid_update()
{
    gen_nkro_report();
    report_usb_hid();
}

typedef struct __attribute__((packed)) {
    uint8_t report_id;
    uint8_t cmd;
    uint8_t payload[62];
} hid_output_t;

static void update_led(const uint8_t data[4])
{
    const uint8_t led_bit[18] = { 30, 31, 28, 26, 27, 29, 23, 25, 24,
                                  20, 22, 21, 17, 19, 18, 14, 16, 15 };

    uint32_t leds = (data[0] << 24) | (data[1] << 16) |
                   (data[2] << 8) | data[3];

    for (uint8_t i = 0; i < 6; i++) {
        bool r = leds & (1 << led_bit[i * 3 + 1]);
        bool g = leds & (1 << led_bit[i * 3 + 2]);
        bool b = leds & (1 << led_bit[i * 3]);
        uint32_t color = rgb32(r ? 0xff : 0, g ? 0xff : 0, b ? 0xff : 0, false);
        light_set_main(i, color, true);
    }
}

void hid_proc(const uint8_t *data, uint8_t len)
{
    hid_output_t *output = (hid_output_t *)data;
    if (output->report_id == REPORT_ID_OUTPUT) {
        switch (output->cmd) {
            case 0x01: // Set Timeout
            case 0x02: // Set Sampling Count
                hid_joy.system_status = 0x30;
                printf("USB set timeout/sampling\n");
                break;
            case 0x03: // Clear Board Status
                hid_joy.chutes[0] = 0;
                hid_joy.chutes[1] = 0;
                hid_joy.system_status = 0x00;
                printf("USB clear status\n");
                break;
            case 0x04: // Set General Output
                // LED
                update_led(output->payload);
                break;
            case 0x41: // I don't know what this is
                break;
            default:
                printf("USB unknown cmd: %d\n", output->cmd);
                break;
        }
    } 
}
