/*
 * Ultrasonic sensor driver using GPIO bit-banging.
 *
 * This implementation uses plain GPIO interrupts to measure the echo pulse
 * width, avoiding the complexity and state management issues of the RMT
 * peripheral. Works with HC-SR04, AJ-SR04M, JSN-SR04T and similar sensors.
 *
 * Protocol:
 *   1. Send trigger pulse (10µs minimum for HC-SR04, 1100µs for AJ-SR04M)
 *   2. Sensor sends 8x 40kHz ultrasonic bursts
 *   3. ECHO pin goes HIGH when burst is sent
 *   4. ECHO pin goes LOW when echo is received (or timeout)
 *   5. Pulse width in µs / 58 = distance in cm (or /5.8 for mm)
 */

#include "sensor.h"
#include "sdkconfig.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "sensor";

/* Pin configuration */
#define TRIG_GPIO CONFIG_ULTRASONIC_TRIG_GPIO
#define ECHO_GPIO CONFIG_ULTRASONIC_ECHO_GPIO

/* Timing constants */
#define TRIGGER_PULSE_US 1100 /* 1.1ms for AJ-SR04M (use 15 for HC-SR04) */
#define ECHO_TIMEOUT_US 30000 /* 30ms max echo time (~5m range) */
#define SPEED_OF_SOUND_MM_US 0.343f /* mm per microsecond at ~20°C */

/* Valid measurement range (sensor physical limits) */
#define DIST_MIN_MM 100
#define DIST_MAX_MM 4500

/* Rolling average buffer */
#define BUFFER_SIZE 3
static uint32_t distance_buffer[BUFFER_SIZE];
static int buffer_index = 0;
static bool buffer_filled = false;

/* ISR state - volatile because shared between ISR and task context */
static volatile int64_t echo_start_time = 0;
static volatile int64_t echo_end_time = 0;
static volatile bool echo_received = false;
static volatile uint32_t isr_call_count = 0;  /* Debug: count ISR invocations */
static volatile uint32_t rising_edge_count = 0;
static volatile uint32_t falling_edge_count = 0;
static SemaphoreHandle_t echo_sem = NULL;

/*
 * GPIO ISR handler for ECHO pin.
 * Captures timestamps on both rising and falling edges.
 */
static void IRAM_ATTR echo_isr_handler(void *arg)
{
	int64_t now = esp_timer_get_time();
	int level = gpio_get_level(ECHO_GPIO);

	isr_call_count++;

	if (level == 1) {
		/* Rising edge: echo pulse started */
		echo_start_time = now;
		echo_received = false;
		rising_edge_count++;
	} else {
		/* Falling edge: echo pulse ended */
		echo_end_time = now;
		echo_received = true;
		falling_edge_count++;

		/* Wake up the waiting task */
		BaseType_t hp_task_woken = pdFALSE;
		xSemaphoreGiveFromISR(echo_sem, &hp_task_woken);
		if (hp_task_woken)
			portYIELD_FROM_ISR();
	}
}

void sensor_init(void)
{
	/* Create semaphore for ISR->task signaling */
	echo_sem = xSemaphoreCreateBinary();
	configASSERT(echo_sem != NULL);

	/* Configure TRIG pin as output, initially low */
	gpio_config_t trig_conf = {
		.pin_bit_mask = (1ULL << TRIG_GPIO),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	ESP_ERROR_CHECK(gpio_config(&trig_conf));
	gpio_set_level(TRIG_GPIO, 0);

	/* Configure ECHO pin as input with interrupt on any edge */
	gpio_config_t echo_conf = {
		.pin_bit_mask = (1ULL << ECHO_GPIO),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_ANYEDGE,
	};
	ESP_ERROR_CHECK(gpio_config(&echo_conf));

	/* Install GPIO ISR service and add handler for ECHO pin */
	ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
	ESP_ERROR_CHECK(gpio_isr_handler_add(ECHO_GPIO, echo_isr_handler, NULL));

	/* Initialize buffer with max distance */
	for (int i = 0; i < BUFFER_SIZE; i++)
		distance_buffer[i] = DIST_MAX_MM;

	ESP_LOGI(TAG, "Ultrasonic sensor ready (TRIG:%d, ECHO:%d)", TRIG_GPIO, ECHO_GPIO);
}

/*
 * Trigger a measurement and wait for the echo.
 * Returns distance in mm, or DIST_MAX_MM on timeout/error.
 */
static uint32_t sensor_measure(void)
{
	/* Clear any pending semaphore give from previous cycle */
	xSemaphoreTake(echo_sem, 0);

	/* Snapshot counters before measurement for debugging */
	uint32_t isr_before = isr_call_count;
	uint32_t rising_before = rising_edge_count;
	uint32_t falling_before = falling_edge_count;

	/* Check ECHO pin state before trigger */
	int echo_level_before = gpio_get_level(ECHO_GPIO);

	/* Reset state */
	echo_start_time = 0;
	echo_end_time = 0;
	echo_received = false;

	/* Send trigger pulse */
	gpio_set_level(TRIG_GPIO, 1);
	esp_rom_delay_us(TRIGGER_PULSE_US);
	gpio_set_level(TRIG_GPIO, 0);

	/* Check ECHO pin state right after trigger */
	int echo_level_after = gpio_get_level(ECHO_GPIO);

	/* Wait for echo with timeout */
	if (xSemaphoreTake(echo_sem, pdMS_TO_TICKS(ECHO_TIMEOUT_US / 1000 + 10)) != pdTRUE) {
		int echo_level_now = gpio_get_level(ECHO_GPIO);
		ESP_LOGW(TAG, "Echo timeout! ECHO pin: before=%d after_trig=%d now=%d | ISR calls: %lu (+%lu) rising: %lu (+%lu) falling: %lu (+%lu)",
			 echo_level_before, echo_level_after, echo_level_now,
			 (unsigned long)isr_call_count, (unsigned long)(isr_call_count - isr_before),
			 (unsigned long)rising_edge_count, (unsigned long)(rising_edge_count - rising_before),
			 (unsigned long)falling_edge_count, (unsigned long)(falling_edge_count - falling_before));
		return DIST_MAX_MM;
	}

	/* Check if we got a valid echo */
	if (!echo_received || echo_start_time == 0 || echo_end_time <= echo_start_time) {
		ESP_LOGW(TAG, "Invalid echo: received=%d start=%lld end=%lld | ISR +%lu rising +%lu falling +%lu",
			 echo_received, echo_start_time, echo_end_time,
			 (unsigned long)(isr_call_count - isr_before),
			 (unsigned long)(rising_edge_count - rising_before),
			 (unsigned long)(falling_edge_count - falling_before));
		return DIST_MAX_MM;
	}

	/* Calculate pulse width and distance */
	int64_t pulse_us = echo_end_time - echo_start_time;

	/* Distance = (pulse_us * speed_of_sound) / 2
	 * We divide by 2 because the sound travels to the object and back.
	 * At ~20°C, speed of sound ≈ 343 m/s = 0.343 mm/µs
	 * So: distance_mm = pulse_us * 0.343 / 2 = pulse_us * 0.1715
	 */
	uint32_t distance_mm = (uint32_t)(pulse_us * SPEED_OF_SOUND_MM_US / 2.0f);

	/* Validate range */
	if (distance_mm < DIST_MIN_MM || distance_mm > DIST_MAX_MM) {
		ESP_LOGD(TAG, "Out of range: %lu mm (pulse=%lld us)", (unsigned long)distance_mm, pulse_us);
		return DIST_MAX_MM;
	}

	return distance_mm;
}

void sensor_update(void)
{
	uint32_t distance = sensor_measure();

	distance_buffer[buffer_index] = distance;
	buffer_index = (buffer_index + 1) % BUFFER_SIZE;

	if (buffer_index == 0)
		buffer_filled = true;

	if (distance < DIST_MAX_MM)
		ESP_LOGI(TAG, "dist=%lumm avg=%lumm thr=%dcm", (unsigned long)distance,
			 (unsigned long)sensor_get_averaged_distance(), CONFIG_ULTRASONIC_THRESHOLD_CM);
}

uint32_t sensor_get_distance(void)
{
	int latest_index = (buffer_index - 1 + BUFFER_SIZE) % BUFFER_SIZE;
	return distance_buffer[latest_index];
}

uint32_t sensor_get_averaged_distance(void)
{
	uint32_t sum = 0;
	int count = buffer_filled ? BUFFER_SIZE : (buffer_index > 0 ? buffer_index : 1);

	for (int i = 0; i < count; i++)
		sum += distance_buffer[i];

	return sum / count;
}

bool sensor_person_detected(void)
{
	/* Threshold is in cm (from Kconfig), sensor returns mm */
	return sensor_get_averaged_distance() < ((uint32_t)CONFIG_ULTRASONIC_THRESHOLD_CM * 10);
}
