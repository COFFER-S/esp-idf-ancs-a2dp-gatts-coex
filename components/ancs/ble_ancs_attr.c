/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ble_ancs.h"

static const char *TAG = "BLE_ANCS_ATTR";

/* Parse notification attributes from data source */
void ble_ancs_parse_attributes(uint8_t *data, uint16_t len, ancs_notification_t *notif)
{
    if (data == NULL || len < 5 || notif == NULL) {
        return;
    }

    uint16_t offset = 0;
    
    /* Command ID should be 0 (Get Notification Attributes Response) */
    if (data[offset++] != 0) {
        ESP_LOGW(TAG, "Invalid command ID");
        return;
    }
    
    /* Notification UID (4 bytes, little endian) */
    uint32_t uid = data[offset] | (data[offset+1] << 8) | 
                   (data[offset+2] << 16) | (data[offset+3] << 24);
    offset += 4;
    
    if (uid != notif->notification_uid) {
        ESP_LOGW(TAG, "Notification UID mismatch");
        return;
    }
    
    /* Parse attributes */
    while (offset < len) {
        /* Attribute ID */
        uint8_t attr_id = data[offset++];
        
        /* Attribute length (2 bytes, big endian) */
        if (offset + 2 > len) break;
        uint16_t attr_len = (data[offset] << 8) | data[offset+1];
        offset += 2;
        
        /* Attribute data */
        if (offset + attr_len > len) break;
        
        ESP_LOGI(TAG, "Attribute ID: %d, Length: %d", attr_id, attr_len);
        
        /* Print attribute value (truncate if too long) */
        char attr_str[128];
        uint16_t copy_len = attr_len < sizeof(attr_str) - 1 ? attr_len : sizeof(attr_str) - 1;
        memcpy(attr_str, &data[offset], copy_len);
        attr_str[copy_len] = '\0';
        ESP_LOGI(TAG, "Attribute value: %s", attr_str);
        
        offset += attr_len;
    }
}

/* Parse app attributes from data source */
void ble_ancs_parse_app_attributes(uint8_t *data, uint16_t len)
{
    if (data == NULL || len < 1) {
        return;
    }
    
    uint16_t offset = 0;
    
    /* Command ID should be 1 (Get App Attributes Response) */
    if (data[offset++] != 1) {
        ESP_LOGW(TAG, "Invalid command ID for app attributes");
        return;
    }
    
    /* App Identifier (null-terminated string) */
    char app_id[64] = {0};
    uint8_t i = 0;
    while (offset < len && data[offset] != 0 && i < sizeof(app_id) - 1) {
        app_id[i++] = data[offset++];
    }
    offset++; /* Skip null terminator */
    
    ESP_LOGI(TAG, "App ID: %s", app_id);
    
    /* Parse attributes */
    while (offset < len) {
        uint8_t attr_id = data[offset++];
        
        if (offset + 2 > len) break;
        uint16_t attr_len = (data[offset] << 8) | data[offset+1];
        offset += 2;
        
        if (offset + attr_len > len) break;
        
        char attr_str[128] = {0};
        uint16_t copy_len = attr_len < sizeof(attr_str) - 1 ? attr_len : sizeof(attr_str) - 1;
        memcpy(attr_str, &data[offset], copy_len);
        
        ESP_LOGI(TAG, "App Attribute %d: %s", attr_id, attr_str);
        
        offset += attr_len;
    }
}
