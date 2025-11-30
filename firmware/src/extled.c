/*
 * Extended LED Signals from USB Serial
 * WHowe <github.com/whowechina>
 * 
 */

#include "extled.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "tusb.h"

#include "config.h"
#include "board_defs.h"
 
const int led_intf = 2;
static struct {
    uint8_t buf[128];
    int pos;
} reader;

static void reader_poll_data()
{
    if (tud_cdc_n_available(led_intf)) {
        int count = tud_cdc_n_read(led_intf, reader.buf + reader.pos,
                                   sizeof(reader.buf) - reader.pos);
        if (count > 0) {
            bool all_zero = true;
            for (int i = 1; i < count; i++) {
                if (reader.buf[reader.pos + i] != 0) {
                    all_zero = false;
                    break;
                }
            }
            if (all_zero) {
                return;
            }

            uint32_t now = time_us_32();
            printf("\n%6ld(%2d)>>", now / 1000, count);
            for (int i = 0; i < count; i++) {
                printf(" %02X", reader.buf[reader.pos + i]);
            }
            reader.pos += count;
        } else {
            printf("\n0");
        }
    }
}

void extled_init()
{
    reader.pos = 0;
}

void extled_update()
{
    reader_poll_data();

    if (reader.pos == 0) {
        return;
    }

    reader.pos = 0;
}