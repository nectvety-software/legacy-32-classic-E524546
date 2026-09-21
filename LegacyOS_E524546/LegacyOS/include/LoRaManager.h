#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════
//  LoRaManager - LoRa radio communication for LegacyOS
//  Hardware: SX1276/SX1278 module (connect to spare pins)
//  Library:  RadioLib (install from Arduino Library Manager)
//            https://github.com/jgromes/RadioLib
// ═══════════════════════════════════════════════════════════

// LoRa pin definitions (set your hardware pins here)
#define LORA_SCK_PIN    -1   // Change to your wiring
#define LORA_MISO_PIN   -1
#define LORA_MOSI_PIN   -1
#define LORA_CS_PIN     -1
#define LORA_RST_PIN    -1
#define LORA_IRQ_PIN    -1

// LoRa default config
#define LORA_FREQ      433.0   // MHz (433 / 868 / 915)
#define LORA_BW        125.0   // kHz
#define LORA_SF        9       // Spreading factor
#define LORA_CR        7       // Coding rate
#define LORA_SYNC      0x12   // Sync word
#define LORA_POWER     14      // dBm

struct LoRaPacket {
  String   data;
  int      rssi;
  float    snr;
  uint32_t timestamp;
  bool     sent;
};

class LoRaManager {
public:
  bool     available  = false;
  bool     initialized = false;
  float    frequency  = LORA_FREQ;
  int      spreadFactor = LORA_SF;
  float    bandwidth  = LORA_BW;
  int      power      = LORA_POWER;
  bool     rxMode     = true;
  bool     newPacket  = false;
  
  std::vector<LoRaPacket> rxHistory;
  std::vector<LoRaPacket> txHistory;
  int      packetCount = 0;
  
  void init() {
    if (LORA_CS_PIN < 0) {
      Serial.println(F("[LoRa] No LoRa pins configured - module disabled"));
      available = false;
      return;
    }
    
    /* ── With RadioLib installed: ──────────────────────────
    SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_CS_PIN);
    radio = new SX1276(new Module(LORA_CS_PIN, LORA_IRQ_PIN, LORA_RST_PIN));
    
    int state = radio->begin(frequency, bandwidth, spreadFactor, LORA_CR, LORA_SYNC, power);
    if (state == RADIOLIB_ERR_NONE) {
      available = initialized = true;
      radio->startReceive();
      Serial.println(F("[LoRa] SX1276 initialized"));
    } else {
      Serial.printf("[LoRa] Init failed: %d\n", state);
    }
    ────────────────────────────────────────────────────── */
    
    Serial.println(F("[LoRa] Stub mode - install RadioLib"));
    available = false;
  }
  
  bool send(const String& msg) {
    if (!available) return false;
    /* radio->transmit(msg.c_str()); */
    LoRaPacket p;
    p.data = msg; p.sent = true; p.timestamp = millis();
    txHistory.push_back(p);
    if (txHistory.size() > 50) txHistory.erase(txHistory.begin());
    return true;
  }
  
  bool receive(LoRaPacket& pkt) {
    if (!available || !newPacket) return false;
    newPacket = false;
    if (!rxHistory.empty()) { pkt = rxHistory.back(); return true; }
    return false;
  }
  
  void setFrequency(float f) {
    frequency = f;
    /* if (available) radio->setFrequency(f); */
  }
  
  String getStatus() {
    if (!available) return "LoRa: Not available";
    char buf[50];
    snprintf(buf, 50, "%.1f MHz SF%d BW%.0f Pwr%ddBm",
      frequency, spreadFactor, bandwidth, power);
    return String(buf);
  }

private:
  // SX1276* radio = nullptr;  // Uncomment with RadioLib
};

