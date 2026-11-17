# xf_bsp: board bring-up only (power, reset, QSPI panel, I2C touch, LVGL port).
#
# No page, string or product-data logic belongs in this component; the sources
# here must stay free of `xf_ui` and `xf_core` dependencies so the BSP can be
# reused by another application without dragging the showcase state along.

idf_component_register(
    SRCS
        "xf_bsp.c"
        "xf_bsp_display.c"
        "xf_bsp_touch.c"
        "xf_bsp_audio.c"
        "xf_pca9535.c"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "."
    REQUIRES
        esp_lcd
        esp_lcd_co5300
        esp_lcd_touch
        esp_lcd_touch_cst9220
        esp_lvgl_port
        lvgl
        esp_driver_i2c
        esp_driver_i2s
        esp_driver_spi
        esp_timer
        esp_system
        heap
        spi_flash
)