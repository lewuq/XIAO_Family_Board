/*
 * 产品目录（只读模型）公共接口。
 *
 * 数据来源：交接包 product_catalog.json，经 tools/generate_catalog.py 生成
 * main/model/product_catalog_data.c。本头文件只声明类型与访问接口，不保存任何参数值。
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "product_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 参数状态：界面必须按此区分展示，三种形态不得混用（设计说明 §9.4）。 */
typedef enum {
    SPEC_STATUS_VERIFIED = 0,   /* 数据源已核实 */
    SPEC_STATUS_PENDING,        /* 待核实：数据源 unknown / 缺失 */
    SPEC_STATUS_CONFLICT,       /* 待核实：两份来源口径冲突 */
    SPEC_STATUS_NOT_APPLICABLE, /* 不适用：功能在该产品上不存在且不属于缺失 */
} spec_status_t;

/* 数据归属：芯片能力（soc）数据必须在界面标注，不得直接与板级参数并列。 */
typedef enum {
    SPEC_SCOPE_BOARD = 0,
    SPEC_SCOPE_SOC,
} spec_scope_t;

/* 单个对比/详情字段的展示单元。 */
typedef struct {
    const char *value; /* 展示文本（已按状态归一化，含芯片能力标记），行间以 '\n' 分隔 */
    const char *unit;  /* 单位，可为 NULL（此时与 value 同行显示） */
    uint8_t status;    /* spec_status_t */
    uint8_t scope;     /* spec_scope_t */
} spec_field_t;

typedef struct {
    uint16_t id;                 /* 稳定业务主键 */
    const char *id_string;       /* 字符串形式主键，仅用于日志/调试 */
    const char *name;            /* 展示全名 */
    const char *short_name;      /* 对比表头用短名 */
    const char *manufacturer;    /* 厂商原名 */
    uint8_t manufacturer_index;  /* 对应 g_manufacturer_names 下标 */
    const char *feature_en;      /* 特性英文（设计稿原文） */
    const char *feature_zh;      /* 特性中文 */
    const char *applications;    /* 应用领域，'\n' 分隔 */
    const char *notes;           /* 备注/数据缺口说明 */
    const char *sources;         /* 来源 ID，' / ' 分隔 */
    uint8_t asset;               /* asset_id_t */
    bool is_placeholder;         /* true: 使用其他型号素材占位，界面必须标注 */
    uint16_t placeholder_product_id; /* 占位素材所属产品，PRODUCT_ID_INVALID 表示无 */
} product_t;

extern const product_t g_products[PRODUCT_COUNT];
extern const spec_field_t g_specs[PRODUCT_COUNT][SPEC_FIELD_COUNT];
extern const char *const g_product_id_strings[PRODUCT_COUNT];
extern const char *const g_asset_keys[ASSET_COUNT];
extern const char *const g_manufacturer_names[PRODUCT_MANUFACTURER_COUNT];
extern const char *const g_detail_group_names[DETAIL_GROUP_COUNT];
extern const char *const g_compare_category_names[COMPARE_CATEGORY_COUNT];
extern const char *const g_field_names[SPEC_FIELD_COUNT];
extern const uint8_t g_field_detail_group[SPEC_FIELD_COUNT];
extern const int8_t g_field_compare_category[SPEC_FIELD_COUNT];

static inline const product_t *product_get(uint16_t id)
{
    return (id < PRODUCT_COUNT) ? &g_products[id] : NULL;
}

static inline const spec_field_t *product_spec(uint16_t id, uint8_t field)
{
    return (id < PRODUCT_COUNT && field < SPEC_FIELD_COUNT) ? &g_specs[id][field] : NULL;
}

/* 轮播顺序即 0..PRODUCT_COUNT-1；返回下一张的产品主键。 */
static inline uint16_t product_next(uint16_t id)
{
    return (uint16_t)((id + 1u) % PRODUCT_COUNT);
}

static inline uint16_t product_prev(uint16_t id)
{
    return (uint16_t)((id + PRODUCT_COUNT - 1u) % PRODUCT_COUNT);
}

static inline bool field_in_category(uint8_t field, uint8_t category)
{
    return g_field_compare_category[field] == (int8_t)category;
}

#ifdef __cplusplus
}
#endif