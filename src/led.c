#include "led.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

static const char *TAG = "led";

static led_strip_handle_t strip = NULL;
static bool blink_state = false;

void rgb_init(void)
{
	led_strip_config_t strip_conf = {
		.strip_gpio_num = CONFIG_RGB_LED_GPIO,
		.max_leds = 1,
		.led_model = LED_MODEL_WS2812,
		.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
		.flags.invert_out = false,
	};

	led_strip_rmt_config_t rmt_conf = {
		.clk_src = RMT_CLK_SRC_DEFAULT,
		.resolution_hz = 10 * 1000 * 1000,
		.mem_block_symbols = 0,
		.flags.with_dma = false,
	};

	ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_conf, &rmt_conf, &strip));
	ESP_ERROR_CHECK(led_strip_clear(strip));

	ESP_LOGI(TAG, "RGB LED ready on GPIO %d", CONFIG_RGB_LED_GPIO);
}

void rgb_off(void)
{
	if (strip)
		led_strip_clear(strip);
}

void rgb_set_blue(void)
{
	if (strip) {
		led_strip_set_pixel(strip, 0, 0, 0, 80);
		led_strip_refresh(strip);
	}
}

void rgb_set_red(void)
{
	if (strip) {
		led_strip_set_pixel(strip, 0, 80, 0, 0);
		led_strip_refresh(strip);
	}
}

bool rgb_blink_red_slow(void)
{
	blink_state = !blink_state;
	if (blink_state)
		rgb_set_red();
	else
		rgb_off();
	return blink_state;
}
