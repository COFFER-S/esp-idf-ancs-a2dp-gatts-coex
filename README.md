# ANCS + A2DP + GATTS Coexistence Example

This example demonstrates the coexistence of three Bluetooth profiles:
- **ANCS** (Apple Notification Center Service) - GATT Client connecting to iOS devices
- **A2DP Sink** - Classic Bluetooth audio streaming receiver
- **GATT Server** - BLE server providing custom services

## Features

- Dual-mode Bluetooth (BLE + Classic) simultaneous operation
- iOS notification access via ANCS
- High-quality audio streaming via A2DP
- Custom BLE service for device configuration
- Coexistence management for resource sharing

## Hardware Requirements

- ESP32 series chip (ESP32, ESP32-S3, etc.)
- External audio DAC (optional, for A2DP audio output)
- iOS device (for ANCS functionality)

## Software Requirements

- ESP-IDF v5.2.1 (recommended and tested)
- Bluetooth Classic and BLE enabled in menuconfig

> **Note:** This project is tested and verified with ESP-IDF v5.2.1. While it may work with other versions, v5.2.1 is the recommended version for best compatibility.

## Project Structure

```
esp-idf-ancs-a2dp-gatts-coex/
├── CMakeLists.txt              # Project CMake file
├── main/
│   ├── CMakeLists.txt          # Main component CMake
│   ├── main.c                  # Application entry point
│   ├── coex_manager.c          # Coexistence manager
│   ├── coex_manager.h          # Coexistence manager header
│   ├── bt_init.c               # Bluetooth initialization
│   └── bt_init.h               # Bluetooth init header
├── components/
│   ├── ancs/                   # ANCS component
│   │   ├── ble_ancs.c
│   │   ├── ble_ancs.h
│   │   └── ble_ancs_attr.c
│   ├── a2dp_sink/              # A2DP Sink component
│   │   ├── bt_app_av.c
│   │   ├── bt_app_av.h
│   │   └── bt_app_core.c
│   └── gatts_server/           # GATT Server component
│       ├── gatts_server.c
│       └── gatts_server.h
└── README.md                   # This file
```

## Configuration

### Menuconfig Options

```bash
idf.py menuconfig
```

Required configurations:
- Component config → Bluetooth → [ ] Classic Bluetooth (BR/EDR) [x]
- Component config → Bluetooth → [ ] Bluetooth Low Energy [x]
- Component config → Bluetooth → Bluedroid Options → [ ] BT/BLE MAX ACL CONNECTIONS: 2

### Example Configuration

Copy the example configuration:
```bash
cp sdkconfig.defaults sdkconfig
```

## Build and Flash

### Requirements

**Required ESP-IDF Version:** v5.2.1 (tested and verified)

For other versions:
- v5.0.x: Should work with minor modifications
- v5.1.x: Tested, known to work
- **v5.2.1: ✅ Recommended and fully tested**
- v5.3.x: Should work, not yet verified

### Environment Setup

```bash
# Export ESP-IDF environment
. $IDF_PATH/export.sh

# Verify ESP-IDF version
idf.py --version
# Expected: ESP-IDF v5.2.1
```

### Build the project

```bash
# Set target (if not already set)
idf.py set-target esp32

# Build
idf.py build
```

### Flash to device

```bash
# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Or flash only
idf.py -p /dev/ttyUSB0 flash
```

### Monitor output

```bash
# Monitor only (no flash)
idf.py -p /dev/ttyUSB0 monitor

# Exit monitor: Ctrl + ]
```

## Usage

### Initial Setup

1. Power on the ESP32 device
2. The device will start advertising as "ESP_COEX_ANCS_A2DP"
3. For iOS devices: Pair with the device to enable ANCS
4. For audio: Connect via Bluetooth A2DP from any audio source

### ANCS Features

- Receive iOS notifications (calls, messages, apps)
- Read notification attributes (title, message, app name)
- Perform notification actions (accept/decline calls)

### A2DP Features

- Receive audio stream from smartphones, computers
- Control playback (play/pause, next/previous)
- Volume control

### GATT Server Features

- Device configuration service
- Status notification service
- Custom characteristic for app interaction

## Coexistence Implementation

### Resource Management

The coexistence manager handles:
- Bluetooth radio time-sharing between BR/EDR and BLE
- Connection priority management
- Memory allocation for different profiles

### Event Handling

Events are dispatched to appropriate handlers:
- GATT events → ANCS or GATT Server
- AVDTP events → A2DP Sink
- Connection events → Coexistence Manager

### Power Management

- Dynamic power saving when idle
- Sniff mode for BR/EDR connections
- BLE connection interval optimization

## Troubleshooting

### Common Issues

1. **ANCS not working**
   - Ensure iOS device is paired
   - Check Notification Access permission on iOS
   - Verify BLE connection is established

2. **No audio from A2DP**
   - Check audio routing configuration
   - Verify A2DP connection status
   - Check volume level

3. **Coexistence issues**
   - Increase connection interval
   - Reduce number of simultaneous connections
   - Check memory allocation

### Debug Logging

Enable debug logging:
```bash
idf.py menuconfig
# Component config → Log output → Default log verbosity → Debug
```

### Support

For issues and feature requests, please create an issue in the repository.

## License

This example is provided under the CC0-1.0 license.

See LICENSE file for details.

## Acknowledgments

- ESP-IDF Bluetooth examples
- ANCS specification from Apple
- A2DP specification from Bluetooth SIG
