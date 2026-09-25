# G3 physical board test (PENDING)

1. Unzip VQEAF-OS and build `pio run -e vqeaf_os`. Do not upload if dependency compile fails. Preserve board's SD contents.
2. `pio run -e vqeaf_os -t upload`; inspect `pio device monitor -b 115200` for panic, brownout, NVS theme migration or SD errors.
3. Power on with no SD; expect VQEAF boot text and dark multi-tab launcher after splash. Navigate LEFT/RIGHT six tabs, UP/DOWN all items (Home has six and scrolls), OPTION modal, A back Home, B details.
4. Place sample themes in `SD:/System/Themes/`, reboot, choose **Settings tab → Theme manager → VQEAF Night/Day**. Verify palette and all app colors; remove SD and ensure no crash. Add one 125–260 KB Studio `.vqeaf` file with embedded images; it should import colors but *not* phoneShell assets.
5. Reuse the original signed `welcome.qeapp` in the Inbox. Install via **Applications tab → App installer** and confirm a row appears in Applications after install/reboot. Verify signature rejection for unsigned/corrupt bundle using original tools. Native Lua and `.vxp` are intentionally unsupported.
6. In a browser URL input editor, hold SELECT longer than 650 ms: log `[key] mode: T9` and use physical UP (digit 2) twice to type a→b. Hold SELECT again and use the virtual D-Pad grid; normal Home key outside the editor remains available.
7. Test legacy utility pages (WiFi, Bluetooth, Gallery, Music, Browser, Shell, Recovery), real TLS with valid/expired/wrong-host certificates, SD remove/reinsert and WiFi badge. Run `diag help` at 115200, per `docs/BOARD_TEST_V201.md`.
8. Run for 10 minutes, inspect stack/heap/PSRAM, check flicker on 100 list key presses, theme file corruption and crash recovery; record physical FPS and DMA performance. **Not executed in this generated source package**.

BoardConfig GPIO is unchanged: TFT SCL48 MOSI12 CS14 DC47 RST3 LED39; keypad MENU18 UP7 A15 LEFT45 START17 RIGHT6 OPTION8 DOWN46 B5 SELECT16; SDMMC 1-bit CLK13 CMD11 DAT0 9. The original optional audio 4/1/2 is unchanged; use verified hardware wiring only.
