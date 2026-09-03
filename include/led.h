#pragma once
#ifndef LED_H
#define LED_H

#include <stdbool.h>

void rgb_init(void);
void rgb_off(void);
void rgb_set_blue(void);
void rgb_set_red(void);
void rgb_set_orange(void);
void rgb_set_green(void);
bool rgb_blink_red_slow(void);
bool rgb_blink_green_slow(void);

#endif // LED_H
