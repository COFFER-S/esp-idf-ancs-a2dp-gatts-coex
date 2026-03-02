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
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_bt_device.h"
#include "a2dp_sink.h"

static const char *TAG = "A2DP_SINK";

/* A2DP sink state */
typedef enum {
    A2DP_STATE_IDLE = 0,
    A2DP_STATE_CONNECTING,
    A2DP_STATE_CONNECTED,
    A2DP_STATE_STREAMING,
} a2dp_sink_state_t;

/* A2DP sink context */
typedef struct {
    a2dp_sink_state_t state;
    a2dp_sink_cfg_t cfg;
    a2dp_sink_event_cb_t event_cb;

    esp_bd_addr_t remote_bda;
    bool connected;
    bool streaming;
    uint8_t volume;

    uint32_t packet_count;
    uint64_t byte_count;
} a2dp_sink_ctx_t;

static a2dp_sink_ctx_t s_a2dp_ctx;
static SemaphoreHandle_t s_a2dp_mutex = NULL;

/* Forward declarations */
static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
static void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len);
static void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);
static void bt_app_rc_tg_cb(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param);

/* Initialize A2DP sink */
esp_err_t a2dp_sink_init(const a2dp_sink_cfg_t *cfg)
{
    ESP_LOGI(TAG, "Initializing A2DP sink");

    if (cfg == NULL) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_a2dp_ctx, 0, sizeof(s_a2dp_ctx));
    s_a2dp_ctx.cfg = *cfg;
    s_a2dp_ctx.volume = 50;  /* Default volume 50% */

    /* Create mutex */
    s_a2dp_mutex = xSemaphoreCreateMutex();
    if (s_a2dp_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Set device name */
    esp_bt_dev_set_device_name(cfg->device_name);
    ESP_LOGI(TAG, "Device name: %s", cfg->device_name);

    /* Initialize A2DP sink */
    esp_err_t ret = esp_a2d_register_callback(&bt_app_a2d_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "A2DP callback registration failed: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_a2dp_mutex);
        return ret;
    }

    ret = esp_a2d_sink_register_data_callback(&bt_app_a2d_data_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "A2DP data callback registration failed: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_a2dp_mutex);
        return ret;
    }

    ret = esp_a2d_sink_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "A2DP sink init failed: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_a2dp_mutex);
        return ret;
    }

    /* Initialize AVRCP Controller */
    ret = esp_avrc_ct_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AVRC CT init failed: %s", esp_err_to_name(ret));
        esp_a2d_sink_deinit();
        vSemaphoreDelete(s_a2dp_mutex);
        return ret;
    }

    ret = esp_avrc_ct_register_callback(&bt_app_rc_ct_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AVRC CT callback registration failed: %s", esp_err_to_name(ret));
        esp_avrc_ct_deinit();
        esp_a2d_sink_deinit();
        vSemaphoreDelete(s_a2dp_mutex);
        return ret;
    }

    /* Initialize AVRCP Target */
    ret = esp_avrc_tg_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AVRC TG init failed: %s", esp_err_to_name(ret));
        esp_avrc_ct_deinit();
        esp_a2d_sink_deinit();
        vSemaphoreDelete(s_a2dp_mutex);
        return ret;
    }

    ret = esp_avrc_tg_register_callback(&bt_app_rc_tg_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AVRC TG callback registration failed: %s", esp_err_to_name(ret));
        esp_avrc_tg_deinit();
        esp_avrc_ct_deinit();
        esp_a2d_sink_deinit();
        vSemaphoreDelete(s_a2dp_mutex);
        return ret;
    }

    /* Set AVRCP capabilities */
    esp_avrc_rn_evt_cap_mask_t evt_set = {0};
    esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
    esp_avrc_tg_set_rn_evt_cap(&evt_set);

    ESP_LOGI(TAG, "A2DP sink initialized successfully");

    return ESP_OK;
}

/* Deinitialize A2DP sink */
esp_err_t a2dp_sink_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing A2DP sink");

    esp_avrc_tg_deinit();
    esp_avrc_ct_deinit();
    esp_a2d_sink_deinit();

    if (s_a2dp_mutex != NULL) {
        vSemaphoreDelete(s_a2dp_mutex);
        s_a2dp_mutex = NULL;
    }

    ESP_LOGI(TAG, "A2DP sink deinitialized");

    return ESP_OK;
}

/* Register event callback */
esp_err_t a2dp_sink_register_callback(a2dp_sink_event_cb_t cb)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_a2dp_mutex, portMAX_DELAY) == pdTRUE) {
        s_a2dp_ctx.event_cb = cb;
        xSemaphoreGive(s_a2dp_mutex);
    }

    return ESP_OK;
}

/* Start A2DP sink */
esp_err_t a2dp_sink_start(void)
{
    ESP_LOGI(TAG, "Starting A2DP sink");

    /* Set connectable and discoverable */
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    ESP_LOGI(TAG, "A2DP sink started, waiting for connections...");

    return ESP_OK;
}

/* Stop A2DP sink */
esp_err_t a2dp_sink_stop(void)
{
    ESP_LOGI(TAG, "Stopping A2DP sink");

    /* Set non-connectable and non-discoverable */
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

    ESP_LOGI(TAG, "A2DP sink stopped");

    return ESP_OK;
}

/* Check if connected */
bool a2dp_sink_is_connected(void)
{
    bool connected = false;

    if (xSemaphoreTake(s_a2dp_mutex, portMAX_DELAY) == pdTRUE) {
        connected = s_a2dp_ctx.connected;
        xSemaphoreGive(s_a2dp_mutex);
    }

    return connected;
}

/* Set volume */
esp_err_t a2dp_sink_set_volume(uint8_t volume)
{
    if (volume > 127) {
        volume = 127;
    }

    if (xSemaphoreTake(s_a2dp_mutex, portMAX_DELAY) == pdTRUE) {
        s_a2dp_ctx.volume = volume;
        xSemaphoreGive(s_a2dp_mutex);
    }

    /* Send volume change to remote */
    esp_avrc_rn_param_t rn_param;
    rn_param.volume = volume;
    esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_CHANGED, &rn_param);

    ESP_LOGI(TAG, "Volume set to %d", volume);

    return ESP_OK;
}

/* Send playback control */
esp_err_t a2dp_sink_send_control(uint8_t cmd)
{
    ESP_LOGI(TAG, "Sending playback control: %d", cmd);

    esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_PRESSED);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_RELEASED);

    return ESP_OK;
}

/* A2DP callback */
static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
            ESP_LOGI(TAG, "A2DP connection state: %d", param->conn_stat.state);

            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                memcpy(s_a2dp_ctx.remote_bda, param->conn_stat.remote_bda, sizeof(esp_bd_addr_t));
                s_a2dp_ctx.connected = true;
                s_a2dp_ctx.state = A2DP_STATE_CONNECTED;
                ESP_LOGI(TAG, "A2DP connected to: %02x:%02x:%02x:%02x:%02x:%02x",
                         param->conn_stat.remote_bda[0], param->conn_stat.remote_bda[1],
                         param->conn_stat.remote_bda[2], param->conn_stat.remote_bda[3],
                         param->conn_stat.remote_bda[4], param->conn_stat.remote_bda[5]);
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                s_a2dp_ctx.connected = false;
                s_a2dp_ctx.streaming = false;
                s_a2dp_ctx.state = A2DP_STATE_IDLE;
                ESP_LOGI(TAG, "A2DP disconnected");
            }
            break;

        case ESP_A2D_AUDIO_STATE_EVT:
            ESP_LOGI(TAG, "A2DP audio state: %d", param->audio_stat.state);

            if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
                s_a2dp_ctx.streaming = true;
                s_a2dp_ctx.state = A2DP_STATE_STREAMING;
                ESP_LOGI(TAG, "A2DP audio streaming started");
            } else {
                s_a2dp_ctx.streaming = false;
                s_a2dp_ctx.state = A2DP_STATE_CONNECTED;
                ESP_LOGI(TAG, "A2DP audio streaming stopped");
            }
            break;

        case ESP_A2D_AUDIO_CFG_EVT:
            ESP_LOGI(TAG, "A2DP audio config, codec type: %d", param->audio_cfg.mcc.type);
            s_a2dp_ctx.cfg.sample_rate = 44100;  /* Default, should parse from config */
            break;

        case ESP_A2D_MEDIA_CTRL_ACK_EVT:
            ESP_LOGI(TAG, "A2DP media control ack: cmd=%d, status=%d",
                     param->media_ctrl_stat.cmd, param->media_ctrl_stat.status);
            break;

        default:
            ESP_LOGD(TAG, "A2DP event: %d", event);
            break;
    }
}

/* A2DP data callback */
static void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len)
{
    /* Update counters */
    s_a2dp_ctx.packet_count++;
    s_a2dp_ctx.byte_count += len;

    /* Here you would typically send the audio data to a DAC or audio codec */
    /* For now, we just count the packets */

    static uint32_t last_log = 0;
    uint32_t now = xTaskGetTickCount();
    if (now - last_log > pdMS_TO_TICKS(5000)) {  /* Log every 5 seconds */
        ESP_LOGI(TAG, "A2DP: %lu packets, %llu bytes, %lu pps",
                 (unsigned long)s_a2dp_ctx.packet_count, s_a2dp_ctx.byte_count,
                 (unsigned long)(s_a2dp_ctx.packet_count / 5));
        last_log = now;
    }
}

/* AVRCP controller callback */
static void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    switch (event) {
        case ESP_AVRC_CT_CONNECTION_STATE_EVT:
            ESP_LOGI(TAG, "AVRC CT connection state: %d", param->conn_stat.connected);
            break;

        case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
            ESP_LOGD(TAG, "AVRC passthrough response: %d", param->psth_rsp.tl);
            break;

        case ESP_AVRC_CT_METADATA_RSP_EVT:
            ESP_LOGI(TAG, "AVRC metadata response");
            /* Could parse metadata here (title, artist, album) */
            break;

        case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
            ESP_LOGI(TAG, "AVRC change notify: event id=%d", param->change_ntf.event_id);
            if (param->change_ntf.event_id == ESP_AVRC_RN_VOLUME_CHANGE) {
                ESP_LOGI(TAG, "Remote volume changed");
            }
            break;

        default:
            ESP_LOGD(TAG, "AVRC CT event: %d", event);
            break;
    }
}

/* AVRCP target callback */
static void bt_app_rc_tg_cb(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param)
{
    switch (event) {
        case ESP_AVRC_TG_CONNECTION_STATE_EVT:
            ESP_LOGI(TAG, "AVRC TG connection state: %d", param->conn_stat.connected);
            break;

        case ESP_AVRC_TG_REMOTE_FEATURES_EVT:
            ESP_LOGI(TAG, "AVRC TG remote features: 0x%lx", (unsigned long)param->rmt_feats.feat_mask);
            break;

        case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT:
            ESP_LOGI(TAG, "AVRC TG set absolute volume: %d", param->set_abs_vol.volume);
            a2dp_sink_set_volume(param->set_abs_vol.volume);
            break;

        default:
            ESP_LOGD(TAG, "AVRC TG event: %d", event);
            break;
    }
}
