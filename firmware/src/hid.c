#include <stdint.h>
#include <stdbool.h>

#include "board_defs.h"

#include "tusb.h"
#include "usb_descriptors.h"
#include "button.h"
#include "gimbal.h"
#include "airkey.h"
#include "config.h"
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

const static struct {
    uint8_t group;
    uint8_t bit;
} button_to_io4_map[] = {
    { 0, 0 }, { 0, 5 }, { 0, 4 }, // Left ABC
    { 0, 1 }, { 1, 0 }, { 0, 15 }, // Right ABC
    { 1, 14 }, { 0, 13 }, // AUX 12
}, wad_left = { 1, 15 }, wad_right =  { 0, 14 };


static void report_usb_hid()
{
    if (tud_hid_ready()) {
        if (geki_cfg->hid.joy || geki_runtime.key_stuck) {
            hid_joy.adcs[0] = (gimbal_read() - 128) << 8;

            static uint16_t last_buttons = 0;
            uint16_t buttons = button_read();
            hid_joy.buttons[0] = 0;
            hid_joy.buttons[1] = 0;
            for (int i = 0; i < button_num(); i++) {
                uint8_t group = button_to_io4_map[i].group;
                uint8_t bit = button_to_io4_map[i].bit;
                if (buttons & (1 << i)) {
                    hid_joy.buttons[group] |= (1 << bit);
                }
            }
            if (airkey_get(0)) {
                hid_joy.buttons[wad_left.group] |= (1 << wad_left.bit);
            }
            if (airkey_get(1)) {
                hid_joy.buttons[wad_right.group] |= (1 << wad_right.bit);
            }

            if ((last_buttons ^ buttons) & (1 << 11)) {
                if (buttons & (1 << 11)) {
                   // just pressed coin button
                   hid_joy.chutes[0] += 0x100;
                }
            }
            tud_hid_n_report(0, REPORT_ID_JOYSTICK, &hid_joy, sizeof(hid_joy));
            last_buttons = buttons;
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

void hid_proc(const uint8_t *data, uint8_t len)
{
    hid_output_t *output = (hid_output_t *)data;
    if (output->report_id == REPORT_ID_OUTPUT) {
        switch (output->cmd) {
            case 0x01: // Set Timeout
            case 0x02: // Set Sampling Count
                hid_joy.system_status = 0x30;
                break;
            case 0x03: // Clear Board Status
                hid_joy.chutes[0] = 0;
                hid_joy.chutes[1] = 0;
                hid_joy.system_status = 0x00;
                break;
            case 0x04: // Set General Output
                // LED
                break;
            case 0x41: // I don't know what this is
                break;
            default:
                printf("USB unknown cmd: %d\n", output->cmd);
                break;
        }
    }
}
