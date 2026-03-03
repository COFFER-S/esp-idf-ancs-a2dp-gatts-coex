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

/* GATT Profile UUIDs */
#define GATTS_SERVICE_UUID 0x00FF
#define GATTS_CHAR_UUID    0xFF01
#define GATTS_DESCR_UUID   0x2902

/* Attribute value storage */
static uint16_t gatts_service_uuid = GATTS_SERVICE_UUID;
static uint16_t gatts_char_uuid = GATTS_CHAR_UUID;
static uint16_t gatts_descr_uuid = GATTS_DESCR_UUID;

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

/* Forward declarations */
static void gatts_event_handler(esp_gatts_cb_event_t event, 
                                 esp_gatt_if_t gatts_if, 
                                 esp_ble_gatts_cb_param_t *param);
static void gap_event_handler(esp_gap_ble_cb_event_t event, 
                               esp_ble_gap_cb_param_t *param);
static esp_err_t start_advertising(void);

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
                
                /* Create service using local UUID storage */
                esp_gatt_srvc_id_t service_id = {
                    .is_primary = true,
                    .id = {
                        .uuid = {
                            .len = ESP_UUID_LEN_16,
                            .uuid = {.uuid16 = GATTS_SERVICE_UUID}
                        },
                        .inst_id = 0
                    }
                };
                esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_ATTR_VAL_LEN_MAX);
            }
            break;
        }
        
        case ESP_GATTS_CREATE_EVT: {
            ESP_LOGI(TAG, "GATTS_CREATE_EVT, status=%d, service_handle=%d",
                     param->create.status, param->create.service_handle);
            if (param->create.status == ESP_GATT_OK) {
                s_gatts_ctx.service_handle = param->create.service_handle;
                
                /* Add characteristic using local UUID */
                esp_bt_uuid_t char_uuid = {
                    .len = ESP_UUID_LEN_16,
                    .uuid = {.uuid16 = GATTS_CHAR_UUID}
                };
                esp_gatt_char_prop_t char_prop = ESP_GATT_CHAR_PROP_BIT_READ | 
                                                  ESP_GATT_CHAR_PROP_BIT_WRITE |
                                                  ESP_GATT_CHAR_PROP_BIT_NOTIFY;
                esp_ble_gatts_add_char(s_gatts_ctx.service_handle, &char_uuid,
                                       ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                       char_prop, NULL, NULL);
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
            
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "GAP_ADV_START_COMPLETE - Advertising started!");
                s_gatts_ctx.state = GATTS_STATE_ADVERTISING;
            } else {
                ESP_LOGE(TAG, "GAP_ADV_START_COMPLETE - Failed, status=%d", 
                         param->adv_start_cmpl.status);
            }
            break;
            
        default:
            break;
    }
}

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
        .service_uuid_len = sizeof(gatts_service_uuid),
        .p_service_uuid = (uint8_t *)&gatts_service_uuid,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };
    
    /* Set advertising data */
    esp_err_t ret = esp_ble_gap_set_device_name("ESP32-Coex");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set device name: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config adv data: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Advertising configuration complete");
    return ESP_OK;
}

/* Get GATT server connection status */
bool gatts_server_is_connected(void)
{
    return s_gatts_ctx.connected;
}

/* Get GATT server connection info */
void gatts_server_get_conn_info(uint16_t *conn_id, esp_bd_addr_t *remote_bda, uint8_t *battery_level)
{
    if (conn_id) *conn_id = s_gatts_ctx.conn_id;
    if (remote_bda) memcpy(*remote_bda, s_gatts_ctx.remote_bda, sizeof(esp_bd_addr_t));
    if (battery_level) *battery_level = s_gatts_ctx.battery_level;
}

/* Send notification */
esp_err_t gatts_server_send_notification(uint16_t conn_id, uint16_t handle, uint8_t *value, uint16_t len)
{
    if (!s_gatts_ctx.connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    (void)conn_id; /* unused - we use internal context */
    
    return esp_ble_gatts_send_indicate(s_gatts_ctx.gatts_if, s_gatts_ctx.conn_id,
                                       handle, len, value, false);
}

/* Update battery level */
esp_err_t gatts_server_update_battery_level(uint8_t level)
{
    s_gatts_ctx.battery_level = level;
    
    /* Send notification if connected */
    if (s_gatts_ctx.connected) {
        /* Battery level characteristic handle would be used here */
        ESP_LOGI(TAG, "Battery level updated: %d%%", level);
    }
    
    return ESP_OK;
}

/* Deinitialize GATT Server */
esp_err_t gatts_server_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing GATT Server");
    
    if (s_gatts_mutex != NULL) {
        vSemaphoreDelete(s_gatts_mutex);
        s_gatts_mutex = NULL;
    }
    
    memset(&s_gatts_ctx, 0, sizeof(s_gatts_ctx));
    
    return ESP_OK;
}

/* Register event callback */
esp_err_t gatts_server_register_callback(gatts_server_event_cb_t cb)
{
    s_gatts_ctx.event_cb = cb;
    return ESP_OK;
}

/* Start advertising wrapper */
esp_err_t gatts_server_start_advertising(void)
{
    return start_advertising();
}

/* Stop advertising wrapper */
esp_err_t gatts_server_stop_advertising(void)
{
    return esp_ble_gap_stop_advertising();
}

/* Send indication wrapper */
esp_err_t gatts_server_send_indication(uint16_t conn_id, uint16_t handle,
                                         uint8_t *value, uint16_t len)
{
    if (!s_gatts_ctx.connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    (void)conn_id; /* unused */
    
    return esp_ble_gatts_send_indicate(s_gatts_ctx.gatts_if, s_gatts_ctx.conn_id,
                                       handle, len, value, true);
}
