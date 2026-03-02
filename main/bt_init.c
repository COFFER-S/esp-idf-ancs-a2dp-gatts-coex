/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "bt_init.h"

static const char *TAG = "BT_INIT";

static bool s_bt_initialized = false;

esp_err_t bt_init(void)
{
    ESP_LOGI(TAG, "Initializing Bluetooth...");
    
    if (s_bt_initialized) {
        ESP_LOGW(TAG, "Bluetooth already initialized");
        return ESP_OK;
    }
    
    /* Initialize NVS - required for Bluetooth pairing info */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "Erasing NVS flash...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS flash initialized");
    
    /* Release BT controller memory for dual mode operation */
    ESP_LOGI(TAG, "Releasing BT controller memory...");
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret) {
        ESP_LOGW(TAG, "Failed to release classic BT memory: %s", esp_err_to_name(ret));
        /* Continue anyway, this is not fatal */
    }
    
    /* Initialize BT controller with BLE mode only (simpler for now) */
    ESP_LOGI(TAG, "Initializing BT controller...");
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "BT controller initialized");
    
    /* Enable BT controller in BLE mode */
    ESP_LOGI(TAG, "Enabling BT controller...");
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
        esp_bt_controller_deinit();
        return ret;
    }
    ESP_LOGI(TAG, "BT controller enabled");
    
    /* Initialize Bluedroid stack */
    ESP_LOGI(TAG, "Initializing Bluedroid...");
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    
    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return ret;
    }
    ESP_LOGI(TAG, "Bluedroid initialized");
    
    /* Enable Bluedroid */
    ESP_LOGI(TAG, "Enabling Bluedroid...");
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return ret;
    }
    ESP_LOGI(TAG, "Bluedroid enabled");
    
    s_bt_initialized = true;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Bluetooth initialization complete!");
    ESP_LOGI(TAG, "Mode: BLE Only");
    ESP_LOGI(TAG, "Profiles: ANCS + A2DP + GATTS");
    ESP_LOGI(TAG, "========================================");
    
    return ESP_OK;
}

esp_err_t bt_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing Bluetooth...");
    
    if (!s_bt_initialized) {
        ESP_LOGW(TAG, "Bluetooth not initialized");
        return ESP_OK;
    }
    
    /* Disable Bluedroid */
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    
    /* Disable and deinit controller */
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    
    s_bt_initialized = false;
    
    ESP_LOGI(TAG, "Bluetooth deinitialized");
    
    return ESP_OK;
}

bool bt_is_initialized(void)
{
    return s_bt_initialized;
}