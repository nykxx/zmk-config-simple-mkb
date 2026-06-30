/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_drag_scroll

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/input_processor.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct drag_scroll_config {
    int16_t min_drag_distance;
};

struct drag_scroll_data {
    struct k_mutex lock;
    int32_t accumulated_drag;
};

static bool event_is_relative_xy(const struct input_event *event) {
    return event->type == INPUT_EV_REL && (event->code == INPUT_REL_X || event->code == INPUT_REL_Y);
}

static int drag_scroll_handle_event(const struct device *dev, struct input_event *event,
                                    uint32_t param1, uint32_t param2,
                                    struct zmk_input_processor_state *state) {
    if (!event_is_relative_xy(event)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const struct drag_scroll_config *cfg = dev->config;
    struct drag_scroll_data *data = dev->data;

    int ret = k_mutex_lock(&data->lock, K_FOREVER);
    if (ret < 0) {
        return ret;
    }

    data->accumulated_drag += abs(event->value);
    if (data->accumulated_drag < cfg->min_drag_distance) {
        k_mutex_unlock(&data->lock);
        return ZMK_INPUT_PROC_STOP;
    }

    data->accumulated_drag = 0;
    k_mutex_unlock(&data->lock);

    return ZMK_INPUT_PROC_CONTINUE;
}

static int drag_scroll_init(const struct device *dev) {
    struct drag_scroll_data *data = dev->data;
    k_mutex_init(&data->lock);

    return 0;
}

static const struct zmk_input_processor_driver_api drag_scroll_driver_api = {
    .handle_event = drag_scroll_handle_event,
};

#define DRAG_SCROLL_INST(n)                                                                        \
    static struct drag_scroll_data processor_drag_scroll_data_##n = {};                            \
    static const struct drag_scroll_config processor_drag_scroll_config_##n = {                    \
        .min_drag_distance = DT_INST_PROP_OR(n, min_drag_distance, 0),                             \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, drag_scroll_init, NULL, &processor_drag_scroll_data_##n,              \
                          &processor_drag_scroll_config_##n, POST_KERNEL,                          \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &drag_scroll_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DRAG_SCROLL_INST)
