/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef COEX_MANAGER_H
#define COEX_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Coexistence state definitions */
typedef enum {
    COEX_STATE_IDLE = 0,
    COEX_STATE_ANCS_CONNECTING,
    COEX_STATE_ANCS_CONNECTED,
    COEX_STATE_A2DP_CONNECTING,
    COEX_STATE_A2DP_CONNECTED,
    COEX_STATE_GATTS_ADV,
    COEX_STATE_GATTS_CONNECTED,
    COEX_STATE_ALL_CONNECTED,
} coex_state_t;

/* Connection type */
typedef enum {
    CONN_TYPE_ANCS = 0,
    CONN_TYPE_A2DP,
    CONN_TYPE_GATTS,
    CONN_TYPE_MAX
} conn_type_t;

/* Connection info */
typedef struct {
    bool connected;
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
    uint32_t conn_time;
} conn_info_t;

/* Coexistence manager context */
typedef struct {
    coex_state_t state;
    conn_info_t connections[CONN_TYPE_MAX];
    
    /* Resource management */
    bool radio_busy;
    uint32_t last_activity_time;
    
    /* Statistics */
    uint32_t ancs_notify_count;
    uint32_t a2dp_pkt_count;
    uint32_t gatts_rw_count;
} coex_manager_t;

/**
 * @brief Initialize coexistence manager
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t coex_manager_init(void);

/**
 * @brief Deinitialize coexistence manager
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t coex_manager_deinit(void);

/**
 * @brief Get current coexistence state
 * 
 * @return coex_state_t Current state
 */
coex_state_t coex_manager_get_state(void);

/**
 * @brief Set connection status
 * 
 * @param type Connection type
 * @param connected true if connected, false if disconnected
 * @param conn_id Connection ID (for BLE)
 * @param bda Remote device address
 */
void coex_manager_set_connection(conn_type_t type, bool connected, 
                                  uint16_t conn_id, esp_bd_addr_t bda);

/**
 * @brief Get connection info
 * 
 * @param type Connection type
 * @return conn_info_t* Connection info pointer (NULL if not connected)
 */
conn_info_t* coex_manager_get_connection(conn_type_t type);

/**
 * @brief Check if all profiles are connected
 * 
 * @return true if ANCS, A2DP, and GATTS are all connected
 */
bool coex_manager_all_connected(void);

/**
 * @brief Request radio access
 * 
 * This function manages radio time-sharing between BR/EDR and BLE
 * 
 * @param profile Profile requesting radio (0=ANCS/GATTS, 1=A2DP)
 * @return true if radio access granted
 */
bool coex_manager_request_radio(uint8_t profile);

/**
 * @brief Release radio access
 * 
 * @param profile Profile releasing radio
 */
void coex_manager_release_radio(uint8_t profile);

/**
 * @brief Update activity timestamp
 * 
 * Called when any profile has activity
 */
void coex_manager_update_activity(void);

/**
 * @brief Get coexistence manager statistics
 * 
 * @param stats Pointer to coex_manager_t to fill
 */
void coex_manager_get_stats(coex_manager_t *stats);

/**
 * @brief Print coexistence status (for debugging)
 */
void coex_manager_print_status(void);

/**
 * @brief Handle BLE GAP event for coexistence
 * 
 * This function should be called from GAP callback
 * 
 * @param event GAP event
 * @param param GAP event parameters
 */
void coex_manager_handle_gap_event(esp_gap_ble_cb_event_t event, 
                                   esp_ble_gap_cb_param_t *param);

/**
 * @brief Handle BLE GATTC event for coexistence
 * 
 * This function should be called from GATTC callback
 * 
 * @param event GATTC event
 * @param gattc_if GATT interface
 * @param param GATTC event parameters
 */
void coex_manager_handle_gattc_event(esp_gattc_cb_event_t event,
                                     esp_gatt_if_t gattc_if,
                                     esp_ble_gattc_cb_param_t *param);

/**
 * @brief Handle BLE GATTS event for coexistence
 * 
 * This function should be called from GATTS callback
 * 
 * @param event GATTS event
 * @param gatts_if GATT interface
 * @param param GATTS event parameters
 */
void coex_manager_handle_gatts_event(esp_gatts_cb_event_t event,
                                     esp_gatt_if_t gatts_if,
                                     esp_ble_gatts_cb_param_t *param);

#ifdef __cplusplus
}
#endif

#endif /* COEX_MANAGER_H */
