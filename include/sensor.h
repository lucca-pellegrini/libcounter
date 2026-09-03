#pragma once
#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <stdbool.h>

// Initialize the ultrasonic sensor
void sensor_init(void);

// Get the current distance reading (in cm)
uint32_t sensor_get_distance(void);

// Get the averaged distance over the last 200ms
uint32_t sensor_get_averaged_distance(void);

// Check if person is detected (distance below threshold)
bool sensor_person_detected(void);

// Update the sensor readings (call from main task)
void sensor_update(void);

#endif // SENSOR_H
