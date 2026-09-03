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

extern volatile bool paused;
static bool confirm_active = false;

#define DEBOUNCE_MS 300
#define CONFIRM_WINDOW_MS 5000
#define RED_HOLD_MS 2000

static void IRAM_ATTR button_isr_handler(void *arg)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(button_sem, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void show_confirm_prompt(void)
{
	ssd1306_handle_t h = display_get_handle();
	if (h) {
		ssd1306_clear_display(h, false);
		ssd1306_display_text(h, 2, "Pressione", false);
		ssd1306_display_text(h, 3, "novamente", false);
		ssd1306_display_text(h, 4, "para zerar!", false);
	}
}

static void button_task(void *arg)
{
	ESP_LOGI(TAG, "Button task started");

	TickType_t last_valid_press = 0;

	for (;;) {
		/* Wait for the first press (ISR signals the semaphore) */
		if (xSemaphoreTake(button_sem, portMAX_DELAY) != pdTRUE)
			continue;

		TickType_t now = xTaskGetTickCount();
		if (pdTICKS_TO_MS(now - last_valid_press) < DEBOUNCE_MS)
			continue;
		last_valid_press = now;

		ESP_LOGI(TAG, "First button press - entering reset confirm window");
		confirm_active = true;
		paused = true;
		show_confirm_prompt();

		/* Blink red slowly while waiting for a second press within 5s */
		TickType_t window_end = now + pdMS_TO_TICKS(CONFIRM_WINDOW_MS);
		bool confirmed = false;

		while (xTaskGetTickCount() < window_end) {
			rgb_blink_red_slow();
			vTaskDelay(pdMS_TO_TICKS(250));

			/* Poll for a second press during the window */
			if (gpio_get_level(CONFIG_BUTTON_GPIO) == 1) {
				TickType_t press_now = xTaskGetTickCount();
				if (pdTICKS_TO_MS(press_now - last_valid_press) >= DEBOUNCE_MS) {
					last_valid_press = press_now;
					confirmed = true;
					break;
				}
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

		/* Drain any button presses queued during the window */
		xSemaphoreTake(button_sem, 0);
	}
}

void button_init(void (*reset_callback)(void))
{
	button_sem = xSemaphoreCreateBinary();
	assert(button_sem != NULL);

	BaseType_t ret = xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
	assert(ret == pdPASS);

	gpio_config_t io_conf = {
		.intr_type = GPIO_INTR_POSEDGE,
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
