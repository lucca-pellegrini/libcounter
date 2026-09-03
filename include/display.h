#pragma once
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include "ssd1306.h"

// Initialize the OLED display
void display_init(void);

// Get the SSD1306 handle (for direct text rendering)
ssd1306_handle_t display_get_handle(void);

// Update the display with the internal (un-divided) count and live distance (mm).
// The count is divided by two for display; if it is odd it draws a small marker
// in the top-right corner to signal someone came through.
void display_update(uint32_t count, uint32_t distance_mm);

// Force the next display_update to redraw the full screen
void display_invalidate(void);

// Clear the display
void display_clear(void);

// Show a brief "Resetado!" confirmation screen
void display_show_reset(void);

// Show a brief "Salvo!" confirmation screen after a manual save
void display_show_saved(void);

// Boot sequence screens
void display_boot_puc(void);
void display_boot_credits(void);
void display_boot_flash(void);

#endif // DISPLAY_H
