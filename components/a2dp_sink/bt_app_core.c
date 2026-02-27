/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include "bt_app_core.h"

static const char *TAG = "BT_APP_CORE";

/* Task handle for the A2DP task */
static TaskHandle_t s_bt_app_task_handle = NULL;

/* Ring buffer for A2DP data */
static RingbufHandle_t s_ringbuf_a2dp = NULL;

/* Buffer size for A2DP ring buffer */
#define RINGBUF_SIZE (32 * 1024)  /* 32KB buffer */

/* Structure for A2DP data message */
typedef struct {
    uint8_t *data;
    uint32_t len;
} a2dp_data_msg_t;

/* Task for handling A2DP data */
static void bt_app_task_handler(void *arg)
{
    uint8_t *data = NULL;
    size_t item_size = 0;

    ESP_LOGI(TAG, "BT APP task started");

    while (1) {
        /* Receive data from ring buffer */
        data = (uint8_t *)xRingbufferReceive(s_ringbuf_a2dp, &item_size, portMAX_DELAY);
        
        if (data != NULL && item_size > 0) {
            /* Process received data here */
            /* For now, we just log the size */
            ESP_LOGD(TAG, "Received %d bytes of audio data", item_size);
            
            /* Return item to ring buffer */
            vRingbufferReturnItem(s_ringbuf_a2dp, (void *)data);
        }
    }
}

esp_err_t bt_app_core_init(void)
{
    ESP_LOGI(TAG, "Initializing BT APP Core");

    /* Create ring buffer for A2DP data */
    s_ringbuf_a2dp = xRingbufferCreate(RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (s_ringbuf_a2dp == NULL) {
        ESP_LOGE(TAG, "Failed to create ring buffer");
        return ESP_ERR_NO_MEM;
    }

    /* Create BT APP task */
    BaseType_t ret = xTaskCreate(bt_app_task_handler, "BtAppTask", 
                                  2048, NULL, configMAX_PRIORITIES - 3,
                                  &s_bt_app_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create BT APP task");
        vRingbufferDelete(s_ringbuf_a2dp);
        s_ringbuf_a2dp = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "BT APP Core initialized successfully");
    return ESP_OK;
}

void bt_app_core_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing BT APP Core");

    /* Delete task */
    if (s_bt_app_task_handle != NULL) {
        vTaskDelete(s_bt_app_task_handle);
        s_bt_app_task_handle = NULL;
    }

    /* Delete ring buffer */
    if (s_ringbuf_a2dp != NULL) {
        vRingbufferDelete(s_ringbuf_a2dp);
        s_ringbuf_a2dp = NULL;
    }

    ESP_LOGI(TAG, "BT APP Core deinitialized");
}

esp_err_t bt_app_send_audio_data(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_ringbuf_a2dp == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Send data to ring buffer */
    BaseType_t ret = xRingbufferSend(s_ringbuf_a2dp, (void *)data, len, pdMS_TO_TICKS(100));
    if (ret != pdTRUE) {
        ESP_LOGW(TAG, "Failed to send audio data to ring buffer");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}
