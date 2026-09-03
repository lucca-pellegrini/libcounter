#pragma once
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// Initialize the OLED display
void display_init(void);

// Update the display with counter and live distance (mm)
void display_update(uint32_t count, uint32_t distance_mm);

// Clear the display
void display_clear(void);

#endif // DISPLAY_H
