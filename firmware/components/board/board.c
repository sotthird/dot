#include "board_priv.h"

void board_init(void) {
    board_hw_t hw = {0};
    board_hw_init(&hw);
    board_lvgl_init(&hw);
}
