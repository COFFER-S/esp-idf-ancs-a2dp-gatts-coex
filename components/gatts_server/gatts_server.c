/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "gatts_server.h"

static const char *TAG = "GATTS_SVC";

/* GATT Server State */
typedef enum {
    GATTS_STATE_IDLE = 0,
    GATTS_STATE_INIT,
    GATTS_STATE_ADVERTISING,
    GATTS_STATE_CONNECTED,
} gatts_state_t;

/* GATT Server Context */
typedef struct {
    gatts_state_t state;
    gatts_server_cfg_t cfg;
    gatts_server_event_cb_t event_cb;
    
    uint16_t gatts_if;
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
    bool connected;
    
    /* Service handles */
    uint16_t service_handle;
    uint16_t char_handle;
    uint16_t cccd_handle;
    
    uint8_t battery_level;
} gatts_ctx_t;

static gatts_ctx_t s_gatts_ctx;
static SemaphoreHandle_t s_gatts_mutex = NULL;

/* Forward declarations */
static void gatts_event_handler(esp_gatts_cb_event_t event, 
                                 esp_gatt_if_t gatts_if, 
                                 esp_ble_gatts_cb_param_t *param);
static void gap_event_handler(esp_gap_ble_cb_event_t event, 
                               esp_ble_gap_cb_param_t *param);
static esp_err_t start_advertising(void);

/* GATT Profile UUIDs */
#define GATTS_SERVICE_UUID 0x00FF
#define GATTS_CHAR_UUID    0xFF01
#define GATTS_DESCR_UUID   0x2902

/* Attribute table */
enum {
    GATTS_IDX_SVC,
    GATTS_IDX_CHAR_VAL,
    GATTS_IDX_CHAR_CFG,
    GATTS_IDX_NB,
};

static esp_gatts_attr_db_t gatt_db[GATTS_IDX_NB] = {
    /* Service Declaration */
    [GATTS_IDX_SVC] = 
    {{ESP_GATT_AUTO_RSP}, {
        ESP_UUID_LEN_16, 
        (uint8_t *)&GATTS_SERVICE_UUID, 
        ESP_GATT_PERM_READ, 
        sizeof(uint16_t), 
        sizeof(uint16_t), 
        (uint8_t *)&GATTS_SERVICE_UUID
    }},

    /* Characteristic Declaration */
    [GATTS_IDX_CHAR_VAL] = 
    {{ESP_GATT_AUTO_RSP}, {
        ESP_UUID_LEN_16, 
        (uint8_t *)&GATTS_CHAR_UUID, 
        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 
        GATTS_ATTR_VAL_LEN_MAX, 
        0, 
        NULL
    }},

    /* Client Characteristic Configuration Descriptor */
    [GATTS_IDX_CHAR_CFG] = 
    {{ESP_GATT_AUTO_RSP}, {
        ESP_UUID_LEN_16, 
        (uint8_t *)&GATTS_DESCR_UUID, 
        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 
        sizeof(uint16_t), 
        0, 
        NULL
    }},
};

esp_err_t gatts_server_init(const gatts_server_cfg_t *cfg)
{
    ESP_LOGI(TAG, "Initializing GATT Server");
    
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(&s_gatts_ctx, 0, sizeof(s_gatts_ctx));
    s_gatts_ctx.cfg = *cfg;
    s_gatts_ctx.state = GATTS_STATE_IDLE;
    s_gatts_ctx.battery_level = 100;
    
    /* Create mutex */
    s_gatts_mutex = xSemaphoreCreateMutex();
    if (s_gatts_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    /* Register GATTS callback */
    esp_err_t ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATTS callback: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_gatts_mutex);
        return ret;
    }
    
    /* Register GAP callback */
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GAP callback: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_gatts_mutex);
        return ret;
    }
    
    /* Register GATT app */
    ret = esp_ble_gatts_app_register(0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATT app: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_gatts_mutex);
        return ret;
    }
    
    s_gatts_ctx.state = GATTS_STATE_INIT;
    
    ESP_LOGI(TAG, "GATT Server initialized successfully");
    return ESP_OK;
}

esp_err_t gatts_server_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing GATT Server");
    
    if (s_gatts_mutex != NULL) {
        vSemaphoreDelete(s_gatts_mutex);
        s_gatts_mutex = NULL;
    }
    
    memset(&s_gatts_ctx, 0, sizeof(s_gatts_ctx));
    
    ESP_LOGI(TAG, "GATT Server deinitialized");
    return ESP_OK;
}

esp_err_t gatts_server_register_callback(gatts_server_event_cb_t cb)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_gatts_mutex, portMAX_DELAY) == pdTRUE) {
        s_gatts_ctx.event_cb = cb;
        xSemaphoreGive(s_gatts_mutex);
    }
    
    return ESP_OK;
}

static esp_err_t start_advertising(void)
{
    ESP_LOGI(TAG, "Starting BLE advertising");
    
    /* Configure advertising data */
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x0020,
        .max_interval = 0x0040,
        .appearance = 0x0000,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = 0,
        .p_service_uuid = NULL,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };
    
    /* Configure scan response data */
    esp_ble_adv_data_t scan_rsp_data = {
        .set_scan_rsp = true,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x0020,
        .max_interval = 0x0040,
        .appearance = 0x0000,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(uint16_t),
        .p_service_uuid = (uint8_t *)&GATTS_SERVICE_UUID,
        .flag = 0,
    };
    
    /* Set advertising data */
    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure adv data: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* Set scan response data */
    ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure scan rsp data: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Advertising data configured, waiting for GAP event to start");
    return ESP_OK;
}

esp_err_t gatts_server_start_advertising(void)
{
    ESP_LOGI(TAG, "Starting GATT Server advertising");
    
    if (s_gatts_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return start_advertising();
}

esp_err_t gatts_server_stop_advertising(void)
{
    ESP_LOGI(TAG, "Stopping GATT Server advertising");
    
    esp_err_t ret = esp_ble_gap_stop_advertising();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop advertising: %s", esp_err_to_name(ret));
        return ret;
    }
    
    if (xSemaphoreTake(s_gatts_mutex, portMAX_DELAY) == pdTRUE) {
        if (s_gatts_ctx.state == GATTS_STATE_ADVERTISING) {
            s_gatts_ctx.state = GATTS_STATE_INIT;
        }
        xSemaphoreGive(s_gatts_mutex);
    }
    
    ESP_LOGI(TAG, "Advertising stopped");
    return ESP_OK;
}

esp_err_t gatts_server_send_notification(uint16_t conn_id, uint16_t handle, 
                                          uint8_t *value, uint16_t len)
{
    if (value == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = esp_ble_gatts_send_indicate(s_gatts_ctx.gatts_if, conn_id, 
                                                 handle, len, value, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send notification: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t gatts_server_send_indication(uint16_t conn_id, uint16_t handle,
                                        uint8_t *value, uint16_t len)
{
    if (value == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = esp_ble_gatts_send_indicate(s_gatts_ctx.gatts_if, conn_id,
                                                 handle, len, value, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send indication: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

bool gatts_server_is_connected(void)
{
    bool connected = false;
    
    if (xSemaphoreTake(s_gatts_mutex, portMAX_DELAY) == pdTRUE) {
        connected = s_gatts_ctx.connected;
        xSemaphoreGive(s_gatts_mutex);
    }
    
    return connected;
}

int gatts_server_get_connection_count(void)
{
    return gatts_server_is_connected() ? 1 : 0;
}

esp_err_t gatts_server_set_battery_level(uint8_t level)
{
    if (level > 100) {
        level = 100;
    }
    
    if (xSemaphoreTake(s_gatts_mutex, portMAX_DELAY) == pdTRUE) {
        s_gatts_ctx.battery_level = level;
        xSemaphoreGive(s_gatts_mutex);
    }
    
    return ESP_OK;
}

/* GATT Event Handler */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, 
                                esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            ESP_LOGI(TAG, "GATTS_REG_EVT, status=%d, app_id=%d", 
                     param->reg.status, param->reg.app_id);
            if (param->reg.status == ESP_GATT_OK) {
                s_gatts_ctx.gatts_if = gatts_if;
                /* Create service */
                esp_ble_gatts_create_service(gatts_if, 
                    (esp_gatt_srvc_id_t *)gatt_db[GATTS_IDX_SVC].attr_value,
                    GATTS_ATTR_VAL_LEN_MAX);
            }
            break;
        }
        
        case ESP_GATTS_CREATE_EVT: {
            ESP_LOGI(TAG, "GATTS_CREATE_EVT, status=%d, service_handle=%d",
                     param->create.status, param->create.service_handle);
            if (param->create.status == ESP_GATT_OK) {
                s_gatts_ctx.service_handle = param->create.service_handle;
                /* Add characteristics */
                for (int i = GATTS_IDX_CHAR_VAL; i < GATTS_IDX_NB; i++) {
                    esp_ble_gatts_add_char(s_gatts_ctx.service_handle,
                                           gatt_db[i].attr_value,
                                           gatt_db[i].attr_control->auto_rsp,
                                           NULL);
                }
                /* Start service */
                esp_ble_gatts_start_service(s_gatts_ctx.service_handle);
            }
            break;
        }
        
        case ESP_GATTS_START_EVT: {
            ESP_LOGI(TAG, "GATTS_START_EVT, status=%d, service_handle=%d",
                     param->start.status, param->start.service_handle);
            if (param->start.status == ESP_GATT_OK) {
                s_gatts_ctx.state = GATTS_STATE_INIT;
                /* Start advertising */
                start_advertising();
            }
            break;
        }
        
        case ESP_GATTS_CONNECT_EVT: {
            ESP_LOGI(TAG, "GATTS_CONNECT_EVT, conn_id=%d", param->connect.conn_id);
            s_gatts_ctx.conn_id = param->connect.conn_id;
            memcpy(s_gatts_ctx.remote_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            s_gatts_ctx.connected = true;
            s_gatts_ctx.state = GATTS_STATE_CONNECTED;
            
            /* Notify callback */
            if (s_gatts_ctx.event_cb) {
                gatts_server_event_data_t event_data = {
                    .type = GATTS_EVENT_CONNECT,
                    .conn_id = param->connect.conn_id,
                };
                s_gatts_ctx.event_cb(GATTS_EVENT_CONNECT, &event_data);
            }
            break;
        }
        
        case ESP_GATTS_DISCONNECT_EVT: {
            ESP_LOGI(TAG, "GATTS_DISCONNECT_EVT, reason=%d", param->disconnect.reason);
            s_gatts_ctx.connected = false;
            s_gatts_ctx.state = GATTS_STATE_INIT;
            
            /* Notify callback */
            if (s_gatts_ctx.event_cb) {
                gatts_server_event_data_t event_data = {
                    .type = GATTS_EVENT_DISCONNECT,
                    .conn_id = s_gatts_ctx.conn_id,
                };
                s_gatts_ctx.event_cb(GATTS_EVENT_DISCONNECT, &event_data);
            }
            
            /* Restart advertising */
            start_advertising();
            break;
        }
        
        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(TAG, "GATTS_WRITE_EVT, handle=%d, len=%d", 
                     param->write.handle, param->write.len);
            
            /* Notify callback */
            if (s_gatts_ctx.event_cb) {
                gatts_server_event_data_t event_data = {
                    .type = GATTS_EVENT_WRITE,
                    .conn_id = param->write.conn_id,
                    .data.write.handle = param->write.handle,
                    .data.write.value = param->write.value,
                    .data.write.len = param->write.len,
                    .data.write.need_rsp = param->write.need_rsp,
                };
                s_gatts_ctx.event_cb(GATTS_EVENT_WRITE, &event_data);
            }
            break;
        }
        
        case ESP_GATTS_READ_EVT: {
            ESP_LOGI(TAG, "GATTS_READ_EVT, handle=%d", param->read.handle);
            
            /* Notify callback */
            if (s_gatts_ctx.event_cb) {
                gatts_server_event_data_t event_data = {
                    .type = GATTS_EVENT_READ,
                    .conn_id = param->read.conn_id,
                    .data.read.handle = param->read.handle,
                };
                s_gatts_ctx.event_cb(GATTS_EVENT_READ, &event_data);
            }
            break;
        }
        
        default:
            break;
    }
}

/* GAP Event Handler */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "GAP_ADV_DATA_SET_COMPLETE");
            /* Start advertising */
            esp_ble_gap_start_advertising(&adv_params);
            break;
            
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "GAP_SCAN_RSP_DATA_SET_COMPLETE");
            break;
            
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "GAP_ADV_START_COMPLETE - Advertising started!");
                s_gatts_ctx.state = GATTS_STATE_ADVERTISING;
            } else {
                ESP_LOGE(TAG, "GAP_ADV_START_COMPLETE - Failed, status=%d", 
                         param->adv_start_cmpl.status);
            }
            break;
            
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "GAP_ADV_STOP_COMPLETE");
            if (s_gatts_ctx.state == GATTS_STATE_ADVERTISING) {
                s_gatts_ctx.state = GATTS_STATE_INIT;
            }
            break;
            
        default:
            break;
    }
}

/* Advertising parameters */
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x0020,
    .adv_int_max        = 0x0040,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr          = {0},
    .peer_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* Start advertising */
static esp_err_t start_advertising(void)
{
    ESP_LOGI(TAG, "Starting BLE advertising");
    
    /* Configure advertising data */
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x0020,
        .max_interval = 0x0040,
        .appearance = 0x0000,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = 0,
        .p_service_uuid = NULL,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };
    
    /* Set advertising data */
    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure adv data: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Advertising data configured, waiting for GAP event to start");
    return ESP_OK;
}

/* Public API implementations */
esp_err_t gatts_server_start_advertising(void)
{
    ESP_LOGI(TAG, "Starting GATT Server advertising");
    
    if (s_gatts_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return start_advertising();
}

esp_err_t gatts_server_stop_advertising(void)
{
    ESP_LOGI(TAG, "Stopping GATT Server advertising");
    
    esp_err_t ret = esp_ble_gap_stop_advertising();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop advertising: %s", esp_err_to_name(ret));
        return ret;
    }
    
    if (xSemaphoreTake(s_gatts_mutex, portMAX_DELAY) == pdTRUE) {
        if (s_gatts_ctx.state == GATTS_STATE_ADVERTISING) {
            s_gatts_ctx.state = GATTS_STATE_INIT;
        }
        xSemaphoreGive(s_gatts_mutex);
    }
    
    ESP_LOGI(TAG, "Advertising stopped");
    return ESP_OK;
}

esp_err_t gatts_server_send_notification(uint16_t conn_id, uint16_t handle, 
                                          uint8_t *value, uint16_t len)
{
    if (value == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = esp_ble_gatts_send_indicate(s_gatts_ctx.gatts_if, conn_id, 
                                                 handle, len, value, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send notification: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t gatts_server_send_indication(uint16_t conn_id, uint16_t handle,
                                        uint8_t *value, uint16_t len)
{
    if (value == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = esp_ble_gatts_send_indicate(s_gatts_ctx.gatts_if, conn_id,
                                                 handle, len, value, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send indication: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

bool gatts_server_is_connected(void)
{
    bool connected = false;
    
    if (xSemaphoreTake(s_gatts_mutex, portMAX_DELAY) == pdTRUE) {
        connected = s_gatts_ctx.connected;
        xSemaphoreGive(s_gatts_mutex);
    }
    
    return connected;
}

int gatts_server_get_connection_count(void)
{
    return gatts_server_is_connected() ? 1 : 0;
}

esp_err_t gatts_server_set_battery_level(uint8_t level)
{
    if (level > 100) {
        level = 100;
    }
    
    if (xSemaphoreTake(s_gatts_mutex, portMAX_DELAY) == pdTRUE) {
        s_gatts_ctx.battery_level = level;
        xSemaphoreGive(s_gatts_mutex);
    }
    
    return ESP_OK;
}
