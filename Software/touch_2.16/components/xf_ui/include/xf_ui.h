/**
 * @file xf_ui.h
 * @brief Public entry point of the LVGL presentation layer.
 *
 * xf_ui owns the only LVGL task that mutates widgets, the single event queue and
 * the single mutable business state. Callers inside the firmware only start it
 * and then talk to it through events (design note 10.4).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "xf_app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the event queue and the controller task, then show the carousel.
 * @return ESP_OK on success; ESP_ERR_INVALID_STATE when already started;
 *         ESP_ERR_NO_MEM when the queue or the task could not be created.
 * @note Requires the display and touch to be initialised by xf_bsp first, and
 *       requires LVGL to live in its own task (esp_lvgl_port). Never call from
 *       inside LVGL context, because the task takes the LVGL lock immediately.
 */
esp_err_t xf_ui_start(void);

/**
 * @brief Stop the controller task and drop the queued events.
 * @return ESP_OK, or ESP_ERR_INVALID_STATE when not running.
 * @note The widgets are left as they are; the caller decides whether to clear
 *       the screen. Intended for tests and for a future sleep transition.
 */
esp_err_t xf_ui_stop(void);

/**
 * @brief Queue a model selection from another task (zero-based ID 0..17).
 * @return ESP_OK if queued, ESP_ERR_INVALID_ARG for an unknown model,
 *         ESP_ERR_INVALID_STATE before start, ESP_ERR_TIMEOUT for a full queue.
 * @note Not ISR-safe. This API does not create an I2C address or protocol.
 */
esp_err_t xf_ui_select_product(uint8_t product_id);

/** @brief UI-task callback containing product-ID bits to highlight externally. */
typedef void (*xf_ui_selection_sink_t)(uint32_t products, void *context);
/** @brief Register before xf_ui_start(); callbacks must not block or use LVGL. */
esp_err_t xf_ui_set_selection_sink(xf_ui_selection_sink_t sink, void *context);
/** @brief Queue an external DOWN/UP with an authoritative held-product mask. */
esp_err_t xf_ui_touchboard_event(uint8_t product_id, bool down, uint32_t held_products);
/** @brief Queue a held-product recovery snapshot (unchanged masks are inert). */
esp_err_t xf_ui_touchboard_snapshot(uint32_t held_products);

/**
 * @brief Read the current business state.
 * @return Borrowed pointer, valid for the lifetime of the application.
 * @warning The state is mutated by the controller task. Callers must treat the
 *          snapshot as best-effort and must not write through this pointer.
 */
const xf_app_state_t *xf_ui_state(void);

#ifdef __cplusplus
}
#endif