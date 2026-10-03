idf_component_register(
    SRCS
        "main.c"
        "app.c"
        "serial_link.c"
        "apps/ci/ci.c"
        "apps/ci/ci_ui.c"
        "apps/cpu/cpu.c"
        "apps/cpu/cpu_ui.c"
        "apps/spotify/spotify.c"
        "apps/spotify/spotify_ui.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        board
        esp_driver_usb_serial_jtag
        heap
)
