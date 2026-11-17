# xf_assets: image, brand and font resources (design note 10.2).
#
# Images are linked straight from the pre-converted RGB565A8 payloads that
# tools/convert_assets.py writes into main/assets/img; nothing is copied and no
# run-time decoding is needed. Fonts and the brand logo are generated C sources
# under generated/ (tools/gen_xf_fonts.py, tools/gen_xf_brand.py).
idf_component_register(SRCS "xf_asset_images.c"
                            "generated/xf_brand_logo.c"
                            "generated/xf_font_ui_18.c"
                            "generated/xf_font_ui_22.c"
                            "generated/xf_font_ui_32.c"
                            "generated/xf_font_detail_23.c"
                            "generated/xf_font_detail_29.c"
                       INCLUDE_DIRS "include" "generated"
                       REQUIRES lvgl xf_data)

# The payloads are read-only project inputs, so they are referenced in place
# instead of being duplicated into the component.
set(_xf_img_dir "${CMAKE_CURRENT_LIST_DIR}/../../main/assets/img")

set(_xf_asset_keys
    samd21_front
    samd21_plus_front
    stm32c5_front
    mg24_sense_front
    rp2350_front
    rp2040_front
    rp2040_plus_front
    ra4m1_front
    esp32_c3_front
    esp32_c5_front
    esp32_c6_front
    esp32_s3_plus_front
    esp32_s3_sense_front
    nrf52840_sense_front
    nrf52840_plus_front
    nrf54l15_sense_front
    nrf54lm20a_sense_front
    nrf54lm20b_sense_front)

foreach(_key IN LISTS _xf_asset_keys)
    foreach(_prefix hero thumb)
        set(_bin "${_xf_img_dir}/${_prefix}_${_key}.bin")
        if(NOT EXISTS "${_bin}")
            message(FATAL_ERROR "xf_assets: missing image payload ${_bin}. "
                                "Run tools/build_assets.py before building.")
        endif()
        target_add_binary_data(${COMPONENT_LIB} "${_bin}" BINARY)
    endforeach()
endforeach()