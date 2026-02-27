/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef GATTS_SERVER_H
#define GATTS_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_gatts_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GATTS_SERVER_TAG "GATTS_SERVER"

/* Service UUIDs */
#define GATTS_SERVICE_UUID_DEVICE_INFO  0x180A
#define GATTS_SERVICE_UUID_BATTERY      0x180F
#define GATTS_SERVICE_UUID_CUSTOM     0x00FF

/* Characteristic UUIDs */
#define GATTS_CHAR_UUID_DEVICE_NAME     0x2A00
#define GATTS_CHAR_UUID_MANUFACTURER  0x2A29
#define GATTS_CHAR_UUID_BATTERY_LEVEL   0x2A19
#define GATTS_CHAR_UUID_CUSTOM_DATA     0xFF01

/* GATT Server configuration */
typedef struct {
    const char *device_name;
    uint16_t appearance;
    uint8_t *adv_data;
    uint16_t adv_data_len;
    uint8_t *scan_rsp_data;
    uint16_t scan_rsp_data_len;
} gatts_cfg_t;

/* GATT Server callback events */
typedef enum {
    GATTS_CB_EVENT_CONNECT,
    GATTS_CB_EVENT_DISCONNECT,
    GATTS_CB_EVENT_WRITE,
    GATTS_CB_EVENT_READ,
    GATTS_CB_EVENT_NOTIFY_COMPLETE,
} gatts_cb_event_t;

/* GATT Server callback data */
typedef struct {
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
    uint16_t handle;
    uint16_t offset;
    uint8_t *value;
    uint16_t len;
} gatts_cb_param_t;

/* GATT Server callback type */
typedef void (*gatts_cb_t)(gatts_cb_event_t event, gatts_cb_param_t *param);

/**
 * @brief Initialize GATT Server
 * 
 * @param cfg Configuration structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_init(const gatts_cfg_t *cfg);

/**
 * @brief Deinitialize GATT Server
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_deinit(void);

/**
 * @brief Start advertising
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_start_adv(void);

/**
 * @brief Stop advertising
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_stop_adv(void);

/**
 * @brief Send notification to client
 * 
 * @param conn_id Connection ID
 * @param handle Characteristic handle
 * @param value Data to send
 * @param len Data length
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_send_notify(uint16_t conn_id, uint16_t handle, uint8_t *value, uint16_t len);

/**
 * @brief Send indication to client
 * 
 * @param conn_id Connection ID
 * @param handle Characteristic handle
 * @param value Data to send
 * @param len Data length
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_send_indicate(uint16_t conn_id, uint16_t handle, uint8_t *value, uint16_t len);

/**
 * @brief Get connection status
 * 
 * @return true if connected
 */
bool gatts_server_is_connected(void);

/**
 * @brief Get connection ID
 * 
 * @return uint16_t Connection ID
 */
uint16_t gatts_server_get_conn_id(void);

/**
 * @brief Register callback function
 * 
 * @param cb Callback function
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_register_callback(gatts_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* GATTS_SERVER_H */
