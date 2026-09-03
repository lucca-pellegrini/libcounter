#include "led.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

static const char *TAG = "led";

static led_strip_handle_t strip = NULL;
static bool red_blink_state = false;
static bool green_blink_state = false;

#define BRIGHTNESS 128

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

static void rgb_set(uint32_t red, uint32_t green, uint32_t blue)
{
	if (strip) {
		led_strip_set_pixel(strip, 0, red, green, blue);
		led_strip_refresh(strip);
	}
}

void rgb_set_blue(void)
{
	rgb_set(0, 0, BRIGHTNESS);
}

void rgb_set_red(void)
{
	rgb_set(BRIGHTNESS, 0, 0);
}

void rgb_set_orange(void)
{
	rgb_set(BRIGHTNESS, BRIGHTNESS / 2, 0);
}

void rgb_set_green(void)
{
	rgb_set(0, BRIGHTNESS, 0);
}

void rgb_rainbow(uint16_t hue)
{
	if (strip) {
		led_strip_set_pixel_hsv(strip, 0, hue, 255, BRIGHTNESS);
		led_strip_refresh(strip);
	}
}

bool rgb_blink_red_slow(void)
{
	red_blink_state = !red_blink_state;
	if (red_blink_state)
		rgb_set_red();
	else
		rgb_off();
	return red_blink_state;
}

bool rgb_blink_green_slow(void)
{
	green_blink_state = !green_blink_state;
	if (green_blink_state)
		rgb_set_green();
	else
		rgb_off();
	return green_blink_state;
}
