#pragma once
#ifndef NVS_UTIL_H
#define NVS_UTIL_H

#include <stdint.h>
#include <stdbool.h>

void nvs_util_init(void);
uint32_t nvs_util_load_count(void);
bool nvs_util_save_count(uint32_t count, bool force);
void nvs_util_clear_count(void);

#endif // NVS_UTIL_H
