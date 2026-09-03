#include "display.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "ssd1306.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

static const char *TAG = "display";

extern volatile bool debug_mode;

static ssd1306_handle_t display_handle = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;

static bool initialized = false;
static uint32_t last_count;
static uint32_t last_distance_mm;
static TickType_t last_refresh;

/* Small filled square drawn at the top-right corner when odd. */
#define ODD_MARKER_SEG 120
#define ODD_MARKER_WIDTH 6
#define ODD_MARKER_PAGE 0
static uint8_t marker_fill[ODD_MARKER_WIDTH] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t marker_clear[ODD_MARKER_WIDTH] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

ssd1306_handle_t display_get_handle(void)
{
	return display_handle;
}

void display_init(void)
{
	// Initialize I2C master bus (new driver API)
	i2c_master_bus_config_t bus_config = {
		.i2c_port = I2C_NUM_0,
		.sda_io_num = CONFIG_OLED_I2C_SDA_GPIO,
		.scl_io_num = CONFIG_OLED_I2C_SCL_GPIO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};

	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

	// Initialize SSD1306 on the I2C bus
	ssd1306_config_t ssd1306_conf = I2C_SSD1306_128x64_CONFIG_DEFAULT;

	esp_err_t ret = ssd1306_init(i2c_bus, &ssd1306_conf, &display_handle);
	if (ret != ESP_OK || display_handle == NULL) {
		ESP_LOGE(TAG, "Failed to initialize SSD1306: %s", esp_err_to_name(ret));
		return;
	}

	ssd1306_clear_display(display_handle, false);
	ssd1306_enable_display(display_handle);

	ESP_LOGI(TAG, "OLED display initialized on SCL:%d, SDA:%d", CONFIG_OLED_I2C_SCL_GPIO, CONFIG_OLED_I2C_SDA_GPIO);
}

void display_invalidate(void)
{
	initialized = false;
}

void display_update(uint32_t count, uint32_t distance_mm)
{
	if (display_handle == NULL)
		return;

	TickType_t now = xTaskGetTickCount();

	const bool timeout = (now - last_refresh) >= pdMS_TO_TICKS(30000);
	const bool count_changed = (count >> 1) != last_count;
	const bool distance_changed = distance_mm != last_distance_mm;

	if (initialized && !count_changed && !distance_changed && !timeout)
		return;

	char buffer[22];

	if (!initialized) {
		ssd1306_clear_display(display_handle, false);
		ssd1306_display_text(display_handle, 1, debug_mode ? "MODO DEBUG   ;)" : "PUC Minas", false);
		ssd1306_display_text(display_handle, 7, "Botao p/ zerar", false);
	} else {
		if (count_changed) {
			snprintf(buffer, sizeof(buffer), "%-21s", "");
			ssd1306_display_text_x3(display_handle, 3, buffer, false);
		}
		if (distance_changed) {
			snprintf(buffer, sizeof(buffer), "%-21s", "");
			ssd1306_display_text(display_handle, 6, buffer, false);
		}
	}

	if (count_changed || !initialized) {
		snprintf(buffer, sizeof(buffer), "%5lu", (unsigned long)(count >> 1));
		ssd1306_display_text_x3(display_handle, 3, buffer, false);
	}

	if (debug_mode && (distance_changed || !initialized)) {
		uint32_t distance_cm = distance_mm / 10;
		snprintf(buffer, sizeof(buffer), "Medida: %5lucm", (unsigned long)distance_cm);
		ssd1306_display_text(display_handle, 6, buffer, false);
	}

	/* Always draw/clear the top-right marker to match current internal-count parity. */
	ssd1306_display_image(display_handle, ODD_MARKER_PAGE, ODD_MARKER_SEG, (count & 1) ? marker_fill : marker_clear,
			      ODD_MARKER_WIDTH);

	last_count = count >> 1;
	last_distance_mm = distance_mm;
	last_refresh = now;
	initialized = true;
}

void display_clear(void)
{
	if (display_handle)
		ssd1306_clear_display(display_handle, false);
}

void display_show_reset(void)
{
	if (display_handle == NULL)
		return;

	ssd1306_clear_display(display_handle, true);
	ssd1306_display_text(display_handle, 3, "  Zerado!", true);
}

void display_show_saved(void)
{
	if (display_handle == NULL)
		return;

	ssd1306_clear_display(display_handle, true);
	ssd1306_display_text(display_handle, 3, "  Salvo!", true);
}

void display_boot_puc(void)
{
	if (display_handle == NULL)
		return;

	ssd1306_clear_display(display_handle, true);
	ssd1306_display_text_x3(display_handle, 1, " PUC", true);
	ssd1306_display_text(display_handle, 4, "     Minas", true);
	ssd1306_display_text(display_handle, 6, "  Contador de", true);
	ssd1306_display_text(display_handle, 7, "    Pessoas", true);
}

void display_boot_credits(void)
{
	if (display_handle == NULL)
		return;

	ssd1306_clear_display(display_handle, false);
	ssd1306_display_text(display_handle, 1, "Copyright 2026", false);
	ssd1306_display_text(display_handle, 3, "Lucca Pellegrini", false);
	ssd1306_display_text(display_handle, 4, "Felipe Lara", false);
	ssd1306_display_text(display_handle, 6, "Engenharia de", false);
	ssd1306_display_text(display_handle, 7, "Computacao", false);
}

void display_boot_flash(void)
{
	if (display_handle == NULL)
		return;

	/* Fill every page solid white for the flash effect. */
	uint8_t white[128];
	memset(white, 0xFF, sizeof(white));
	for (uint8_t page = 0; page < 8; page++)
		ssd1306_display_image(display_handle, page, 0, white, 128);
}
