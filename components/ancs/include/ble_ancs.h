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

/* ANCS UUIDs */
#define ANCS_SERVICE_UUID {0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79}
#define ANCS_NOTIFICATION_SOURCE_UUID {0xBD, 0x1D, 0xA2, 0x99, 0xE6, 0x25, 0x58, 0x8C, 0xD9, 0x42, 0x01, 0x63, 0x0D, 0x12, 0xBF, 0x9F}
#define ANCS_CONTROL_POINT_UUID {0xD9, 0xD9, 0xAA, 0xFD, 0xBD, 0x9B, 0x21, 0x98, 0xA8, 0x49, 0xE1, 0x45, 0xF3, 0xD8, 0xD1, 0x69}
#define ANCS_DATA_SOURCE_UUID {0xFB, 0x7B, 0x7C, 0xCE, 0x6A, 0xB3, 0x44, 0xBE, 0xB5, 0x4B, 0xD6, 0x24, 0xE9, 0xC6, 0xEA, 0x22}

/* Notification Event IDs */
typedef enum {
    ANCS_EVT_NOTIFICATION_ADDED = 0,
    ANCS_EVT_NOTIFICATION_MODIFIED = 1,
    ANCS_EVT_NOTIFICATION_REMOVED = 2,
} ancs_evt_id_t;

/* Notification Flags */
typedef enum {
    ANCS_FLAG_SILENT = 0x01,
    ANCS_FLAG_IMPORTANT = 0x02,
} ancs_flag_t;

/* Category IDs */
typedef enum {
    ANCS_CAT_OTHER = 0,
    ANCS_CAT_INCOMING_CALL = 1,
    ANCS_CAT_MISSED_CALL = 2,
    ANCS_CAT_VOICEMAIL = 3,
    ANCS_CAT_SOCIAL = 4,
    ANCS_CAT_SCHEDULE = 5,
    ANCS_CAT_EMAIL = 6,
    ANCS_CAT_NEWS = 7,
    ANCS_CAT_HEALTH = 8,
    ANCS_CAT_BUSINESS = 9,
    ANCS_CAT_LOCATION = 10,
    ANCS_CAT_ENTERTAINMENT = 11,
} ancs_cat_id_t;

/* Command IDs */
typedef enum {
    ANCS_CMD_GET_NOTIFICATION_ATTRIBUTES = 0,
    ANCS_CMD_GET_APP_ATTRIBUTES = 1,
    ANCS_CMD_PERFORM_NOTIFICATION_ACTION = 2,
} ancs_cmd_id_t;

/* Notification Attribute IDs */
typedef enum {
    ANCS_ATTR_APP_ID = 0,
    ANCS_ATTR_TITLE = 1,
    ANCS_ATTR_SUBTITLE = 2,
    ANCS_ATTR_MESSAGE = 3,
    ANCS_ATTR_MESSAGE_SIZE = 4,
    ANCS_ATTR_DATE = 5,
    ANCS_ATTR_POSITIVE_ACTION_LABEL = 6,
    ANCS_ATTR_NEGATIVE_ACTION_LABEL = 7,
} ancs_attr_id_t;

/* Action IDs */
typedef enum {
    ANCS_ACTION_POSITIVE = 0,
    ANCS_ACTION_NEGATIVE = 1,
} ancs_action_id_t;

/* Notification data structure */
typedef struct {
    uint8_t event_id;
    uint32_t notification_uid;
    uint8_t flags;
    uint8_t category_id;
    uint8_t category_count;
    
    /* Attributes */
    char app_id[64];
    char title[256];
    char subtitle[256];
    char message[512];
    char message_size[16];
    char date[32];
    char positive_action_label[64];
    char negative_action_label[64];
} ancs_notification_t;

/* ANCS callback events */
typedef enum {
    ANCS_CB_EVENT_CONNECTED,
    ANCS_CB_EVENT_DISCONNECTED,
    ANCS_CB_EVENT_NOTIFICATION_RECEIVED,
    ANCS_CB_EVENT_NOTIFICATION_REMOVED,
} ancs_cb_event_t;

/* ANCS callback data */
typedef struct {
    ancs_cb_event_t event;
    union {
        ancs_notification_t notification;
        uint32_t notification_uid;
    } data;
} ancs_cb_data_t;

/* ANCS callback type */
typedef void (*ancs_cb_t)(ancs_cb_event_t event, ancs_cb_data_t *data);

/* ANCS configuration */
typedef struct {
    const char *device_name;
    ancs_cb_t callback;
} ancs_cfg_t;

/**
 * @brief Initialize ANCS client
 * 
 * @param cfg Configuration structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ancs_init(const ancs_cfg_t *cfg);

/**
 * @brief Deinitialize ANCS client
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ancs_deinit(void);

/**
 * @brief Start ANCS discovery
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ancs_start(void);

/**
 * @brief Stop ANCS
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ancs_stop(void);

/**
 * @brief Get notification attributes
 * 
 * @param notification_uid Notification UID
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ancs_get_notification_attributes(uint32_t notification_uid);

/**
 * @brief Perform notification action
 * 
 * @param notification_uid Notification UID
 * @param action Action to perform (positive/negative)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ancs_perform_notification_action(uint32_t notification_uid, ancs_action_id_t action);

/**
 * @brief Check if ANCS is connected
 * 
 * @return true if connected
 */
bool ancs_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_ANCS_H */
