#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "config.h"
#include "save.h"
#include "cli.h"

#include "lever.h"

extern uint8_t RING_DATA[];

#include "nfc.h"
#include "aime.h"

#include "airkey.h"

#include "usb_descriptors.h"

#define SENSE_LIMIT_MAX 9
#define SENSE_LIMIT_MIN -9

static void disp_light()
{
    printf("[Light]\n");
    printf("  Level: %d.\n", geki_cfg->light.level);
}

static void disp_lever()
{
    printf("[Lever]\n");
    printf("  %s, raw %d-%d.\n",
            geki_cfg->lever.invert ? "invert" : "normal",
            geki_cfg->lever.min, geki_cfg->lever.max);
}

static void disp_sound()
{
    printf("[Sound]\n");
    printf("  Volume: %d\n", geki_cfg->sound.volume);
}

static void disp_hid()
{
    printf("[HID]\n");
    printf("  IO4: %s.\n", geki_cfg->hid.joy ? "ON" : "OFF");
}

static void disp_tof_status()
{
    for (int i = 0; i < airkey_tof_num(); i++) {
        printf("  TOF %d: %s", i, airkey_tof_model(i));
    }
    printf("\n");
}

static void disp_tof_trigger()
{
    const char *names[3] = { "WAD Left", "WAD Right", "Shift (Right)" };

    for (int i = 0; i < 3; i++) {
        typeof(geki_cfg->tof.trigger[0]) trigger = geki_cfg->tof.trigger[i];
        printf("  %s: in[%d-%d], out[%d-%d]\n", names[i],
            trigger.in_low, trigger.in_high, trigger.out_low, trigger.out_high);
    }
}

static void disp_tof_mix()
{
    for (int i = 0; i < 2; i++) {
        if (geki_cfg->tof.mix[i].algo > 4) {
            geki_cfg->tof.mix[i].algo = default_cfg.tof.mix[i].algo;
            config_changed();
        }
        const char *algos[] = { "Primary", "Secondary", "Max", "Min", "Avg" };
        printf("  %s: %s", i == 0 ? "Left Mix" : "Right Mix",
                           algos[geki_cfg->tof.mix[i].algo]);
        if (geki_cfg->tof.mix[i].algo == MIX_AVG) {
            const char *windows[] = { "No", "5%", "10%", "15%", "20%", "25%", "30%", "35%" };
            printf(", %s Window", windows[geki_cfg->tof.mix[i].window]);
        } else {
            printf("%s", geki_cfg->tof.mix[i].strict ? ", Strict" : "");
        }
        printf("\n");
    }
}

static void disp_tof_roi()
{
    printf("  ROI: %d (only for VL53L1X)\n", geki_cfg->tof.roi);
}

static void disp_tof()
{
    printf("[TOF]\n");
    disp_tof_status();
    disp_tof_mix();
    disp_tof_trigger();
    disp_tof_roi();
}

static void disp_aime()
{
    printf("[AIME]\n");
    printf("   NFC Module: %s\n", nfc_module_name());
    printf("  Virtual AIC: %s\n", geki_cfg->aime.virtual_aic ? "ON" : "OFF");
    printf("         Mode: %d\n", geki_cfg->aime.mode);
}

void handle_display(int argc, char *argv[])
{
    const char *usage = "Usage: display [light|sound|hid|lever|tof|aime]\n";
    if (argc > 1) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        disp_light();
        disp_lever();
        disp_sound();
        disp_hid();
        disp_tof();
        disp_aime();
        return;
    }

    const char *choices[] = {"light", "lever", "sound", "hid", "tof", "aime"};
    switch (cli_match_prefix(choices, count_of(choices), argv[0])) {
        case 0:
            disp_light();
            break;
        case 1:
            disp_lever();
            break;
        case 2:
            disp_sound();
            break;
        case 3:
            disp_hid();
            break;
        case 4:
            disp_tof();
            break;
        case 5:
            disp_aime();
            break;
        default:
            printf(usage);
            break;
    }
}

static int fps[2];
void fps_count(int core)
{
    static uint32_t last[2] = {0};
    static int counter[2] = {0};

    counter[core]++;

    uint32_t now = time_us_32();
    if (now - last[core] < 1000000) {
        return;
    }
    last[core] = now;
    fps[core] = counter[core];
    counter[core] = 0;
}

static void handle_level(int argc, char *argv[])
{
    const char *usage = "Usage: level <0..255>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    int level = cli_extract_non_neg_int(argv[0], 0);
    if ((level < 0) || (level > 255)) {
        printf(usage);
        return;
    }

    geki_cfg->light.level = level;
    config_changed();
    disp_light();
}

static void handle_hid(int argc, char *argv[])
{
    const char *usage = "Usage: hid <io4|off>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    const char *choices[] = {"io4", "off"};
    int match = cli_match_prefix(choices, count_of(choices), argv[0]);
    if (match < 0) {
        printf(usage);
        return;
    }

    geki_cfg->hid.joy = (match == 0) ? 1 : 0;
    config_changed();
    disp_hid();
}

static void calibrate_range(uint32_t seconds)
{
    uint32_t mins = lever_raw();
    uint32_t maxs = lever_raw();

    uint64_t start = time_us_64();

    while (time_us_64() - start < seconds * 1000000) {
        uint16_t val = lever_raw();
        printf("%4d\n", val);
        if (val < mins) {
            mins -= (mins - val) / 2;
        } else if (val > maxs) {
            maxs += (val - maxs) / 2;
        }
        sleep_ms(7);
    }

    geki_cfg->lever.min = mins;
    geki_cfg->lever.max = maxs;
}

static void lever_calibrate()
{
    printf("Slowly swing the lever in full range.\n");
    printf("Now calibrating ...");
    fflush(stdout);

    calibrate_range(5);
    printf(" done.\n");
}

static void lever_invert(const char *param)
{
    const char *usage = "Usage: lever invert <on|off>\n";

    int invert = cli_match_prefix((const char *[]){"off", "on"}, 2, param);
    if (invert < 0) {
        printf(usage);
        return;
    }

    printf("param:%s, invert:%d\n", param, invert);

    geki_cfg->lever.invert = invert;
}

static void handle_lever(int argc, char *argv[])
{
    const char *usage = "Usage: lever calibrate\n"
                        "       lever invert <on|off>\n";

    if (argc == 1) {
        if (strncasecmp(argv[0], "calibrate", strlen(argv[0])) != 0) {
            printf(usage);
            return;
        }
        lever_calibrate();
    } else if (argc == 2) {
        if (strncasecmp(argv[0], "invert", strlen(argv[0])) != 0) {
            printf(usage);
            return;
        }
        lever_invert(argv[1]);
    } else {
        printf(usage);
        return;
    }

    config_changed();
    disp_lever();
}

static bool handle_tof_roi(int argc, char *argv[])
{
    if (argc != 1) {
        return false;
    }

    int roi = cli_extract_non_neg_int(argv[0], 0);
    if ((roi < 4) || (roi > 16)) {
        return false;
    }

    geki_cfg->tof.roi = roi;
    airkey_tof_update_roi();

    config_changed();
    disp_tof_roi();
    return true;
}

static bool handle_tof_mix(int side, int argc, char *argv[])
{
    if ((argc < 1) || (argc > 2)) {
        return false;
    }

    const char *algos[] = { "primary", "secondary", "max", "min", "avg" };
    int algo = cli_match_prefix(algos, 5, argv[0]);
    if (algo < 0) {
        return false;
    }

    if (algo == MIX_AVG) {
        int window = 0;
        if (argc == 2) {
            window = cli_extract_non_neg_int(argv[1], 0);
            if ((window < 1) || (window > 7)) {
                return false;
            }
        }
        geki_cfg->tof.mix[side].window = window;
        geki_cfg->tof.mix[side].strict = (window > 0);
    } else {
        geki_cfg->tof.mix[side].window = 0;
        geki_cfg->tof.mix[side].strict = false;
        if ((argc == 2) &&
            (strncasecmp(argv[1], "strict", strlen(argv[1])) == 0)) {
            geki_cfg->tof.mix[side].strict = true;
        }
    }
    geki_cfg->tof.mix[side].algo = algo;
    config_changed();
    disp_tof_mix();
    return true;
}

static inline bool out_of_bound(int value, int low, int high)
{
    return ((value < low) || (value > high));
}

static bool handle_tof_trigger(int argc, char *argv[])
{
    if ((argc < 3) || (argc > 5)) {
        return false;
    }

    const char *names[] = { "left", "right", "shift" };
    int side = cli_match_prefix(names, 3, argv[0]);
    if (side < 0) {
        return false;
    }

    int in_low = cli_extract_non_neg_int(argv[1], 0);
    int in_high = cli_extract_non_neg_int(argv[2], 0);
    if ((in_high < in_low) ||
         out_of_bound(in_low, 1, 999) || out_of_bound(in_high, 1, 999)) {
        return false;
    }

    int out_low = in_low;
    int out_high = in_high;
    if (argc >= 4) {
        out_low = cli_extract_non_neg_int(argv[3], 0);
        if ((out_low > in_low) || out_of_bound(out_low, 1, 999)) {
            return false;
        }
    }
    if (argc == 5) {
        out_high = cli_extract_non_neg_int(argv[4], 0);
        if ((out_high < in_high) || out_of_bound(out_high, 1, 999)) {
            return false;
        }
    }

    geki_cfg->tof.trigger[side].in_low = in_low;
    geki_cfg->tof.trigger[side].in_high = in_high;
    geki_cfg->tof.trigger[side].out_low = out_low;
    geki_cfg->tof.trigger[side].out_high = out_high;
    config_changed();
    disp_tof_trigger();
    return true;
}

static bool handle_tof_diag(int argc, char *argv[])
{
    if (argc == 0) {
        geki_runtime.tof_diag = !geki_runtime.tof_diag;
    } else if (argc == 1) {
        const char *on_off[] = { "on", "off" };
        int choice = cli_match_prefix(on_off, 2, argv[0]);
        if (choice < 0) {
            return false;
        }
        geki_runtime.tof_diag = (choice == 0);
    } else {
        return false;
    }

    printf("TOF diagnose %s.\n", geki_runtime.tof_diag ? "enabled" : "disabled");
    return true;
}

static void handle_tof(int argc, char *argv[])
{
    const char *usage = "Usage: tof roi <4..16>\n"
                        "       tof <left|right> <primary|secondary|max|min> [strict]\n"
                        "       tof <left|right> <avg> [window]\n"
                        "       tof trigger <left|right|shift> <in_low> <in_high> [<out_low> [out_high]]\n"
                        "       tof diagnose [on|off]\n"
                        "   window: 1..7 (5% ~ 35%)\n"
                        "   in_low, in_high, out_low, out_high: 1..999\n"
                        "   in_high>=in_low, out_low<=in_low, out_high>=in_high\n";

    if (argc < 1) {
        printf(usage);
        return;
    }

    const char *commands[] = { "left", "right", "roi", "trigger", "diagnose" };
    int match = cli_match_prefix(commands, count_of(commands), argv[0]);

    if (match == 2) {
        if (handle_tof_roi(argc - 1, argv + 1)) {
            return;
        }
    } else if (match == 3) {
        if (handle_tof_trigger(argc - 1, argv + 1)) {
            return;
        }
    } else if (match == 4) {
        if (handle_tof_diag(argc - 1, argv + 1)) {
            return;
        }
    } else if (match >= 0) {
        if (handle_tof_mix(match, argc - 1, argv + 1)) {
            return;
        }
    }

    printf(usage);
}

static void handle_save()
{
    save_request(true);
}

static void handle_factory_reset()
{
    config_factory_reset();
    printf("Factory reset done.\n");
}

static void handle_nfc()
{
    nfc_init();
    printf("NFC module: %s\n", nfc_module_name());
    nfc_rf_field(true);
    nfc_card_t card = nfc_detect_card();
    nfc_rf_field(false);
    printf("Card: %s", nfc_card_type_str(card.card_type));
    for (int i = 0; i < card.len; i++) {
        printf(" %02x", card.uid[i]);
    }
    printf("\n");
}

static bool handle_aime_mode(const char *mode)
{
    if (strcmp(mode, "0") == 0) {
        geki_cfg->aime.mode = 0;
    } else if (strcmp(mode, "1") == 0) {
        geki_cfg->aime.mode = 1;
    } else {
        return false;
    }
    aime_sub_mode(geki_cfg->aime.mode);
    config_changed();
    return true;
}

static bool handle_aime_virtual(const char *onoff)
{
    if (strcasecmp(onoff, "on") == 0) {
        geki_cfg->aime.virtual_aic = 1;
    } else if (strcasecmp(onoff, "off") == 0) {
        geki_cfg->aime.virtual_aic = 0;
    } else {
        return false;
    }
    aime_virtual_aic(geki_cfg->aime.virtual_aic);
    config_changed();
    return true;
}

static void handle_aime(int argc, char *argv[])
{
    const char *usage = "Usage:\n"
                        "    aime mode <0|1>\n"
                        "    aime virtual <on|off>\n";
    if (argc != 2) {
        printf("%s", usage);
        return;
    }

    const char *commands[] = { "mode", "virtual" };
    int match = cli_match_prefix(commands, 2, argv[0]);
    
    bool ok = false;
    if (match == 0) {
        ok = handle_aime_mode(argv[1]);
    } else if (match == 1) {
        ok = handle_aime_virtual(argv[1]);
    }

    if (ok) {
        disp_aime();
    } else {
        printf("%s", usage);
    }
}

static void handle_volume(int argc, char *argv[])
{
    const char *usage = "Usage: sound <0..255>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    int vol = cli_extract_non_neg_int(argv[0], 0);

    if ((vol >= 0) && (vol <= 255)) {
        geki_cfg->sound.volume = vol;
        config_changed();
        disp_sound();
    } else {
        printf(usage);
    }
}

void commands_init()
{
    cli_register("display", handle_display, "Display all config.");
    cli_register("level", handle_level, "Set LED brightness level.");
    cli_register("hid", handle_hid, "Set HID mode.");
    cli_register("lever", handle_lever, "Lever related settings.");
    cli_register("tof", handle_tof, "Tof tweaks.");
    cli_register("volume", handle_volume, "Sound feedback volume settings.");
    cli_register("save", handle_save, "Save config to flash.");
    cli_register("factory", handle_factory_reset, "Reset everything to default.");
    cli_register("nfc", handle_nfc, "NFC debug.");
    cli_register("aime", handle_aime, "AIME settings.");
}
