#pragma once
#ifndef LIBCOUNTER_H
#define LIBCOUNTER_H

#include <stdint.h>

// Get the current person count
uint32_t get_person_count(void);

// Reset the person count
void reset_person_count(void);

// Increment the person count
void increment_person_count(void);

#endif // LIBCOUNTER_H
