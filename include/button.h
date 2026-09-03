#pragma once
#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Initialize the button with interrupt handler.
// A short press opens the reset confirmation screen (requires a second press);
// a long press performs a manual NVS save.
void button_init(void);

// Check if button was pressed (for debugging)
bool button_is_pressed(void);

// Binary semaphore given on every press (rising edge). Used to abort the boot
// animation as soon as the button is touched.
SemaphoreHandle_t button_press_sem(void);

// Allow the button to perform real actions (save/reset). Until this is called,
// presses only act as a boot-animation skip and are otherwise ignored.
void button_allow_actions(void);

#endif // BUTTON_H
