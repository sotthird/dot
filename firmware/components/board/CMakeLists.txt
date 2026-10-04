if(CONFIG_DOT_BOARD_AMOLED_2_16)
    set(board_srcs
        "lcd_2_16/board_amoled_2_16.c"
    )
else()
    set(board_srcs
        "lcd_2_1/board_lcd_2_1.c"
        "lcd_2_1/cst820.c"
        "lcd_2_1/st7701s.c"
        "lcd_2_1/tca9554.c"
    )
endif()

idf_component_register(
    SRCS
        "board.c"
        "lvgl_port.c"
        ${board_srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_ledc
        esp_driver_spi
        esp_lcd
        esp_timer
        heap
)
