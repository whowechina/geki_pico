/*
 * Sound Feedback
 * WHowe <github.com/whowechina>
 */

#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/flash.h"

void sound_init();
void sound_toggle(bool on);
void sound_set(int id, bool on);

#endif
