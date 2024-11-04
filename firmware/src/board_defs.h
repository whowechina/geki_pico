/*
 * Geki Controller Board Definitions
 * WHowe <github.com/whowechina>
 */

#if defined BOARD_GEKI_PICO

#define RGB_PIN 16

#define RGB_ORDER GRB // or RGB

#define BUTTON_DEF { 12, 11, 10, 5, 4, 3, 13, 2 }
#define SOUND_DEF { 8, 6 }

#define TOF_LEFT_PORT i2c1
#define TOF_RIGHT_PORT i2c0
#define TOF_LEFT_SECOND_GPIO 9
#define TOF_RIGHT_SECOND_GPIO 7

#define TOF_GPIO_DEF { 18, 19, 0, 1 }
#define TOF_I2C_FREQ 400*1000

#define AXIS_MUX_PIN_A 21
#define AXIS_MUX_PIN_B 20
#define ADC_CHANNEL 0

#define PN532_I2C_PORT i2c1

#define NKRO_KEYMAP "awsdjikl123"
#else

#endif
