#pragma once
#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>

// Initialize the button with interrupt handler
void button_init(void (*reset_callback)(void));

// Check if button was pressed (for debugging)
bool button_is_pressed(void);

#endif // BUTTON_H
