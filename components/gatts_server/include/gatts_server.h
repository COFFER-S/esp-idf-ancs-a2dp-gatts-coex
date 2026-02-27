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

#define GATTS_SERVER_TAG "GATTS_SVC"
#define GATTS_DEVICE_NAME "ESP_COEX_ANCS_A2DP"

/* Service UUIDs */
#define GATTS_SERVICE_UUID_DEVICE_INFO  0x180A
#define GATTS_SERVICE_UUID_BATTERY      0x180F
#define GATTS_SERVICE_UUID_CUSTOM       0x00FF

/* Characteristic UUIDs */
#define GATTS_CHAR_UUID_MANUFACTURER    0x2A29
#define GATTS_CHAR_UUID_MODEL_NUMBER    0x2A24
#define GATTS_CHAR_UUID_SERIAL_NUMBER   0x2A25
#define GATTS_CHAR_UUID_FIRMWARE_REV    0x2A26
#define GATTS_CHAR_UUID_BATTERY_LEVEL   0x2A19
#define GATTS_CHAR_UUID_CUSTOM_DATA     0xFF01

/* Maximum attribute length */
#define GATTS_ATTR_VAL_LEN_MAX  600

/* GATT Server configuration */
typedef struct {
    const char *device_name;        /*!< Device name for advertising */
    uint16_t appearance;            /*!< Device appearance */
    bool enable_battery;            /*!< Enable battery service */
    bool enable_device_info;        /*!< Enable device info service */
} gatts_server_cfg_t;

/* GATT Server event types */
typedef enum {
    GATTS_EVENT_CONNECT,            /*!< Client connected */
    GATTS_EVENT_DISCONNECT,         /*!< Client disconnected */
    GATTS_EVENT_WRITE,              /*!< Write request */
    GATTS_EVENT_READ,               /*!< Read request */
    GATTS_EVENT_EXEC_WRITE,         /*!< Execute write */
    GATTS_EVENT_MTU,                /*!< MTU changed */
} gatts_server_event_t;

/* GATT Server event data */
typedef struct {
    gatts_server_event_t type;
    uint16_t conn_id;
    union {
        struct {
            uint16_t handle;
            uint8_t *value;
            uint16_t len;
            bool need_rsp;
        } write;
        struct {
            uint16_t handle;
        } read;
        struct {
            uint16_t mtu;
        } mtu;
    } data;
} gatts_server_event_data_t;

/* Event callback type */
typedef void (*gatts_server_event_cb_t)(gatts_server_event_t event,
                                          gatts_server_event_data_t *data);

/**
 * @brief Initialize GATT Server
 *
 * @param cfg Configuration structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_init(const gatts_server_cfg_t *cfg);

/**
 * @brief Deinitialize GATT Server
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_deinit(void);

/**
 * @brief Register event callback
 *
 * @param cb Callback function
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_register_callback(gatts_server_event_cb_t cb);

/**
 * @brief Start advertising
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_start_advertising(void);

/**
 * @brief Stop advertising
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_stop_advertising(void);

/**
 * @brief Send notification to client
 *
 * @param conn_id Connection ID
 * @param handle Characteristic handle
 * @param value Data to send
 * @param len Data length
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_send_notification(uint16_t conn_id, uint16_t handle,
                                           uint8_t *value, uint16_t len);

/**
 * @brief Send indication to client
 *
 * @param conn_id Connection ID
 * @param handle Characteristic handle
 * @param value Data to send
 * @param len Data length
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_send_indication(uint16_t conn_id, uint16_t handle,
                                         uint8_t *value, uint16_t len);

/**
 * @brief Check if connected
 *
 * @return true if connected
 */
bool gatts_server_is_connected(void);

/**
 * @brief Get connection count
 *
 * @return int Number of connected clients
 */
int gatts_server_get_connection_count(void);

/**
 * @brief Set battery level
 *
 * @param level Battery level (0-100)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gatts_server_set_battery_level(uint8_t level);

#ifdef __cplusplus
}
#endif

#endif /* GATTS_SERVER_H */
