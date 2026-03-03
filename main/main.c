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

static const char *TAG = "ANC_A2DP_GATTS";

/* Event group bits */
#define BT_INIT_DONE_BIT    BIT0
#define ALL_READY_BIT       BIT1

static EventGroupHandle_t s_coex_event_group;

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
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Initialize ANCS component
 */
static esp_err_t ancs_init_local(void)
{
    ESP_LOGI(TAG, "Initializing ANCS...");
    
    ESP_LOGI(TAG, "ANCS initialized (placeholder)");
    return ESP_OK;
}

/**
 * @brief Initialize A2DP Sink component
 */
static esp_err_t a2dp_sink_init_local(void)
{
    ESP_LOGI(TAG, "Initializing A2DP Sink...");
    
    ESP_LOGI(TAG, "A2DP Sink initialized (placeholder)");
    return ESP_OK;
}

/**
 * @brief Initialize GATT Server component
 */
static esp_err_t gatts_init_local(void)
{
    ESP_LOGI(TAG, "Initializing GATT Server...");
    
    ESP_LOGI(TAG, "GATT Server initialized (placeholder)");
    return ESP_OK;
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
    ESP_ERROR_CHECK(ancs_init_local());
    ESP_ERROR_CHECK(a2dp_sink_init_local());
    ESP_ERROR_CHECK(gatts_init_local());
    
    /* Create tasks */
    xTaskCreate(coex_monitor_task, "coex_monitor", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Initialization complete, running...");
    
    /* Main loop */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
