/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_defs.h"
#include "ble_ancs.h"

static const char *TAG = "BLE_ANCS";

/* ANCS Service UUID */
#define ANCS_SERVICE_UUID_STR "7905F431-B5CE-4E99-A40F-4B1E122D00D0"

/* ANCS Characteristic UUIDs */
#define ANCS_NOTIFICATION_SOURCE_UUID "9FBF120D-6301-42D9-8C58-25E699A21DBD"
#define ANCS_CONTROL_POINT_UUID       "69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9"
#define ANCS_DATA_SOURCE_UUID         "22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB"

/* ANCS state */
typedef enum {
    ANCS_STATE_IDLE = 0,
    ANCS_STATE_SCANNING,
    ANCS_STATE_CONNECTING,
    ANCS_STATE_CONNECTED,
    ANCS_STATE_DISCOVERING,
    ANCS_STATE_READY,
} ancs_state_t;

/* ANCS context */
typedef struct {
    ancs_state_t state;
    
    /* GATT Client interface */
    esp_gatt_if_t gattc_if;
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
    
    /* Service handles */
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    
    /* Characteristic handles */
    uint16_t notif_source_handle;
    uint16_t notif_source_cccd_handle;
    uint16_t control_point_handle;
    uint16_t data_source_handle;
    uint16_t data_source_cccd_handle;
    
    /* Callbacks */
    ancs_notification_cb_t notif_cb;
    ancs_attribute_cb_t attr_cb;
} ancs_ctx_t;

/* Static context */
static ancs_ctx_t s_ancs_ctx;
static SemaphoreHandle_t s_ancs_mutex = NULL;

/* Forward declarations */
static void ancs_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void ancs_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void ancs_discover_services(void);

/* Initialize ANCS */
esp_err_t ble_ancs_init(void)
{
    ESP_LOGI(TAG, "Initializing ANCS");
    
    memset(&s_ancs_ctx, 0, sizeof(s_ancs_ctx));
    s_ancs_ctx.state = ANCS_STATE_IDLE;
    s_ancs_ctx.gattc_if = ESP_GATT_IF_NONE;
    
    /* Create mutex */
    s_ancs_mutex = xSemaphoreCreateMutex();
    if (s_ancs_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    /* Register GATT Client callback */
    esp_err_t ret = esp_ble_gattc_register_callback(ancs_gattc_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATTC callback: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_ancs_mutex);
        return ret;
    }
    
    /* Register GAP callback */
    ret = esp_ble_gap_register_callback(ancs_gap_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GAP callback: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_ancs_mutex);
        return ret;
    }
    
    /* Register GATT Client app */
    ret = esp_ble_gattc_app_register(0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATT app: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_ancs_mutex);
        return ret;
    }
    
    ESP_LOGI(TAG, "ANCS initialized successfully");
    return ESP_OK;
}

/* Deinitialize ANCS */
esp_err_t ble_ancs_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing ANCS");
    
    if (s_ancs_mutex != NULL) {
        vSemaphoreDelete(s_ancs_mutex);
        s_ancs_mutex = NULL;
    }
    
    ESP_LOGI(TAG, "ANCS deinitialized");
    return ESP_OK;
}

/* Start scanning for iOS devices */
esp_err_t ble_ancs_start_scan(void)
{
    ESP_LOGI(TAG, "Starting scan for iOS devices");
    
    if (xSemaphoreTake(s_ancs_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_ancs_ctx.state != ANCS_STATE_IDLE) {
        ESP_LOGW(TAG, "Invalid state for scan: %d", s_ancs_ctx.state);
        xSemaphoreGive(s_ancs_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    s_ancs_ctx.state = ANCS_STATE_SCANNING;
    
    /* Configure scan parameters */
    static esp_ble_scan_params_t ble_scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,
        .scan_window = 0x30,
        .scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE
    };
    
    esp_err_t ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set scan params: %s", esp_err_to_name(ret));
        s_ancs_ctx.state = ANCS_STATE_IDLE;
        xSemaphoreGive(s_ancs_mutex);
        return ret;
    }
    
    ret = esp_ble_gap_start_scanning(30);  /* Scan for 30 seconds */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start scanning: %s", esp_err_to_name(ret));
        s_ancs_ctx.state = ANCS_STATE_IDLE;
        xSemaphoreGive(s_ancs_mutex);
        return ret;
    }
    
    xSemaphoreGive(s_ancs_mutex);
    
    ESP_LOGI(TAG, "Scanning started");
    return ESP_OK;
}

/* Stop scanning */
esp_err_t ble_ancs_stop_scan(void)
{
    ESP_LOGI(TAG, "Stopping scan");
    
    esp_err_t ret = esp_ble_gap_stop_scanning();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop scanning: %s", esp_err_to_name(ret));
        return ret;
    }
    
    if (xSemaphoreTake(s_ancs_mutex, portMAX_DELAY) == pdTRUE) {
        if (s_ancs_ctx.state == ANCS_STATE_SCANNING) {
            s_ancs_ctx.state = ANCS_STATE_IDLE;
        }
        xSemaphoreGive(s_ancs_mutex);
    }
    
    ESP_LOGI(TAG, "Scanning stopped");
    return ESP_OK;
}

/* Register notification callback */
esp_err_t ble_ancs_register_notification_cb(ancs_notification_cb_t cb)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_ancs_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_ancs_ctx.notif_cb = cb;
    
    xSemaphoreGive(s_ancs_mutex);
    
    ESP_LOGI(TAG, "Notification callback registered");
    return ESP_OK;
}

/* Register attribute callback */
esp_err_t ble_ancs_register_attribute_cb(ancs_attribute_cb_t cb)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_ancs_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_ancs_ctx.attr_cb = cb;
    
    xSemaphoreGive(s_ancs_mutex);
    
    ESP_LOGI(TAG, "Attribute callback registered");
    return ESP_OK;
}

/* Get notification attributes */
esp_err_t ble_ancs_get_notification_attributes(uint32_t notification_uid)
{
    ESP_LOGI(TAG, "Getting notification attributes for UID: %lu", notification_uid);
    
    if (xSemaphoreTake(s_ancs_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_ancs_ctx.state != ANCS_STATE_READY) {
        ESP_LOGE(TAG, "Not ready to get attributes, state: %d", s_ancs_ctx.state);
        xSemaphoreGive(s_ancs_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Build command to get notification attributes */
    uint8_t cmd[20];
    uint8_t cmd_len = 0;
    
    cmd[cmd_len++] = ANCS_COMMAND_ID_GET_NOTIFICATION_ATTRIBUTES;
    
    /* Notification UID (little endian) */
    cmd[cmd_len++] = notification_uid & 0xFF;
    cmd[cmd_len++] = (notification_uid >> 8) & 0xFF;
    cmd[cmd_len++] = (notification_uid >> 16) & 0xFF;
    cmd[cmd_len++] = (notification_uid >> 24) & 0xFF;
    
    /* Attributes to get */
    cmd[cmd_len++] = ANCS_NOTIFICATION_ATTRIBUTE_ID_APP_IDENTIFIER;
    cmd[cmd_len++] = ANCS_NOTIFICATION_ATTRIBUTE_ID_TITLE;
    cmd[cmd_len++] = 0xFF;  /* Title max length */
    cmd[cmd_len++] = 0xFF;
    cmd[cmd_len++] = ANCS_NOTIFICATION_ATTRIBUTE_ID_MESSAGE;
    cmd[cmd_len++] = 0xFF;  /* Message max length */
    cmd[cmd_len++] = 0xFF;
    
    /* Write to control point */
    esp_err_t ret = esp_ble_gattc_write_char(
        s_ancs_ctx.gattc_if,
        s_ancs_ctx.conn_id,
        s_ancs_ctx.control_point_handle,
        cmd_len,
        cmd,
        ESP_GATT_WRITE_TYPE_RSP,
        ESP_GATT_AUTH_REQ_NONE
    );
    
    xSemaphoreGive(s_ancs_mutex);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write control point: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Get notification attributes command sent");
    return ESP_OK;
}

/* Perform notification action */
esp_err_t ble_ancs_perform_action(uint32_t notification_uid, ancs_action_id_t action_id)
{
    ESP_LOGI(TAG, "Performing action %d on notification %lu", action_id, notification_uid);
    
    if (xSemaphoreTake(s_ancs_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_ancs_ctx.state != ANCS_STATE_READY) {
        ESP_LOGE(TAG, "Not ready to perform action, state: %d", s_ancs_ctx.state);
        xSemaphoreGive(s_ancs_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Build command to perform action */
    uint8_t cmd[10];
    uint8_t cmd_len = 0;
    
    cmd[cmd_len++] = ANCS_COMMAND_ID_PERFORM_NOTIFICATION_ACTION;
    
    /* Notification UID (little endian) */
    cmd[cmd_len++] = notification_uid & 0xFF;
    cmd[cmd_len++] = (notification_uid >> 8) & 0xFF;
    cmd[cmd_len++] = (notification_uid >> 16) & 0xFF;
    cmd[cmd_len++] = (notification_uid >> 24) & 0xFF;
    
    /* Action ID */
    cmd[cmd_len++] = action_id;
    
    /* Write to control point */
    esp_err_t ret = esp_ble_gattc_write_char(
        s_ancs_ctx.gattc_if,
        s_ancs_ctx.conn_id,
        s_ancs_ctx.control_point_handle,
        cmd_len,
        cmd,
        ESP_GATT_WRITE_TYPE_RSP,
        ESP_GATT_AUTH_REQ_NONE
    );
    
    xSemaphoreGive(s_ancs_mutex);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write control point: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Perform action command sent");
    return ESP_OK;
}

/* Check if connected */
bool ble_ancs_is_connected(void)
{
    bool connected = false;
    
    if (xSemaphoreTake(s_ancs_mutex, portMAX_DELAY) == pdTRUE) {
        connected = (s_ancs_ctx.state == ANCS_STATE_READY);
        xSemaphoreGive(s_ancs_mutex);
    }
    
    return connected;
}

/* Get connection state */
int ble_ancs_get_state(void)
{
    int state = 0;
    
    if (xSemaphoreTake(s_ancs_mutex, portMAX_DELAY) == pdTRUE) {
        state = (int)s_ancs_ctx.state;
        xSemaphoreGive(s_ancs_mutex);
    }
    
    return state;
}

/* GAP callback */
static void ancs_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            /* Handle scan results - look for iOS devices */
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                /* Check if this is an iOS device by looking for ANCS service in advertising data */
                /* For now, just log the device */
                char bda_str[18];
                sprintf(bda_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                        param->scan_rst.bda[0], param->scan_rst.bda[1],
                        param->scan_rst.bda[2], param->scan_rst.bda[3],
                        param->scan_rst.bda[4], param->scan_rst.bda[5]);
                ESP_LOGD(TAG, "Found device: %s, rssi: %d", bda_str, param->scan_rst.rssi);
            }
            break;
            
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "Scan stopped");
            break;
            
        default:
            break;
    }
}

/* GATTC callback */
static void ancs_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTC_REG_EVT:
            ESP_LOGI(TAG, "GATTC registered, app_id: %d, status: %d", param->reg.app_id, param->reg.status);
            if (param->reg.status == ESP_GATT_OK) {
                s_ancs_ctx.gattc_if = gattc_if;
            }
            break;
            
        case ESP_GATTC_CONNECT_EVT:
            ESP_LOGI(TAG, "GATTC connected, conn_id: %d", param->connect.conn_id);
            s_ancs_ctx.conn_id = param->connect.conn_id;
            memcpy(s_ancs_ctx.remote_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            
            if (xSemaphoreTake(s_ancs_mutex, portMAX_DELAY) == pdTRUE) {
                s_ancs_ctx.state = ANCS_STATE_CONNECTED;
                xSemaphoreGive(s_ancs_mutex);
            }
            
            /* Request MTU update */
            esp_ble_gattc_send_mtu_req(gattc_if, param->connect.conn_id);
            break;
            
        case ESP_GATTC_DISCONNECT_EVT:
            ESP_LOGI(TAG, "GATTC disconnected, reason: %d", param->disconnect.reason);
            
            if (xSemaphoreTake(s_ancs_mutex, portMAX_DELAY) == pdTRUE) {
                s_ancs_ctx.state = ANCS_STATE_IDLE;
                s_ancs_ctx.conn_id = 0;
                memset(s_ancs_ctx.remote_bda, 0, sizeof(esp_bd_addr_t));
                xSemaphoreGive(s_ancs_mutex);
            }
            break;
            
        case ESP_GATTC_CFG_MTU_EVT:
            ESP_LOGI(TAG, "MTU configured: %d", param->cfg_mtu.mtu);
            /* Start service discovery */
            ancs_discover_services();
            break;
            
        case ESP_GATTC_SEARCH_RES_EVT:
            /* Service found */
            ESP_LOGI(TAG, "Service found: UUID len=%d", param->search_res.srvc_id.uuid.len);
            s_ancs_ctx.service_start_handle = param->search_res.start_handle;
            s_ancs_ctx.service_end_handle = param->search_res.end_handle;
            break;
            
        case ESP_GATTC_SEARCH_CMPL_EVT:
            ESP_LOGI(TAG, "Service discovery complete: status=%d", param->search_cmpl.status);
            if (param->search_cmpl.status == ESP_GATT_OK) {
                if (xSemaphoreTake(s_ancs_mutex, portMAX_DELAY) == pdTRUE) {
                    s_ancs_ctx.state = ANCS_STATE_READY;
                    xSemaphoreGive(s_ancs_mutex);
                }
                ESP_LOGI(TAG, "ANCS ready!");
            }
            break;
            
        case ESP_GATTC_NOTIFY_EVT:
            /* Handle notification from iOS */
            ESP_LOGI(TAG, "Notification received: handle=%d", param->notify.handle);
            if (s_ancs_ctx.notif_cb && param->notify.value_len >= 8) {
                ancs_notification_t notif;
                notif.event_id = param->notify.value[0];
                notif.event_flags = param->notify.value[1];
                notif.category_id = param->notify.value[2];
                notif.category_count = param->notify.value[3];
                notif.notification_uid = param->notify.value[4] |
                                        (param->notify.value[5] << 8) |
                                        (param->notify.value[6] << 16) |
                                        (param->notify.value[7] << 24);
                s_ancs_ctx.notif_cb(&notif);
            }
            break;
            
        default:
            ESP_LOGD(TAG, "GATTC event: %d", event);
            break;
    }
}

/* Discover ANCS services */
static void ancs_discover_services(void)
{
    ESP_LOGI(TAG, "Discovering ANCS services");
    
    esp_bt_uuid_t ancs_service_uuid;
    ancs_service_uuid.len = ESP_UUID_LEN_128;
    
    /* ANCS Service UUID: 7905F431-B5CE-4E99-A40F-4B1E122D00D0 */
    uint8_t ancs_uuid[ESP_UUID_LEN_128] = {
        0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4,
        0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79
    };
    memcpy(ancs_service_uuid.uuid.uuid128, ancs_uuid, ESP_UUID_LEN_128);
    
    esp_err_t ret = esp_ble_gattc_search_service(
        s_ancs_ctx.gattc_if,
        s_ancs_ctx.conn_id,
        &ancs_service_uuid
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to search service: %s", esp_err_to_name(ret));
    }
}
