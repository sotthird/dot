#include "board_priv.h"

#if !CONFIG_DOT_BOARD_AMOLED_2_16
/* Only the AMOLED board has a battery chip. */
bool board_battery_read(int* percent, bool* charging, bool* usb_power) {
    return false;
}
#endif

void board_init(void) {
    board_hw_t hw = {0};
    board_hw_init(&hw);
    board_lvgl_init(&hw);
}
