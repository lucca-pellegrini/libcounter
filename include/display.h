#pragma once
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include "ssd1306.h"

// Initialize the OLED display
void display_init(void);

// Get the SSD1306 handle (for direct text rendering)
ssd1306_handle_t display_get_handle(void);

// Update the display with counter and live distance (mm)
void display_update(uint32_t count, uint32_t distance_mm);

// Force the next display_update to redraw the full screen
void display_invalidate(void);

// Clear the display
void display_clear(void);

// Show a brief "Resetado!" confirmation screen
void display_show_reset(void);

#endif // DISPLAY_H
