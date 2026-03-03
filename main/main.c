/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"

#include "bt_init.h"
#include "coex_manager.h"
#include "a2dp_sink.h"
#include "gatts_server.h"
#include "ble_ancs.h"

static const char *TAG = "ANC_A2DP_GATTS";

/* Event group bits */
#define BT_INIT_DONE_BIT    BIT0
#define ANCS_READY_BIT      BIT1
#define A2DP_READY_BIT      BIT2
#define GATTS_READY_BIT     BIT3
#define ALL_READY_BIT       BIT4

static EventGroupHandle_t s_coex_event_group;

/* Forward declarations */
static void ancs_callback(ancs_cb_event_t event, ancs_cb_data_t *data);
static void a2dp_callback(a2dp_sink_event_t event, a2dp_sink_event_data_t *data);
static void gatts_callback(gatts_server_event_t event, gatts_server_event_data_t *data);

/**
 * @brief Task to handle coexistence state monitoring
 */
static void coex_monitor_task(void *pvParameters)
{
    coex_state_t prev_state = COEX_STATE_IDLE;
    coex_state_t curr_state;
    
    ESP_LOGI(TAG, "Coexistence monitor task started");
    
    while (1) {
        curr_state = coex_manager_get_state();
        
        if (curr_state != prev_state) {
            ESP_LOGI(TAG, "State changed: %d -> %d", prev_state, curr_state);
            coex_manager_print_status();
            prev_state = curr_state;
        }
        
        /* Check if all profiles are connected */
        if (coex_manager_all_connected()) {
            xEventGroupSetBits(s_coex_event_group, ALL_READY_BIT);
        } else {
            xEventGroupClearBits(s_coex_event_group, ALL_READY_BIT);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Task to demonstrate usage when all profiles connected
 */
static void demo_task(void *pvParameters)
{
    EventBits_t bits;
    
    ESP_LOGI(TAG, "Demo task started, waiting for all profiles to connect...");
    
    while (1) {
        bits = xEventGroupWaitBits(
            s_coex_event_group,
            ALL_READY_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );
        
        if (bits & ALL_READY_BIT) {
            ESP_LOGI(TAG, "=================================");
            ESP_LOGI(TAG, "All profiles connected!");
            ESP_LOGI(TAG, "- ANCS: Receiving iOS notifications");
            ESP_LOGI(TAG, "- A2DP: Audio streaming active");
            ESP_LOGI(TAG, "- GATTS: BLE services available");
            ESP_LOGI(TAG, "=================================");
            
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
}

/**
 * @brief Initialize ANCS component
 */
static esp_err_t ancs_init(void)
{
    ESP_LOGI(TAG, "Initializing ANCS...");
    
    ancs_cfg_t ancs_cfg = {
        .device_name = "ESP_COEX_ANCS_A2DP",
        .callback = ancs_callback,
    };
    
    esp_err_t ret = ancs_init(&ancs_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ANCS initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "ANCS initialized successfully");
    xEventGroupSetBits(s_coex_event_group, ANCS_READY_BIT);
    
    return ESP_OK;
}

/**
 * @brief Initialize A2DP Sink component
 */
static esp_err_t a2dp_sink_init(void)
{
    ESP_LOGI(TAG, "Initializing A2DP Sink...");
    
    a2dp_sink_cfg_t a2dp_cfg = {
        .device_name = "ESP_COEX_ANCS_A2DP",
        .sample_rate = 44100,
        .enable_avrc = true,
    };
    
    esp_err_t ret = a2dp_sink_init(&a2dp_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "A2DP Sink initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = a2dp_sink_register_callback(a2dp_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "A2DP callback registration failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "A2DP Sink initialized successfully");
    xEventGroupSetBits(s_coex_event_group, A2DP_READY_BIT);
    
    return ESP_OK;
}

/**
 * @brief Initialize GATT Server component
 */
static esp_err_t gatts_init(void)
{
    ESP_LOGI(TAG, "Initializing GATT Server...");
    
    gatts_server_cfg_t gatts_cfg = {
        .device_name = "ESP_COEX_ANCS_A2DP",
        .appearance = 0x00,
        .enable_battery = true,
        .enable_device_info = true,
    };
    
    esp_err_t ret = gatts_server_init(&gatts_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GATT Server initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = gatts_server_register_callback(gatts_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GATT callback registration failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "GATT Server initialized successfully");
    xEventGroupSetBits(s_coex_event_group, GATTS_READY_BIT);
    
    return ESP_OK;
}

/* Callback implementations */
static void ancs_callback(ancs_cb_event_t event, ancs_cb_data_t *data)
{
    switch (event) {
        case ANCS_CB_EVENT_CONNECTED:
            ESP_LOGI(TAG, "ANCS connected");
            coex_manager_set_connection(CONN_TYPE_ANCS, true, 0, NULL);
            break;
        case ANCS_CB_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "ANCS disconnected");
            coex_manager_set_connection(CONN_TYPE_ANCS, false, 0, NULL);
            break;
        default:
            break;
    }
}

static void a2dp_callback(a2dp_sink_event_t event, a2dp_sink_event_data_t *data)
{
    switch (event) {
        case A2DP_SINK_EVENT_CONNECTED:
            ESP_LOGI(TAG, "A2DP connected");
            coex_manager_set_connection(CONN_TYPE_A2DP, true, 0, NULL);
            break;
        case A2DP_SINK_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "A2DP disconnected");
            coex_manager_set_connection(CONN_TYPE_A2DP, false, 0, NULL);
            break;
        default:
            break;
    }
}

static void gatts_callback(gatts_server_event_t event, gatts_server_event_data_t *data)
{
    switch (event) {
        case GATTS_EVENT_CONNECT:
            ESP_LOGI(TAG, "GATTS connected");
            coex_manager_set_connection(CONN_TYPE_GATTS, true, data->conn_id, NULL);
            break;
        case GATTS_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "GATTS disconnected");
            coex_manager_set_connection(CONN_TYPE_GATTS, false, 0, NULL);
            break;
        default:
            break;
    }
}

void app_main(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ANCS + A2DP + GATTS Coexistence Example");
    ESP_LOGI(TAG, "========================================");
    
    /* Initialize NVS */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    /* Create event group for coexistence management */
    s_coex_event_group = xEventGroupCreate();
    if (s_coex_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }
    
    /* Initialize coexistence manager */
    ESP_ERROR_CHECK(coex_manager_init());
    
    /* Initialize Bluetooth stack */
    ESP_ERROR_CHECK(bt_init());
    xEventGroupSetBits(s_coex_event_group, BT_INIT_DONE_BIT);
    
    /* Initialize components */
    ESP_ERROR_CHECK(ancs_init());
    ESP_ERROR_CHECK(a2dp_sink_init());
    ESP_ERROR_CHECK(gatts_init());
    
    /* Create tasks */
    xTaskCreate(coex_monitor_task, "coex_monitor", 4096, NULL, 5, NULL);
    xTaskCreate(demo_task, "demo_task", 4096, NULL, 3, NULL);
    
    ESP_LOGI(TAG, "Initialization complete, running...");
    
    /* Main loop */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
