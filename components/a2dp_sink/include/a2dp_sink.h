/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef A2DP_SINK_H
#define A2DP_SINK_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define A2DP_SINK_TAG "A2DP_SINK"

/* A2DP sink configuration */
typedef struct {
    const char *device_name;        /*!< Bluetooth device name */
    uint32_t sample_rate;           /*!< Audio sample rate (typically 44100 or 48000) */
    bool enable_avrc;               /*!< Enable AVRC remote control */
} a2dp_sink_cfg_t;

/* A2DP sink event types */
typedef enum {
    A2DP_SINK_EVENT_CONNECTED,      /*!< Connection established */
    A2DP_SINK_EVENT_DISCONNECTED,   /*!< Connection closed */
    A2DP_SINK_EVENT_AUDIO_DATA,     /*!< Audio data received */
    A2DP_SINK_EVENT_VOLUME_CHANGE,  /*!< Volume changed */
    A2DP_SINK_EVENT_PLAYBACK_CTRL,  /*!< Playback control (play/pause/etc) */
} a2dp_sink_event_t;

/* A2DP sink event data */
typedef struct {
    a2dp_sink_event_t type;
    union {
        struct {
            uint8_t *data;
            uint32_t len;
        } audio;
        struct {
            uint8_t volume;
        } volume;
        struct {
            uint8_t cmd;  /* play/pause/stop/etc */
        } playback;
    } data;
} a2dp_sink_event_data_t;

/* Event callback type */
typedef void (*a2dp_sink_event_cb_t)(a2dp_sink_event_t event, 
                                      a2dp_sink_event_data_t *data);

/**
 * @brief Initialize A2DP sink
 * 
 * @param cfg Configuration structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t a2dp_sink_init(const a2dp_sink_cfg_t *cfg);

/**
 * @brief Deinitialize A2DP sink
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t a2dp_sink_deinit(void);

/**
 * @brief Register event callback
 * 
 * @param cb Callback function
 * @return esp_err_t ESP_OK on success
 */
esp_err_t a2dp_sink_register_callback(a2dp_sink_event_cb_t cb);

/**
 * @brief Start A2DP sink (start advertising/listening)
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t a2dp_sink_start(void);

/**
 * @brief Stop A2DP sink
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t a2dp_sink_stop(void);

/**
 * @brief Get connection status
 * 
 * @return true if connected
 */
bool a2dp_sink_is_connected(void);

/**
 * @brief Set volume
 * 
 * @param volume Volume level (0-127)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t a2dp_sink_set_volume(uint8_t volume);

/**
 * @brief Send playback control command
 * 
 * @param cmd Command (play, pause, stop, etc.)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t a2dp_sink_send_control(uint8_t cmd);

#ifdef __cplusplus
}
#endif

#endif /* A2DP_SINK_H */
