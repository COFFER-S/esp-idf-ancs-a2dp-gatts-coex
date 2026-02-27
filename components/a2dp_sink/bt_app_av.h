/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef BT_APP_AV_H
#define BT_APP_AV_H

#include <stdint.h>
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A2DP callback functions */
void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len);

/* AVRCP callback functions */
void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);
void bt_app_rc_tg_cb(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param);

#ifdef __cplusplus
}
#endif

#endif /* BT_APP_AV_H */
