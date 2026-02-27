/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef BT_INIT_H
#define BT_INIT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Bluetooth controller and stack
 * 
 * This function initializes:
 * - NVS flash
 * - Bluetooth controller (BTDM mode)
 * - Bluedroid stack
 * - Security parameters
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bt_init(void);

/**
 * @brief Deinitialize Bluetooth
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bt_deinit(void);

/**
 * @brief Check if Bluetooth is initialized
 * 
 * @return true if initialized
 */
bool bt_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* BT_INIT_H */
