/*
 * Geki Controller Board Definitions
 * WHowe <github.com/whowechina>
 */

#if defined BOARD_GEKI_PICO

#define RGB_PIN 16

#define RGB_ORDER GRB // or RGB

#define BUTTON_DEF { 12, 11, 10, 5, 4, 3, 13, 2 }
#define SOUND_DEF { 8, 6 }
#define TOF_PORT_DEF { i2c1, i2c0 }
#define TOF_GPIO_DEF { 18, 19, 0, 1 }

#define AXIS_MUX_PIN_A 21
#define AXIS_MUX_PIN_B 20
#define ADC_CHANNEL 0

#define NKRO_KEYMAP "awsdjikl123"
#else

#endif
