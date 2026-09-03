#include "button.h"
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

static void (*reset_callback_func)(void) = NULL;
static SemaphoreHandle_t button_sem = NULL;

// ISR only signals the semaphore - no blocking calls allowed in ISR context
static void IRAM_ATTR button_isr_handler(void *arg)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(button_sem, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Task that waits for button press and calls the callback in task context
static void button_task(void *arg)
{
	ESP_LOGI(TAG, "Button task started");
	for (;;) {
		if (xSemaphoreTake(button_sem, portMAX_DELAY) == pdTRUE) {
			ESP_LOGI(TAG, "Button press detected");
			if (reset_callback_func) {
				reset_callback_func();
			}
		}
	}
}

void button_init(void (*reset_callback)(void))
{
	reset_callback_func = reset_callback;

	// Create binary semaphore for ISR-to-task signaling
	button_sem = xSemaphoreCreateBinary();
	assert(button_sem != NULL);

	// Create task to handle button presses in task context
	BaseType_t ret = xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
	assert(ret == pdPASS);

	// The button is wired between 3V3 and the GPIO.
	// With internal pull-down enabled, the pin reads LOW when idle
	// and HIGH when pressed. We trigger on the rising edge (press).
	gpio_config_t io_conf = {
		.intr_type = GPIO_INTR_POSEDGE, // Rising edge: button press (3V3)
		.pin_bit_mask = (1ULL << CONFIG_BUTTON_GPIO),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_ENABLE, // Pull-down: idle LOW, press HIGH
	};

	ESP_ERROR_CHECK(gpio_config(&io_conf));

	// Install ISR service (may already be installed by another component)
	esp_err_t err = gpio_install_isr_service(0);
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		ESP_ERROR_CHECK(err);
	}
	ESP_ERROR_CHECK(gpio_isr_handler_add(CONFIG_BUTTON_GPIO, button_isr_handler, NULL));

	ESP_LOGI(TAG, "Button initialized on GPIO %d with interrupt", CONFIG_BUTTON_GPIO);
}

bool button_is_pressed(void)
{
	// Button pressed = HIGH (connected to 3V3, pulled down when open)
	return gpio_get_level(CONFIG_BUTTON_GPIO) == 1;
}
