#include "services.h"
#include "board_config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <NimBLEDevice.h>
#if USE_SD_CARD
#include <SD_MMC.h>
#endif

bool Services::begin() {
  const bool fsReady = LittleFS.begin(true);
  prefs_.begin("pochitta", false);
  brightness_ = prefs_.getUChar("brightness", 255);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(TFT_BL_PIN, TFT_BL_PWM_FREQ, TFT_BL_PWM_BITS);
#else
  ledcSetup(TFT_BL_PWM_CHANNEL, TFT_BL_PWM_FREQ, TFT_BL_PWM_BITS);
  ledcAttachPin(TFT_BL_PIN, TFT_BL_PWM_CHANNEL);
#endif
  setBrightness(brightness_);

  WiFi.mode(WIFI_STA);
  NimBLEDevice::init(DEVICE_NAME);

#if USE_SD_CARD
  SD_MMC.setPins(SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_D0_PIN);
  sdReady_ = SD_MMC.begin(
      SD_MMC_MOUNT_POINT,
      SD_MMC_ONE_BIT_MODE,
      SD_MMC_FORMAT_IF_FAIL);
#endif

  return fsReady;
}

std::vector<MenuItem> Services::wifiScan() {
  std::vector<MenuItem> out;
  const int count = WiFi.scanNetworks(false, true);
  for (int i = 0; i < count && i < 20; ++i) {
    out.push_back({WiFi.SSID(i), String(WiFi.RSSI(i)) + " dBm", false});
  }
  WiFi.scanDelete();
  if (out.empty()) out.push_back({"No networks found", "", false});
  return out;
}

std::vector<MenuItem> Services::bleScan() {
  std::vector<MenuItem> out;
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  NimBLEScanResults results = scan->getResults(4000, false);
  for (int i = 0; i < results.getCount() && i < 20; ++i) {
    const auto device = results.getDevice(i);
    String name = device->getName().c_str();
    if (!name.length()) name = device->getAddress().toString().c_str();
    out.push_back(MenuItem{name, String(device->getRSSI()) + " dBm", false});
  }
  scan->clearResults();
  if (out.empty()) out.push_back({"No BLE devices", "", false});
  return out;
}

std::vector<MenuItem> Services::listFiles(const String& path) {
  std::vector<MenuItem> out;
  File root = LittleFS.open(path);
  if (!root || !root.isDirectory()) {
    out.push_back({"Folder unavailable", "", false});
    return out;
  }

  File file = root.openNextFile();
  while (file && out.size() < 30) {
    out.push_back({String(file.name()),
                   file.isDirectory() ? "DIR" : String(file.size()) + " B",
                   false});
    file = root.openNextFile();
  }
  if (out.empty()) out.push_back({"Empty folder", "", false});
  return out;
}

std::vector<MenuItem> Services::listSDCard(const String& path) {
  std::vector<MenuItem> out;
#if USE_SD_CARD
  if (!sdReady_) {
    out.push_back({"SD card unavailable", "Check wiring", false});
    return out;
  }

  File root = SD_MMC.open(path);
  if (!root || !root.isDirectory()) {
    out.push_back({"Folder unavailable", path, false});
    return out;
  }

  File file = root.openNextFile();
  while (file && out.size() < 30) {
    out.push_back({String(file.name()),
                   file.isDirectory() ? "DIR" : String(file.size()) + " B",
                   false});
    file = root.openNextFile();
  }
#else
  out.push_back({"SD support disabled", "", false});
#endif
  if (out.empty()) out.push_back({"Empty SD card", "", false});
  return out;
}

uint64_t Services::sdTotalBytes() const {
#if USE_SD_CARD
  return sdReady_ ? SD_MMC.totalBytes() : 0;
#else
  return 0;
#endif
}

uint64_t Services::sdUsedBytes() const {
#if USE_SD_CARD
  return sdReady_ ? SD_MMC.usedBytes() : 0;
#else
  return 0;
#endif
}

bool Services::saveNote(const String& text) {
  const uint32_t id = prefs_.getUInt("note_id", 0) + 1;
  prefs_.putUInt("note_id", id);
  File file = LittleFS.open("/note_" + String(id) + ".txt", "w");
  if (!file) return false;
  file.print(text);
  file.close();
  return true;
}

std::vector<MenuItem> Services::listNotes() {
  const std::vector<MenuItem> all = listFiles("/");
  std::vector<MenuItem> notes;
  for (const auto& item : all) {
    if (item.label.startsWith("/note_") || item.label.startsWith("note_")) {
      notes.push_back(item);
    }
  }
  if (notes.empty()) notes.push_back({"No notes", "", false});
  return notes;
}

String Services::httpGet(const String& url) {
  if (WiFi.status() != WL_CONNECTED) return "WiFi is not connected.";
  HTTPClient http;
  http.setTimeout(6000);
  if (!http.begin(url)) return "Invalid URL";
  const int code = http.GET();
  String body = code > 0 ? http.getString() : http.errorToString(code);
  http.end();
  if (body.length() > 900) body = body.substring(0, 900);
  return body;
}

String Services::terminalCommand(const String& command) {
  String cmd = command;
  cmd.trim();
  if (cmd == "help") return "help, info, ls, sd, wifi, reboot";
  if (cmd == "info") {
    return String(DEVICE_MODEL) + "\nHeap: " + ESP.getFreeHeap() +
           "\nPSRAM: " + ESP.getPsramSize();
  }
  if (cmd == "ls") {
    const auto files = listFiles();
    String output;
    for (const auto& item : files) output += item.label + "\n";
    return output;
  }
  if (cmd == "sd") {
    return sdReady_ ? String("SD ready: ") + (sdTotalBytes() / 1024 / 1024) + " MB"
                    : "SD unavailable";
  }
  if (cmd == "wifi") {
    return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString()
                                         : "Disconnected";
  }
  if (cmd == "reboot") ESP.restart();
  return "Unknown command";
}

void Services::setBrightness(uint8_t value) {
  brightness_ = value;
  prefs_.putUChar("brightness", value);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(TFT_BL_PIN, value);
#else
  ledcWrite(TFT_BL_PWM_CHANNEL, value);
#endif
}
