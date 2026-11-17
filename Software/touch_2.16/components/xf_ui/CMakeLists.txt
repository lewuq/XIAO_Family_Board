# Presentation component: pages, theme, controller and the generated string table.
#
# xf_ui is the only component that links LVGL, and it depends on the other xf_*
# modules one way only (bsp is below ui, so nothing here includes a BSP header).
idf_component_register(
    SRCS
        "xf_theme.c"
        "xf_ui_widgets.c"
        "xf_ui_format.c"
        "xf_ui_controller.c"
        "xf_page_attract.c"
        "xf_page_catalog.c"
        "xf_page_detail.c"
        "xf_page_compare.c"
        "generated/xf_strings.c"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "."
        "generated"
    REQUIRES
        lvgl
        esp_lvgl_port
        esp_timer
        xf_core
        xf_data
        xf_assets
)