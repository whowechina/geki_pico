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

#include "gimbal.h"

extern uint8_t RING_DATA[];

#include "nfc.h"
#include "aime.h"

#include "usb_descriptors.h"

#define SENSE_LIMIT_MAX 9
#define SENSE_LIMIT_MIN -9

static inline int sprintf_hsv_rgb(char *buf, const rgb_hsv_t *color)
{
    return sprintf(buf, "%s(%d,%d,%d)", color->rgb_hsv ? "hsv" : "rgb",
              color->val[0], color->val[1], color->val[2]);
}

static const char *color_str(const rgb_hsv_t *color, bool left_right)
{
    static char buf[64];
    int count = 0;

    if (left_right) {
        count += sprintf(buf + count, "LEFT ");
    }

    count += sprintf_hsv_rgb(buf + count, color);
    
    if (left_right) {
        count += sprintf(buf + count, ", RIGHT ");
        count += sprintf_hsv_rgb(buf + count, color + 1);
    }

    return buf;
}

static void disp_light()
{
    printf("[Light]\n");
    printf("  Level: %d.\n", geki_cfg->light.level);
    printf("  Colors:\n");
    printf("    base0: %s\n", color_str(geki_cfg->light.base[0], true));
    printf("    base1: %s\n", color_str(geki_cfg->light.base[0], true));
    printf("    button: %s\n", color_str(geki_cfg->light.button, true));
    printf("    boost: %s\n", color_str(geki_cfg->light.boost, true));
    printf("    steer: %s\n", color_str(geki_cfg->light.steer, true));
    printf("    aux_on: %s\n", color_str(&geki_cfg->light.aux_on, false));
    printf("    aux_off: %s\n", color_str(&geki_cfg->light.aux_off, false));
}

static void disp_gimbal()
{
    printf("[Gimbal]\n");
    printf("  %s, %s, raw %d-%d.\n",
            geki_cfg->gimbal.invert ? "invert" : "normal",
            geki_cfg->gimbal.analog ? "analog" : "digital",
            geki_cfg->gimbal.min, geki_cfg->gimbal.max);
}

static void disp_sound()
{
    printf("[Sound]\n");
    printf("  Volume: %d\n", geki_cfg->sound.volume);
}

static void disp_hid()
{
    printf("[HID]\n");
    printf("  Joy: %s, NKRO: %s.\n", 
           geki_cfg->hid.joy ? "on" : "off",
           geki_cfg->hid.nkro ? "on" : "off" );
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
    const char *usage = "Usage: display [light|sound|hid|gimbal|aime]\n";
    if (argc > 1) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        disp_light();
        disp_gimbal();
        disp_sound();
        disp_hid();
        disp_aime();
        return;
    }

    const char *choices[] = {"light", "gimbal", "sound", "hid", "aime"};
    switch (cli_match_prefix(choices, count_of(choices), argv[0])) {
        case 0:
            disp_light();
            break;
        case 1:
            disp_gimbal();
            break;
        case 2:
            disp_sound();
            break;
        case 3:
            disp_hid();
            break;
        case 4:
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
    const char *usage = "Usage: hid <joy|nkro|both>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    const char *choices[] = {"joy", "nkro", "both"};
    int match = cli_match_prefix(choices, 3, argv[0]);
    if (match < 0) {
        printf(usage);
        return;
    }

    geki_cfg->hid.joy = ((match == 0) || (match == 2)) ? 1 : 0;
    geki_cfg->hid.nkro = ((match == 1) || (match == 2)) ? 1 : 0;
    config_changed();
    disp_hid();
}

static void calibrate_range(uint32_t seconds)
{
    uint32_t mins = 2048;
    uint32_t maxs = 2048;

    uint64_t start = time_us_64();

    while (time_us_64() - start < seconds * 1000000) {
        uint16_t val = gimbal_raw();
        printf("%4d\n", val);
        if (val < mins) {
            mins -= (mins - val) / 2;
        } else if (val > maxs) {
            maxs += (val - maxs) / 2;
        }
        sleep_ms(7);
    }

    geki_cfg->gimbal.min = mins;
    geki_cfg->gimbal.max = maxs;
}

static void gimbal_calibrate()
{
    printf("Slowly move the stick in full range.\n");
    printf("Now calibrating ...");
    fflush(stdout);

    calibrate_range(5);
    printf(" done.\n");
}

static void gimbal_invert(const char *param)
{
    const char *usage = "Usage: gimbal invert <on|off>\n";

    int invert = cli_match_prefix((const char *[]){"off", "on"}, 2, param);
    if (invert < 0) {
        printf(usage);
        return;
    }

    printf("param:%s, invert:%d\n", param, invert);

    geki_cfg->gimbal.invert = invert;
}

static void gimbal_analog(const char *param)
{
    const char *usage = "Usage: gimbal analog <on|off>\n";
    int analog = cli_match_prefix((const char *[]){"off", "on"}, 2, param);
    if (analog < 0) {
        printf(usage);
        return;
    }

    geki_cfg->gimbal.analog = analog;
}

static void handle_gimbal(int argc, char *argv[])
{
    const char *usage = "Usage: gimbal calibrate\n"
                        "       gimbal invert <on|off>\n"
                        "       gimbal analog <on|off>\n";
    if (argc == 1) {
        if (strncasecmp(argv[0], "calibrate", strlen(argv[0])) != 0) {
            printf(usage);
            return;
        }
        gimbal_calibrate();
    } else if (argc == 2) {
        int op = cli_match_prefix((const char *[]){"invert", "analog"}, 2, argv[0]);
        if (op == 0) {
            gimbal_invert(argv[1]);
        } else if (op == 1) {
            gimbal_analog(argv[1]);
        } else {
            printf(usage);
            return;
        }
    } else {
        printf(usage);
        return;
    }

    config_changed();
    disp_gimbal();
}

static bool extract_color(rgb_hsv_t *color, char *argv[4])
{
    int rgb_hsv = cli_match_prefix((const char *[]){"rgb", "hsv"}, 2, argv[0]);
    if (rgb_hsv < 0) {
        return false;
    }
    color->rgb_hsv = rgb_hsv;

    for (int i = 0; i < 3; i++) {
        int v = cli_extract_non_neg_int(argv[1 + i], 0);
        if ((v < 0) || (v > 255)) {
            return false;
        }
        color->val[i] = v;
    }

    return true;
}

static void handle_color(int argc, char *argv[])
{
    const char *usage = "Usage: color <name> [left|right] <rgb|hsv> <0..255> <0..255> <0..255>\n"
                        "  name: base0 base1 button boost steer aux_on aux_off\n";
    if ((argc != 5) && (argc != 6)) {
        printf(usage);
        return;
    }

    rgb_hsv_t *names[] = {
        &geki_cfg->light.aux_on,
        &geki_cfg->light.aux_off,
        geki_cfg->light.base[0],
        geki_cfg->light.base[1],
        geki_cfg->light.button,
        geki_cfg->light.boost,
        geki_cfg->light.steer,
    };
    const char *choices[] = {"aux_on", "aux_off", "base0", "base1", "button", "boost", "steer"};
    static_assert(count_of(choices) == count_of(names));

    int name = cli_match_prefix(choices, count_of(choices), argv[0]);
    if (name < 0) {
        printf(usage);
        return;
    }

    bool left = true;
    bool right = true;
    if (argc == 6) {
        int left_right = cli_match_prefix((const char *[]){"left", "right"}, 2, argv[1]);
        if (left_right < 0) {
            printf(usage);
            return;
        }
        left = (left_right == 0);
        right = (left_right == 1);
    }

    rgb_hsv_t color;
    if (!extract_color(&color, argv + argc - 4)) {
        printf(usage);
        return;
    }

    rgb_hsv_t *target = names[name];
    if (left) {
        target[0] = color;
    }
    if ((name >= 2) && right) {
        target[1] = color;
    }

    config_changed();
    disp_light();
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
    cli_register("color", handle_color, "Set LED color.");
    cli_register("hid", handle_hid, "Set HID mode.");
    cli_register("gimbal", handle_gimbal, "Calibrate the gimbals.");
    cli_register("volume", handle_volume, "Sound feedback volume settings.");
    cli_register("save", handle_save, "Save config to flash.");
    cli_register("factory", handle_factory_reset, "Reset everything to default.");
    cli_register("nfc", handle_nfc, "NFC debug.");
    cli_register("aime", handle_aime, "AIME settings.");
}
