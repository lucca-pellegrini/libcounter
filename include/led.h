#pragma once
#ifndef LED_H
#define LED_H

#include <stdbool.h>

void rgb_init(void);
void rgb_off(void);
void rgb_set_blue(void);
void rgb_set_red(void);
bool rgb_blink_red_slow(void);

#endif // LED_H
