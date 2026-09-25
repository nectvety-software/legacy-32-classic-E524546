# v1.8 Apps and media — ESP32-S3 N16R8 / ST7789 240x320

## Applications menu
- **Calculator**: D-pad moves focus among 16 keys (`7 8 9 /`, `4 5 6 *`, `1 2 3 -`, `C 0 = +`). START/SELECT executes focused key; OPTION opens Decimal point / Backspace / Clear; A/B returns to Applications. Uses bounded decimal input and catches division by zero/non-finite results.
- **Stopwatch**: D-pad cycles Start/Pause, Lap, Reset. START/SELECT activates. Four laps retained in RAM until reset (only available when paused). Timer redraws only the central display every 100ms while visible; timing continues via millis() while in another app. It is not a deep-sleep alarm.

## Music
- `/Media/Music` WAV16, 8–48 kHz, mono/stereo.
- List START plays selected track (or toggles pause for same track) and opens Now Playing.
- Now Playing: Left/Right previous/next, Up/Down volume (5% steps, persisted), START play/pause, A returns to list.
- Options: Now Playing, Play/Pause, Next, Previous, Shuffle, Repeat Off/All/One, Stop, Rescan, Details.
- Automatic next track follows completed WAV in the main loop; repeat one restarts same track; repeat all wraps to first. Shuffle uses a 32-bit visited mask for up to 32 tracks, avoids replaying songs until each was tried, and stops at the end with Repeat Off. Repeat All starts a new shuffle cycle.
- No MP3 or Bluetooth A2DP decoder is included. Audio hardware requires an external configured I2S DAC. Browser network calls may block the main loop and interrupt background audio; seamless multitasking requires a future dedicated audio task.

## Gallery & BLE
- Gallery Options > Slideshow moves to the next image every 4 seconds while Gallery stays visible. OPTION in preview toggles slideshow, Back returns to list. BMP, JPEG, PNG use existing viewers. File Manager may open a file outside the indexed Pictures library as a one-item preview.
- BLE scan collects the 16 strongest advertisements based on RSSI, displays full address and signal in a detail page, then deinitializes NimBLE to return heap. **No BLE connect/pair/GATT/A2DP** is promised.

## Memory and compatibility
- Static CalculatorEngine and StopwatchEngine with no heap allocations in models.
- Existing procedural S60 colors/icons, WiFi+pin-only status strip, theme manager, signature-checked QEAPP installer, recovery/Safe Mode, NVS WiFi and portrait 240x320 remain unchanged.
- Music changes don't add a full-screen sprite. Gallery slideshow decodes directly into the display with the existing image service.
- For actual hardware run `pio run -t clean`, `pio run`, `pio run -t upload`, `pio device monitor -b 115200` and validate audio/BLE on the board.
