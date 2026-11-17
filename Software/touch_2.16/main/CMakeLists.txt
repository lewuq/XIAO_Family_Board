# Application entry: the showcase lives in main/blink_example_main.c (the file
# keeps the historical example name). The leftover placeholder app_main.c is no
# longer registered, so only one app_main() is linked.
idf_component_register(SRCS "blink_example_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES xf_bsp xf_ui)