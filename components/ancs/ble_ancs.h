/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef BLE_ANCS_H
#define BLE_ANCS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_gattc_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_ANCS_TAG "BLE_ANCS"
#define EXAMPLE_DEVICE_NAME "ESP_BLE_ANCS"

/* ANCS Notification Source Event Flags */
typedef enum {
    ANCS_EVENT_SILENT = (1 << 0),
    ANCS_EVENT_IMPORTANT = (1 << 1),
    ANCS_EVENT_PREEXISTING = (1 << 2),
    ANCS_EVENT_POSITIVE_ACTION = (1 << 3),
    ANCS_EVENT_NEGATIVE_ACTION = (1 << 4),
} ancs_event_flags_t;

/* ANCS Notification Event IDs */
typedef enum {
    ANCS_EVENT_ID_NOTIFICATION_ADDED = 0,
    ANCS_EVENT_ID_NOTIFICATION_MODIFIED = 1,
    ANCS_EVENT_ID_NOTIFICATION_REMOVED = 2,
} ancs_event_id_t;

/* ANCS Notification Category IDs */
typedef enum {
    ANCS_CATEGORY_ID_OTHER = 0,
    ANCS_CATEGORY_ID_INCOMING_CALL = 1,
    ANCS_CATEGORY_ID_MISSED_CALL = 2,
    ANCS_CATEGORY_ID_VOICEMAIL = 3,
    ANCS_CATEGORY_ID_SOCIAL = 4,
    ANCS_CATEGORY_ID_SCHEDULE = 5,
    ANCS_CATEGORY_ID_EMAIL = 6,
    ANCS_CATEGORY_ID_NEWS = 7,
    ANCS_CATEGORY_ID_HEALTH_AND_FITNESS = 8,
    ANCS_CATEGORY_ID_BUSINESS_AND_FINANCE = 9,
    ANCS_CATEGORY_ID_LOCATION = 10,
    ANCS_CATEGORY_ID_ENTERTAINMENT = 11,
} ancs_category_id_t;

/* ANCS Command IDs */
typedef enum {
    ANCS_COMMAND_ID_GET_NOTIFICATION_ATTRIBUTES = 0,
    ANCS_COMMAND_ID_GET_APP_ATTRIBUTES = 1,
    ANCS_COMMAND_ID_PERFORM_NOTIFICATION_ACTION = 2,
} ancs_command_id_t;

/* ANCS Notification Attribute IDs */
typedef enum {
    ANCS_NOTIFICATION_ATTRIBUTE_ID_APP_IDENTIFIER = 0,
    ANCS_NOTIFICATION_ATTRIBUTE_ID_TITLE = 1,
    ANCS_NOTIFICATION_ATTRIBUTE_ID_SUBTITLE = 2,
    ANCS_NOTIFICATION_ATTRIBUTE_ID_MESSAGE = 3,
    ANCS_NOTIFICATION_ATTRIBUTE_ID_MESSAGE_SIZE = 4,
    ANCS_NOTIFICATION_ATTRIBUTE_ID_DATE = 5,
    ANCS_NOTIFICATION_ATTRIBUTE_ID_POSITIVE_ACTION_LABEL = 6,
    ANCS_NOTIFICATION_ATTRIBUTE_ID_NEGATIVE_ACTION_LABEL = 7,
} ancs_notification_attribute_id_t;

/* ANCS Action IDs */
typedef enum {
    ANCS_ACTION_ID_POSITIVE = 0,
    ANCS_ACTION_ID_NEGATIVE = 1,
} ancs_action_id_t;

/* ANCS Notification Structure */
typedef struct {
    uint8_t event_id;
    uint8_t event_flags;
    uint8_t category_id;
    uint8_t category_count;
    uint32_t notification_uid;
} ancs_notification_t;

/* ANCS Attribute Structure */
typedef struct {
    uint8_t attribute_id;
    uint16_t attribute_len;
    uint8_t *attribute_data;
} ancs_attribute_t;

/* ANCS Callback Type */
typedef void (*ancs_notification_cb_t)(ancs_notification_t *notification);
typedef void (*ancs_attribute_cb_t)(uint32_t notification_uid, ancs_attribute_t *attributes, uint8_t count);

/**
 * @brief Initialize ANCS GATT Client
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_ancs_init(void);

/**
 * @brief Deinitialize ANCS GATT Client
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_ancs_deinit(void);

/**
 * @brief Start scanning for ANCS service (iOS devices)
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_ancs_start_scan(void);

/**
 * @brief Stop scanning
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_ancs_stop_scan(void);

/**
 * @brief Register notification callback
 * 
 * @param cb Callback function
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_ancs_register_notification_cb(ancs_notification_cb_t cb);

/**
 * @brief Register attribute callback
 * 
 * @param cb Callback function
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_ancs_register_attribute_cb(ancs_attribute_cb_t cb);

/**
 * @brief Get notification attributes
 * 
 * @param notification_uid Notification UID
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_ancs_get_notification_attributes(uint32_t notification_uid);

/**
 * @brief Perform notification action
 * 
 * @param notification_uid Notification UID
 * @param action_id Action ID (positive/negative)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_ancs_perform_action(uint32_t notification_uid, ancs_action_id_t action_id);

/**
 * @brief Check if connected to ANCS service
 * 
 * @return true if connected
 */
bool ble_ancs_is_connected(void);

/**
 * @brief Get connection state
 * 
 * @return int Connection state (0=idle, 1=scanning, 2=connecting, 3=connected)
 */
int ble_ancs_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_ANCS_H */
