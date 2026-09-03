#include "nvs_util.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "nvs";

#define NVS_NAMESPACE "libcounter"
#define NVS_KEY_COUNT "person_count"

static bool nvs_ready = false;
static uint32_t last_saved = 0;
static bool has_last_saved = false;

void nvs_util_init(void)
{
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
	nvs_ready = true;
	ESP_LOGI(TAG, "NVS initialized");
}

uint32_t nvs_util_load_count(void)
{
	if (!nvs_ready)
		return 0;

	nvs_handle_t handle;
	if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
		return 0;

	uint32_t count = 0;
	esp_err_t err = nvs_get_u32(handle, NVS_KEY_COUNT, &count);
	nvs_close(handle);

	if (err != ESP_OK) {
		ESP_LOGI(TAG, "No saved count found (%s)", esp_err_to_name(err));
		return 0;
	}

	last_saved = count;
	has_last_saved = true;
	ESP_LOGI(TAG, "Loaded count %lu from NVS", (unsigned long)count);
	return count;
}

bool nvs_util_save_count(uint32_t count, bool force)
{
	if (!nvs_ready)
		return false;

	if (!force && has_last_saved && last_saved == count) {
		ESP_LOGD(TAG, "Count unchanged (%lu), skipping NVS write", (unsigned long)count);
		return false;
	}

	nvs_handle_t handle;
	if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to open NVS for write");
		return false;
	}

	esp_err_t err = nvs_set_u32(handle, NVS_KEY_COUNT, count);
	if (err == ESP_OK)
		err = nvs_commit(handle);
	nvs_close(handle);

	if (err == ESP_OK) {
		last_saved = count;
		has_last_saved = true;
		ESP_LOGI(TAG, "Saved count %lu to NVS", (unsigned long)count);
		return true;
	}

	ESP_LOGE(TAG, "Failed to save count to NVS: %s", esp_err_to_name(err));
	return false;
}

void nvs_util_clear_count(void)
{
	if (!nvs_ready)
		return;

	nvs_handle_t handle;
	if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to open NVS for clear");
		return;
	}

	esp_err_t err = nvs_erase_key(handle, NVS_KEY_COUNT);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
		ESP_LOGE(TAG, "Failed to erase count from NVS: %s", esp_err_to_name(err));
	else
		err = nvs_commit(handle);

	nvs_close(handle);

	last_saved = 0;
	has_last_saved = true;
	ESP_LOGI(TAG, "Cleared count from NVS");

	(void)err;
}
