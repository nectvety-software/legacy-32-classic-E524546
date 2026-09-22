# ESP32-Box

ESP32-Box is a handheld BLE HID controller with TFT display, environmental sensors, and multi-mode control capabilities.

## Hardware Components

| Component | Pin | Description |
|-----------|-----|-------------|
| TFT Display | 39, 47, 14, 48, 12, 3 | 1.8" SPI TFT |
| Button UP | 40 | D-pad up / Scroll |
| Button DOWN | 5 | D-pad down / Scroll |
| Button LEFT | 4 | D-pad left |
| Button RIGHT | 45 | D-pad right |
| Button A | 37 | OK / Left click |
| Button B | 36 | Back / Right click |
| Buzzer | 41 | Audio feedback |
| Battery | 1 | ADC pin for battery monitoring |
| BME280 | 18 (SDA), 6 (SCL) | Temperature, humidity, pressure sensor |

## Features

### Control Modes
- **Remote Mode**: Media playback control (play/pause, volume, skip)
- **Mouse Mode**: Air mouse via BLE
- **Both**: Combined remote + mouse functionality

### BLE Services
| Service | UUID | Description |
|---------|------|-------------|
| HID Service | 1812 | Keyboard/Mouse HID reports |
| Battery Service | 180F | Battery level notifications |
| UART Service | 6e400001-... | Command interface |

### BLE Commands (via UART)
```
STATUS           - Get device status
NAME:<text>      - Set device name
REMOTE_ON/OFF    - Toggle remote mode
MOUSE_ON/OFF     - Toggle mouse mode
MEDIA_PLAY       - Play/Pause
MEDIA_PREV/NEXT  - Skip tracks
MEDIA_VOL_UP/DOWN - Volume control
MOVE:<x>,<y>     - Send mouse coordinates
CLICK            - Left click
CLICK_R          - Right click
BL_ON/BL_OFF     - Backlight control
BUZZER           - Sound test
```

### BLE Client Commands
```
CLIENT_CONN:<addr>:<uuid> - Connect to device by address and optional UUID
CLIENT_UUID:<uuid>        - Connect to device by service UUID
CLIENT_DISC               - Disconnect from current device
CLIENT_PAIRED             - Check if device is connected
CLIENT_AVAIL              - Check bytes available to receive
CLIENT_READ               - Read received data
CLIENT_TX:<data>          - Send data to connected device
CLIENT_SCAN               - Scan for nearby BLE devices
CLIENT_SCAN_UUID:<uuid>   - Scan filtered by service UUID
CLIENT_ADDRS              - Get list of scanned device addresses
```

### BLE Client Features
- **Scan for devices**: Discover nearby BLE devices with RSSI
- **Connect by Address**: Connect to a specific device MAC address
- **Connect by UUID**: Connect to device advertising a specific service UUID
- **Disconnect**: Close active connection
- **Send Data**: Transmit data to connected device
- **Receive Data**: Read notifications from server
- **Quick Commands**: Send preset commands (HELLO, STATUS, PING, etc.)

### BLE Server Commands
```
SERVER_ACCEPT            - Start accepting connections
SERVER_ACCEPT_UUID:<uuid> - Accept connections for specific service UUID
SERVER_STOP              - Stop accepting connections
SERVER_STATUS             - Get server status
SERVER_DISCONNECT         - Disconnect current client
```

### BLE Server Features
- **Accept Connections**: Wait for BLE clients to connect
- **Accept with UUID**: Advertise specific service UUID
- **Multiple Services**: HID, Battery, UART services available
- **Auto Reconnect**: Automatically restart advertising after disconnect
- **Quick UUID Selection**: Choose from UART, HID, or Battery service

### On-Screen Keyboard
- Navigate with D-pad
- Toggle uppercase/lowercase with ABC
- Press OK to save name changes

## Pinout Diagram

```
    ┌─────────────────┐
    │   TFT DISPLAY   │
    │     240x160     │
    └─────────────────┘
    
    ┌─────────────────┐
    │  UP (40)        │
    │ LEFT(4)  RIGHT(45)│
    │  DOWN (5)       │
    │                 │
    │  A (37) B (36)  │
    └─────────────────┘
         │
         └── BUZZER (41)
```

## Build & Flash

1. Install ESP32 board support in Arduino IDE
2. Install required libraries:
   - TFT_eSPI
   - Adafruit BME280
   - ESP32 BLE
3. Select ESP32 Dev Module board
4. Upload to device

## Default Settings

- Device Name: `ESP32-Box`
- Click Sound: ON
- Mouse Speed: 10
- Backlight: ON

Settings are persisted in EEPROM.

## Usage

### Main Menu
- **HOME**: Display sensor data and connection status
- **CONTROL**: Select Remote/Mouse/Both mode
- **BLE SCAN**: Scan for BLE devices (for Server mode)
- **BLE CLIENT**: Connect to other BLE devices as client
- **BLE SERVER**: Accept incoming BLE connections
- **SETTINGS**: Configure device options

### BLE Client Mode
1. Select "BLE CLIENT" from main menu
2. Press OK or A to scan for nearby devices
3. Use UP/DOWN to select a device
4. Press OK to connect
5. Once connected, use SEND to transmit data

### BLE Server Mode
1. Select "BLE SERVER" from main menu
2. Press OK to start accepting connections
3. Use LEFT to accept with HID UUID
4. Use RIGHT to select specific UUID (UART/HID/Battery)
5. Wait for client to connect
6. Press OK again to stop accepting

### Control Mode
1. Enable Remote/Mouse mode in Settings
2. Connect as HID device to computer
3. Use D-pad for navigation, A/B for clicks
4. Hold UP/DOWN in media mode for TikTok scrolling
