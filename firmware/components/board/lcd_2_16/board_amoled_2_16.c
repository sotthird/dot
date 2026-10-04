/* Waveshare ESP32-S3-Touch-AMOLED-2.16: CO5300 QSPI AMOLED, CST9220 touch, AXP2101 PMIC.
 *
 * Placeholder: the drivers arrive in the next phases. */

#include "board_priv.h"
#include "esp_log.h"

static const char* TAG = "board";

void board_hw_init(board_hw_t* hw) {
    ESP_LOGE(TAG, "ESP32-S3-Touch-AMOLED-2.16 support is not implemented yet");
    abort();
}

void board_set_brightness(uint8_t percent) {
}
