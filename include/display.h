#pragma once
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include "ssd1306.h"

// Initialize the OLED display
void display_init(void);

// Get the SSD1306 handle (for direct text rendering)
ssd1306_handle_t display_get_handle(void);

// Update the display with counter and live distance (mm).
// 'odd' indicates that the internal (un-divided) count is odd, which draws a
// small marker in the top-right corner to signal someone came through.
void display_update(uint32_t count, uint32_t distance_mm, bool odd);

// Force the next display_update to redraw the full screen
void display_invalidate(void);

// Clear the display
void display_clear(void);

// Show a brief "Resetado!" confirmation screen
void display_show_reset(void);

// Boot sequence screens
void display_boot_puc(void);
void display_boot_credits(void);
void display_boot_flash(void);

#endif // DISPLAY_H
