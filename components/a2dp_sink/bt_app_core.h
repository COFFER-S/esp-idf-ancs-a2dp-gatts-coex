/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef BT_APP_CORE_H
#define BT_APP_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BT_APP_CORE_TAG "BT_APP_CORE"

/* BT app event type */
typedef enum {
    BT_APP_EVT_STACK_UP = 0,
    BT_APP_EVT_CONNECT,
    BT_APP_EVT_DISCONNECT,
    BT_APP_EVT_START_STREAM,
    BT_APP_EVT_STOP_STREAM,
} bt_app_evt_t;

/* BT app event message */
typedef struct {
    bt_app_evt_t evt;
    uint8_t data[0];  /* Variable length data follows */
} bt_app_msg_t;

/* BT app work function */
typedef void (* bt_app_cb_t)(uint16_t event, void *param);

/**
 * @brief Start BT app task
 * 
 * @return true on success
 */
bool bt_app_task_start_up(void);

/**
 * @brief Shut down BT app task
 */
void bt_app_task_shut_down(void);

/**
 * @brief Dispatch work to BT app task
 * 
 * @param callback Work callback function
 * @param event Event type
 * @param param Event parameters
 * @param param_len Parameter length
 * @return true on success
 */
bool bt_app_work_dispatch(bt_app_cb_t callback, uint16_t event, 
                          void *param, int param_len);

#ifdef __cplusplus
}
#endif

#endif /* BT_APP_CORE_H */
