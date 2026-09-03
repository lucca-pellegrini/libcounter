#pragma once
#ifndef LIBCOUNTER_H
#define LIBCOUNTER_H

#include <stdint.h>

// Application-wide state shared across tasks.
//
//   RUNNING - normal operation: sensor counts, display refreshes, and the
//             count-increment (blue/orange) LED feedback runs.
//   PAUSED  - reset-confirm flow: everything frozen while the reset screens/LED
//             own the UI; the sensor stops counting.
//   SAVING  - save animation in progress: the sensor keeps counting (so no
//             passers-by are missed), the green LED blinks and the "Salvo!"
//             screen plays undisturbed. The count-increment LED feedback and
//             display refresh are suppressed until the animation ends.
typedef enum {
	APP_STATE_RUNNING = 0,
	APP_STATE_PAUSED,
	APP_STATE_SAVING,
} app_state_t;

// Get the current person count
uint32_t get_person_count(void);

// Reset the person count
void reset_person_count(void);

// Increment the person count
void increment_person_count(void);

// Play the save feedback (green blink + "Salvo!" screen) after a save.
// Suppresses normal count feedback and freezes the display while active.
void do_save_feedback(void);

#endif // LIBCOUNTER_H
