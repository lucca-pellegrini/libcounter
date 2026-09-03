#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"

#include "libcounter.h"
#include "sensor.h"
#include "display.h"
#include "button.h"
#include "led.h"

static const char *TAG = "main";

static uint32_t person_count = 0;
static SemaphoreHandle_t count_mutex = NULL;
static SemaphoreHandle_t display_mutex = NULL;
static bool person_was_detected = false;
static uint32_t clear_zone_count = 0;
static uint32_t detect_zone_count = 0;

volatile bool paused = false;

uint32_t get_person_count(void)
{
	uint32_t count;
	xSemaphoreTake(count_mutex, portMAX_DELAY);
	count = person_count;
	xSemaphoreGive(count_mutex);
	return count;
}

void reset_person_count(void)
{
	xSemaphoreTake(count_mutex, portMAX_DELAY);
	person_count = 0;
	xSemaphoreGive(count_mutex);
	ESP_LOGI(TAG, "Counter reset to 0");
}

void increment_person_count(void)
{
	xSemaphoreTake(count_mutex, portMAX_DELAY);
	person_count++;
	xSemaphoreGive(count_mutex);
	ESP_LOGI(TAG, "Counter incremented to %lu", person_count);
}

static void sensor_task(void *pvParameters)
{
	const TickType_t xDelay = pdMS_TO_TICKS(CONFIG_ULTRASONIC_MEASURE_INTERVAL_MS);

	for (;;) {
		if (paused) {
			vTaskDelay(xDelay);
			continue;
		}

		sensor_update();

		bool person_detected = sensor_person_detected();
		uint32_t avg_mm = sensor_get_averaged_distance();

		ESP_LOGI(TAG, "detected=%d was=%d avg=%lumm thr=%dmm", person_detected, person_was_detected,
			 (unsigned long)avg_mm, CONFIG_ULTRASONIC_THRESHOLD_CM * 10);

		if (person_detected && !person_was_detected) {
			detect_zone_count++;
			if (detect_zone_count >= CONFIG_ULTRASONIC_CONFIRM_READS) {
				increment_person_count();
				person_was_detected = true;
				clear_zone_count = 0;

				rgb_set_blue();

				xSemaphoreTake(display_mutex, portMAX_DELAY);
				display_update(get_person_count(), avg_mm);
				xSemaphoreGive(display_mutex);

				vTaskDelay(pdMS_TO_TICKS(1000));
				rgb_off();
			}
		} else if (!person_detected && person_was_detected) {
			clear_zone_count++;
			if (clear_zone_count >= CONFIG_ULTRASONIC_COOLDOWN_READS) {
				person_was_detected = false;
				detect_zone_count = 0;
				clear_zone_count = 0;
				ESP_LOGI(TAG, "Person left detection zone (after %lu clear reads)",
					 (unsigned long)CONFIG_ULTRASONIC_COOLDOWN_READS);
			}
		} else if (person_detected && person_was_detected) {
			clear_zone_count = 0;
		} else {
			detect_zone_count = 0;
		}

		vTaskDelay(xDelay);
	}
}

static void display_task(void *pvParameters)
{
	const TickType_t xDelay = pdMS_TO_TICKS(500);

	for (;;) {
		if (paused) {
			vTaskDelay(xDelay);
			continue;
		}

		xSemaphoreTake(display_mutex, portMAX_DELAY);
		display_update(get_person_count(), sensor_get_averaged_distance());
		xSemaphoreGive(display_mutex);

		vTaskDelay(xDelay);
	}
}

void app_main(void)
{
	ESP_LOGI(TAG, "Library Counter starting...");
	ESP_LOGI(TAG, "Detection threshold: %d cm", CONFIG_ULTRASONIC_THRESHOLD_CM);
	ESP_LOGI(TAG, "Measurement interval: %d ms", CONFIG_ULTRASONIC_MEASURE_INTERVAL_MS);

	count_mutex = xSemaphoreCreateMutex();
	if (!count_mutex) {
		ESP_LOGE(TAG, "Failed to create count mutex");
		return;
	}

	display_mutex = xSemaphoreCreateMutex();
	if (!display_mutex) {
		ESP_LOGE(TAG, "Failed to create display mutex");
		return;
	}

	sensor_init();
	display_init();
	rgb_init();
	button_init(reset_person_count);

	display_update(0, 4500);

	xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 10, NULL);
	xTaskCreate(display_task, "display_task", 4095, NULL, 5, NULL);

	ESP_LOGI(TAG, "Library Counter initialized successfully");
}
