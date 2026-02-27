/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "bt_app_av.h"
#include "a2dp_sink.h"
#include "esp_log.h"

static const char *TAG = "BT_APP_AV";

void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    /* Callback function will be implemented in a2dp_sink.c */
}

void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len)
{
    /* Callback function will be implemented in a2dp_sink.c */
}

void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    /* Callback function will be implemented in a2dp_sink.c */
}

void bt_app_rc_tg_cb(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param)
{
    /* Callback function will be implemented in a2dp_sink.c */
}
