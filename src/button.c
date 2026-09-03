#include "button.h"
#include "led.h"
#include "display.h"
#include "libcounter.h"
#include "nvs_util.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"

static const char *TAG = "button";

static SemaphoreHandle_t button_sem = NULL;

/* Gives on the press (rising) edge so other tasks (e.g. the boot animation)
 * can be skipped as soon as the button is touched. */
static SemaphoreHandle_t press_sem = NULL;

extern volatile bool paused;
static bool confirm_active = false;
static volatile bool actions_allowed = false;

static volatile TickType_t press_start_tick = 0;

#define DEBOUNCE_MS 300
#define CONFIRM_WINDOW_MS 5000
#define RED_HOLD_MS 2000

static void IRAM_ATTR button_isr_handler(void *arg)
{
	if (gpio_get_level(CONFIG_BUTTON_GPIO) == 0) {
		/* Button released: wake the task so it can classify the press. */
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(button_sem, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	} else {
		/* Button pressed: remember when it went down and signal a skip. */
		press_start_tick = xTaskGetTickCountFromISR();
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(press_sem, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

static void show_confirm_prompt(void)
{
	ssd1306_handle_t h = display_get_handle();
	if (h) {
		ssd1306_clear_display(h, true);
		ssd1306_display_text(h, 2, "  Pressione", true);
		ssd1306_display_text(h, 3, "  novamente", true);
		ssd1306_display_text(h, 4, "  para zerar!", true);
	}
}

static void do_manual_save(void)
{
	ESP_LOGI(TAG, "Long press - performing manual save");
	paused = true;

	if (nvs_util_save_count(get_person_count(), true))
		display_show_saved();
	else
		ESP_LOGE(TAG, "Manual save failed");

	/* Green blink: 50ms on, 200ms off, 2s total (matching the auto-save). */
	for (int i = 0; i < 8; i++) {
		rgb_blink_green_slow();
		vTaskDelay(pdMS_TO_TICKS(50));
		rgb_blink_green_slow();
		vTaskDelay(pdMS_TO_TICKS(200));
	}
	rgb_off();

	display_invalidate();
	paused = false;
}

static void do_confirm_reset(void)
{
	ESP_LOGI(TAG, "Short press - entering reset confirm window");
	confirm_active = true;
	paused = true;
	show_confirm_prompt();

	/* Blink red slowly while waiting for a separate second press within 5s. */
	TickType_t window_end = xTaskGetTickCount() + pdMS_TO_TICKS(CONFIRM_WINDOW_MS);
	bool confirmed = false;

	while (xTaskGetTickCount() < window_end) {
		rgb_blink_red_slow();
		/* A second press+release gives the semaphore; take it within the window. */
		if (xSemaphoreTake(button_sem, pdMS_TO_TICKS(250)) == pdTRUE) {
			confirmed = true;
			break;
		}
	}

	if (confirmed) {
		ESP_LOGI(TAG, "Second press - reset confirmed");
		reset_person_count();
		nvs_util_clear_count();
		display_show_reset();
		rgb_set_red();
		vTaskDelay(pdMS_TO_TICKS(RED_HOLD_MS));
		rgb_off();
		ESP_LOGI(TAG, "Counter reset, resuming normal operation");
	} else {
		ESP_LOGI(TAG, "Confirm window expired, no reset");
		rgb_off();
	}

	display_invalidate();
	confirm_active = false;
	paused = false;

	/* Drain any button presses queued during the window. */
	xSemaphoreTake(button_sem, 0);
}

static void button_task(void *arg)
{
	ESP_LOGI(TAG, "Button task started");

	TickType_t last_release = 0;

	for (;;) {
		/* Wake on the button release edge. */
		if (xSemaphoreTake(button_sem, portMAX_DELAY) != pdTRUE)
			continue;

		TickType_t now = xTaskGetTickCount();

		/* Debounce: ignore releases too close to the previous one. */
		if (pdTICKS_TO_MS(now - last_release) < DEBOUNCE_MS)
			continue;
		last_release = now;

		/* While booting, a press only counts as a skip; nothing else. */
		if (!actions_allowed) {
			ESP_LOGI(TAG, "Press during boot ignored (skip only)");
			continue;
		}

		/* Classify the press by how long it was held. */
		uint32_t hold_ms = pdTICKS_TO_MS(now - press_start_tick);
		if (hold_ms >= CONFIG_BUTTON_LONG_PRESS_MS)
			do_manual_save();
		else
			do_confirm_reset();
	}
}

void button_init(void)
{
	button_sem = xSemaphoreCreateBinary();
	assert(button_sem != NULL);

	press_sem = xSemaphoreCreateBinary();
	assert(press_sem != NULL);

	BaseType_t ret = xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
	assert(ret == pdPASS);

	gpio_config_t io_conf = {
		.intr_type = GPIO_INTR_ANYEDGE,
		.pin_bit_mask = (1ULL << CONFIG_BUTTON_GPIO),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
	};
	ESP_ERROR_CHECK(gpio_config(&io_conf));

	esp_err_t err = gpio_install_isr_service(0);
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
		ESP_ERROR_CHECK(err);

	ESP_ERROR_CHECK(gpio_isr_handler_add(CONFIG_BUTTON_GPIO, button_isr_handler, NULL));

	ESP_LOGI(TAG, "Button initialized on GPIO %d", CONFIG_BUTTON_GPIO);
}

bool button_is_pressed(void)
{
	return gpio_get_level(CONFIG_BUTTON_GPIO) == 1;
}

SemaphoreHandle_t button_press_sem(void)
{
	return press_sem;
}

void button_allow_actions(void)
{
	actions_allowed = true;
}
