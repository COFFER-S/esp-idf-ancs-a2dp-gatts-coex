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
#include "esp_timer.h"
#include "coex_manager.h"

static const char *TAG = "COEX_MGR";

/* Static coexistence manager instance */
static coex_manager_t s_coex_mgr;
static SemaphoreHandle_t s_coex_mutex = NULL;
static esp_timer_handle_t s_radio_timer = NULL;

/* Radio allocation tracking */
static bool s_radio_owner[2] = {false, false}; /* 0=BLE(ANCS/GATTS), 1=BR/EDR(A2DP) */
static uint32_t s_radio_grant_time[2] = {0, 0};

/* State strings for logging */
static const char* state_str[] = {
    "IDLE",
    "ANCS_CONNECTING",
    "ANCS_CONNECTED",
    "A2DP_CONNECTING",
    "A2DP_CONNECTED",
    "GATTS_ADV",
    "GATTS_CONNECTED",
    "ALL_CONNECTED"
};

static const char* conn_type_str[] = {
    "ANCS",
    "A2DP",
    "GATTS"
};

/* Forward declarations */
static void radio_timer_callback(void* arg);
static void update_coex_state(void);

esp_err_t coex_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing coexistence manager");
    
    memset(&s_coex_mgr, 0, sizeof(s_coex_mgr));
    s_coex_mgr.state = COEX_STATE_IDLE;
    
    /* Create mutex for thread safety */
    s_coex_mutex = xSemaphoreCreateMutex();
    if (s_coex_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    /* Create radio timer for time-slicing */
    const esp_timer_create_args_t timer_args = {
        .callback = &radio_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "radio_timer"
    };
    
    esp_err_t ret = esp_timer_create(&timer_args, &s_radio_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create radio timer: %d", ret);
        vSemaphoreDelete(s_coex_mutex);
        return ret;
    }
    
    /* Start radio timer with 20ms period for time-slicing */
    esp_timer_start_periodic(s_radio_timer, 20000); /* 20ms */
    
    ESP_LOGI(TAG, "Coexistence manager initialized");
    return ESP_OK;
}

esp_err_t coex_manager_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing coexistence manager");
    
    if (s_radio_timer != NULL) {
        esp_timer_stop(s_radio_timer);
        esp_timer_delete(s_radio_timer);
        s_radio_timer = NULL;
    }
    
    if (s_coex_mutex != NULL) {
        vSemaphoreDelete(s_coex_mutex);
        s_coex_mutex = NULL;
    }
    
    memset(&s_coex_mgr, 0, sizeof(s_coex_mgr));
    
    ESP_LOGI(TAG, "Coexistence manager deinitialized");
    return ESP_OK;
}

coex_state_t coex_manager_get_state(void)
{
    coex_state_t state;
    
    if (xSemaphoreTake(s_coex_mutex, portMAX_DELAY) == pdTRUE) {
        state = s_coex_mgr.state;
        xSemaphoreGive(s_coex_mutex);
    }
    
    return state;
}

void coex_manager_set_connection(conn_type_t type, bool connected, 
                                  uint16_t conn_id, esp_bd_addr_t bda)
{
    if (type >= CONN_TYPE_MAX) {
        ESP_LOGE(TAG, "Invalid connection type: %d", type);
        return;
    }
    
    if (xSemaphoreTake(s_coex_mutex, portMAX_DELAY) == pdTRUE) {
        conn_info_t *conn = &s_coex_mgr.connections[type];
        
        conn->connected = connected;
        conn->conn_id = conn_id;
        if (bda != NULL) {
            memcpy(conn->remote_bda, bda, sizeof(esp_bd_addr_t));
        }
        
        if (connected) {
            conn->conn_time = xTaskGetTickCount();
            ESP_LOGI(TAG, "%s connected, conn_id=%d", conn_type_str[type], conn_id);
        } else {
            ESP_LOGI(TAG, "%s disconnected", conn_type_str[type]);
        }
        
        update_coex_state();
        
        xSemaphoreGive(s_coex_mutex);
    }
}

conn_info_t* coex_manager_get_connection(conn_type_t type)
{
    if (type >= CONN_TYPE_MAX) {
        return NULL;
    }
    
    return &s_coex_mgr.connections[type];
}

bool coex_manager_all_connected(void)
{
    bool all_connected = false;
    
    if (xSemaphoreTake(s_coex_mutex, portMAX_DELAY) == pdTRUE) {
        all_connected = (s_coex_mgr.connections[CONN_TYPE_ANCS].connected &&
                         s_coex_mgr.connections[CONN_TYPE_A2DP].connected &&
                         s_coex_mgr.connections[CONN_TYPE_GATTS].connected);
        xSemaphoreGive(s_coex_mutex);
    }
    
    return all_connected;
}

bool coex_manager_request_radio(uint8_t profile)
{
    if (profile >= 2) {
        return false;
    }
    
    bool granted = false;
    uint32_t current_time = xTaskGetTickCount();
    
    if (xSemaphoreTake(s_coex_mutex, portMAX_DELAY) == pdTRUE) {
        /* Simple time-slicing algorithm */
        uint32_t time_since_grant[2];
        time_since_grant[0] = current_time - s_radio_grant_time[0];
        time_since_grant[1] = current_time - s_radio_grant_time[1];
        
        /* Grant radio to the profile that hasn't had it for longest time */
        if (!s_radio_owner[0] && !s_radio_owner[1]) {
            /* No one has radio, grant to requestor */
            granted = true;
        } else if (s_radio_owner[profile]) {
            /* Already have radio */
            granted = true;
        } else if (!s_radio_owner[profile]) {
            /* Check if we should preempt */
            if (time_since_grant[profile] >= pdMS_TO_TICKS(50)) {
                /* Grant if other profile has had radio for >50ms */
                if (s_radio_owner[1-profile]) {
                    /* Release from other profile first */
                    s_radio_owner[1-profile] = false;
                }
                granted = true;
            }
        }
        
        if (granted) {
            s_radio_owner[profile] = true;
            s_radio_grant_time[profile] = current_time;
        }
        
        xSemaphoreGive(s_coex_mutex);
    }
    
    return granted;
}

void coex_manager_release_radio(uint8_t profile)
{
    if (profile >= 2) {
        return;
    }
    
    if (xSemaphoreTake(s_coex_mutex, portMAX_DELAY) == pdTRUE) {
        s_radio_owner[profile] = false;
        xSemaphoreGive(s_coex_mutex);
    }
}

void coex_manager_update_activity(void)
{
    if (xSemaphoreTake(s_coex_mutex, portMAX_DELAY) == pdTRUE) {
        s_coex_mgr.last_activity_time = xTaskGetTickCount();
        xSemaphoreGive(s_coex_mutex);
    }
}

void coex_manager_get_stats(coex_manager_t *stats)
{
    if (stats == NULL) {
        return;
    }
    
    if (xSemaphoreTake(s_coex_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(stats, &s_coex_mgr, sizeof(coex_manager_t));
        xSemaphoreGive(s_coex_mutex);
    }
}

void coex_manager_print_status(void)
{
    if (xSemaphoreTake(s_coex_mutex, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI(TAG, "=== Coexistence Status ===");
        ESP_LOGI(TAG, "State: %s", state_str[s_coex_mgr.state]);
        
        for (int i = 0; i < CONN_TYPE_MAX; i++) {
            ESP_LOGI(TAG, "%s: %s", 
                     conn_type_str[i],
                     s_coex_mgr.connections[i].connected ? "Connected" : "Disconnected");
        }
        
        ESP_LOGI(TAG, "Radio: BLE=%s, BR/EDR=%s",
                 s_radio_owner[0] ? "Y" : "N",
                 s_radio_owner[1] ? "Y" : "N");
        
        ESP_LOGI(TAG, "========================");
        
        xSemaphoreGive(s_coex_mutex);
    }
}

/* Static helper functions */

static void update_coex_state(void)
{
    /* Determine new state based on connection status */
    bool ancs = s_coex_mgr.connections[CONN_TYPE_ANCS].connected;
    bool a2dp = s_coex_mgr.connections[CONN_TYPE_A2DP].connected;
    bool gatts = s_coex_mgr.connections[CONN_TYPE_GATTS].connected;
    
    if (ancs && a2dp && gatts) {
        s_coex_mgr.state = COEX_STATE_ALL_CONNECTED;
    } else if (ancs && a2dp) {
        s_coex_mgr.state = COEX_STATE_A2DP_CONNECTED;
    } else if (ancs) {
        s_coex_mgr.state = COEX_STATE_ANCS_CONNECTED;
    } else if (gatts) {
        s_coex_mgr.state = COEX_STATE_GATTS_CONNECTED;
    } else {
        s_coex_mgr.state = COEX_STATE_IDLE;
    }
}

static void radio_timer_callback(void* arg)
{
    /* Periodic radio management - runs every 20ms */
    static uint32_t counter = 0;
    
    /* Every 50 ticks (1 second), print radio stats */
    counter++;
    if (counter >= 50) {
        counter = 0;
        /* Could add periodic stats reporting here */
    }
}

/* Event handler functions (stubs for compilation, will be implemented) */

void coex_manager_handle_gap_event(esp_gap_ble_cb_event_t event, 
                                   esp_ble_gap_cb_param_t *param)
{
    coex_manager_update_activity();
    
    /* Handle specific events */
    switch (event) {
        case ESP_GAP_BLE_CONNECT_EVT:
            ESP_LOGD(TAG, "BLE GAP connect event");
            break;
        case ESP_GAP_BLE_DISCONNECT_EVT:
            ESP_LOGD(TAG, "BLE GAP disconnect event");
            break;
        default:
            break;
    }
}

void coex_manager_handle_gattc_event(esp_gattc_cb_event_t event,
                                     esp_gatt_if_t gattc_if,
                                     esp_ble_gattc_cb_param_t *param)
{
    coex_manager_update_activity();
    
    /* ANCS uses GATT Client */
    switch (event) {
        case ESP_GATTC_CONNECT_EVT:
            ESP_LOGD(TAG, "GATTC connect, if=%d", gattc_if);
            break;
        case ESP_GATTC_DISCONNECT_EVT:
            ESP_LOGD(TAG, "GATTC disconnect, if=%d", gattc_if);
            break;
        default:
            break;
    }
}

void coex_manager_handle_gatts_event(esp_gatts_cb_event_t event,
                                     esp_gatt_if_t gatts_if,
                                     esp_ble_gatts_cb_param_t *param)
{
    coex_manager_update_activity();
    
    /* GATT Server events */
    switch (event) {
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGD(TAG, "GATTS connect, if=%d", gatts_if);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGD(TAG, "GATTS disconnect, if=%d", gatts_if);
            break;
        default:
            break;
    }
}
