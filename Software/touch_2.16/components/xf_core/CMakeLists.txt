# xf_core: pure business logic, no LVGL and no hardware dependency.
# Keeping the dependency direction one-way (ui -> core -> data) lets the
# reducer run in host unit tests (code rules 10.3).
idf_component_register(SRCS "xf_compare.c"
                            "xf_idle.c"
                            "xf_state_reducer.c"
                       INCLUDE_DIRS "include"
                       REQUIRES xf_data)