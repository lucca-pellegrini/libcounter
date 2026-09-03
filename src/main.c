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

static const char *TAG = "main";

static uint32_t person_count = 0;
static SemaphoreHandle_t count_mutex = NULL;
static bool person_was_detected = false;
static uint32_t clear_zone_count = 0;
static uint32_t detect_zone_count = 0;

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
		// Update sensor reading
		sensor_update();

		// Check for person detection with hysteresis
		bool person_detected = sensor_person_detected();
		uint32_t avg_mm = sensor_get_averaged_distance();

		ESP_LOGI(TAG, "detected=%d was=%d avg=%lumm thr=%dmm", person_detected, person_was_detected,
			 (unsigned long)avg_mm, CONFIG_ULTRASONIC_THRESHOLD_CM * 10);

		if (person_detected && !person_was_detected) {
			// Person possibly entered the detection zone. Require several
			// consecutive below-threshold readings before confirming, so that
			// noise near the boundary doesn't cause false counts.
			detect_zone_count++;
			if (detect_zone_count >= CONFIG_ULTRASONIC_CONFIRM_READS) {
				// Person confirmed in the detection zone
				increment_person_count();
				person_was_detected = true;
				clear_zone_count = 0;

				// Update display immediately
				display_update(get_person_count(), avg_mm);
			}
		} else if (!person_detected && person_was_detected) {
			// Count consecutive above-threshold readings before allowing recount
			clear_zone_count++;
			if (clear_zone_count >= CONFIG_ULTRASONIC_COOLDOWN_READS) {
				person_was_detected = false;
				detect_zone_count = 0;
				clear_zone_count = 0;
				ESP_LOGI(TAG, "Person left detection zone (after %lu clear reads)",
					 (unsigned long)CONFIG_ULTRASONIC_COOLDOWN_READS);
			}
		} else if (person_detected && person_was_detected) {
			// Still detected — reset the clear zone counter
			clear_zone_count = 0;
		} else {
			// Not detected and not was-detected — reset detection confirm counter
			detect_zone_count = 0;
		}

		vTaskDelay(xDelay);
	}
}

static void display_task(void *pvParameters)
{
	const TickType_t xDelay = pdMS_TO_TICKS(500); // Update display every 500ms

	for (;;) {
		uint32_t avg_mm = sensor_get_averaged_distance();
		display_update(get_person_count(), avg_mm);
		vTaskDelay(xDelay);
	}
}

void app_main(void)
{
	ESP_LOGI(TAG, "Library Counter starting...");
	ESP_LOGI(TAG, "Detection threshold: %d cm", CONFIG_ULTRASONIC_THRESHOLD_CM);
	ESP_LOGI(TAG, "Measurement interval: %d ms", CONFIG_ULTRASONIC_MEASURE_INTERVAL_MS);

	// Create mutex for thread-safe counter access
	count_mutex = xSemaphoreCreateMutex();
	if (!count_mutex) {
		ESP_LOGE(TAG, "Failed to create mutex");
		return;
	}

	// Initialize hardware
	sensor_init();
	display_init();
	button_init(reset_person_count);

	// Display initial count
	display_update(0, 4500);

	// Create tasks
	xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 10, NULL);
	xTaskCreate(display_task, "display_task", 4095, NULL, 5, NULL);

	ESP_LOGI(TAG, "Library Counter initialized successfully");
}
