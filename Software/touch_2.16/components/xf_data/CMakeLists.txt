# xf_data: immutable product catalogue and queries (design note 10.2).
# Registers local sources only; the generated table is produced by
# tools/gen_xf_data.py and must not be edited by hand.
idf_component_register(SRCS "xf_product_lookup.c"
                            "generated/xf_product_catalog.c"
                       INCLUDE_DIRS "include" "generated")