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
#include "nvs_util.h"

static const char *TAG = "main";

static uint32_t person_count = 0;
static SemaphoreHandle_t count_mutex = NULL;
static SemaphoreHandle_t display_mutex = NULL;
static bool person_was_detected = false;
static uint32_t clear_zone_count = 0;
static uint32_t detect_zone_count = 0;

volatile bool paused = false;
volatile bool debug_mode = false;

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

		ESP_LOGD(TAG, "detected=%d was=%d avg=%lumm thr=%dmm", person_detected, person_was_detected,
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

				vTaskDelay(pdMS_TO_TICKS(200));
				rgb_set_orange();
			}
		} else if (!person_detected && person_was_detected) {
			clear_zone_count++;
			if (clear_zone_count >= CONFIG_ULTRASONIC_COOLDOWN_READS) {
				person_was_detected = false;
				detect_zone_count = 0;
				clear_zone_count = 0;
				rgb_off();
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

static void nvs_save_task(void *pvParameters)
{
	const TickType_t xDelay = pdMS_TO_TICKS(CONFIG_NVS_SAVE_INTERVAL_SECONDS * 1000);

	for (;;) {
		/* Wait fixed interval regardless of paused state */
		vTaskDelay(xDelay);

		if (paused)
			continue;

		if (nvs_util_save_count(get_person_count(), false)) {
			/* Indicate the save with a green blink: 50ms on, 200ms off, 2s */
			for (int i = 0; i < 8; i++) {
				rgb_blink_green_slow();
				vTaskDelay(pdMS_TO_TICKS(50));
				rgb_blink_green_slow();
				vTaskDelay(pdMS_TO_TICKS(200));
			}
			rgb_off();
		}
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

	nvs_util_init();
	person_count = nvs_util_load_count();

	sensor_init();
	display_init();
	rgb_init();
	button_init();

	/* Debug mode: hold the button while powering on to skip the boot sequence,
	 * ignore any saved count, and start from the magic debug value. */
	if (button_is_pressed()) {
		debug_mode = true;
		person_count = CONFIG_DEBUG_COUNT;
		ESP_LOGW(TAG, "Debug mode: skipping boot sequence, counter set to %lu",
			 (unsigned long)CONFIG_DEBUG_COUNT);
	}

	/* Boot sequence (skipped in debug mode): rainbow LED while cycling screens. */
	if (!debug_mode) {
		uint16_t hue = 0;
		TickType_t phase_start;

		display_boot_puc();
		phase_start = xTaskGetTickCount();
		while ((xTaskGetTickCount() - phase_start) < pdMS_TO_TICKS(5000)) {
			rgb_rainbow(hue);
			hue = (hue + 5) % 361;
			vTaskDelay(pdMS_TO_TICKS(20));
		}

		display_boot_credits();
		phase_start = xTaskGetTickCount();
		while ((xTaskGetTickCount() - phase_start) < pdMS_TO_TICKS(5000)) {
			rgb_rainbow(hue);
			hue = (hue + 5) % 361;
			vTaskDelay(pdMS_TO_TICKS(20));
		}

		display_boot_flash();
		vTaskDelay(pdMS_TO_TICKS(200));
		display_clear();
		rgb_off();
	}

	display_update(person_count, 9999);

	xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 10, NULL);
	xTaskCreate(display_task, "display_task", 4095, NULL, 5, NULL);
	xTaskCreate(nvs_save_task, "nvs_save_task", 4096, NULL, 3, NULL);

	ESP_LOGI(TAG, "Library Counter initialized successfully");
}
