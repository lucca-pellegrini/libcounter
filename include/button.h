#pragma once
#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>

// Initialize the button with interrupt handler.
// A short press opens the reset confirmation screen (requires a second press);
// a long press performs a manual NVS save.
void button_init(void);

// Check if button was pressed (for debugging)
bool button_is_pressed(void);

#endif // BUTTON_H
