#ifndef POCHITA_BITCHAT_H
#define POCHITA_BITCHAT_H

// ============================================================================
// BLE Chat - a local Bluetooth mesh chat (bitChat-style) for POCHITA OS.
//
// Every device advertises a small GATT "chat server" built on the Nordic UART
// Service (NUS) profile, so any two POCHITA devices can talk peer-to-peer:
//   * scan BLE devices around you (Option key rescans)
//   * connect to a peer (you become the GATT client, it stays the server)
//   * chat on screen using the physical-button virtual keyboard (full duplex)
// Long messages are split into a compact M/S/C/E chunk framing so they survive
// the 20-byte default BLE ATT packet size.
// ============================================================================

#include <Arduino.h>
#include "component/Display.h"
#include "component/Config.h"
#include "component/Keyboard.h"
#include "component/ui_utils.h"
#include <vector>
#include <algorithm>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEAdvertisedDevice.h>
#include <BLEScan.h>
#include <BLEClient.h>

#define CHAT_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHAT_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHAT_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#define CHAT_CHUNK   18
#define CHAT_MAX_MSG 200
#define CHAT_MAX_LOG 30
#define CHAT_CTRL    0x01    // non-printable framing header (never typed)

extern TFT_eSPI tft;

enum ChatScreen { CHAT_MAIN, CHAT_SCAN, CHAT_DEVICE, CHAT_ABOUT, CHAT_ROOM };
ChatScreen chatScreen = CHAT_MAIN;
int chatMenuSel = 0;
int chatSel = 0;
int chatScroll = 0;
String chatPeerName = "";
String chatPeerAddr = "";
bool chatCameFromScan = false;

struct ChatPeer { String name; String addr; int rssi; bool hasService; };
std::vector<ChatPeer> chatPeers;

struct ChatEntry { int kind; String text; };   // 0 = me, 1 = peer, 2 = system
std::vector<ChatEntry> chatLog;

// ---- BLE runtime state ----------------------------------------------------
static BLEServer*        sServer = nullptr;
static BLEService*       sService = nullptr;
static BLECharacteristic* sRx = nullptr;       // remote writes here
static BLECharacteristic* sTx = nullptr;       // we notify here
static BLEAdvertising*   sAdvert = nullptr;
static BLEClient*        sClient = nullptr;
static BLERemoteCharacteristic* sRemoteRx = nullptr;  // we write here
static BLERemoteCharacteristic* sRemoteTx = nullptr;  // we subscribe here
static bool sBleReady = false;
static bool sClientConnected = false;
static bool sServerHasClient = false;
static bool sConnecting = false;
static volatile bool sChatDirty = false;
static volatile bool sHasPending = false;
static String sRxBuf = "";      // multi-chunk assembly (BLE task only)
static volatile unsigned long sFrameTime = 0; // last frame arrival (BLE task)
static String sPendingMsg = ""; // complete incoming message handed to the loop

void chatAdd(int kind, const String& text) {
  if (chatLog.size() >= CHAT_MAX_LOG) chatLog.erase(chatLog.begin());
  chatLog.push_back({kind, text});
}

void chatToast(const String& msg) {
  const int w = 200, h = 38, x = (240 - w) / 2, y = 140;
  tft.fillRoundRect(x, y, w, h, 6, SymbianUI::BG);
  tft.drawRoundRect(x, y, w, h, 6, SymbianUI::ACCENT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.drawString(UiLayout::ellipsize(msg, 22), 120, y + h / 2, 2);
  delay(1100);
}

bool chatCanSend() { return sClientConnected || sServerHasClient; }

String chatStatusText() {
  if (sConnecting) return "Connecting...";
  if (sClientConnected) return "Connected";
  if (sServerHasClient) return "Peer connected";
  return "Disconnected";
}

uint16_t chatStatusColor() {
  if (sConnecting) return SymbianUI::ACCENT;
  if (sClientConnected || sServerHasClient) return SymbianUI::GOOD;
  return SymbianUI::BAD;
}

// ---- message framing (long messages only) -----------------------------------
// Short messages travel as raw text so a phone / laptop BLE UART app (nRF
// Connect, Serial Bluetooth Terminal, ...) reads them cleanly. Long messages
// are split into chunks prefixed by a non-printable header (0x01 + S/C/E) so
// they can never collide with normal typed text. Runs on the BLE event task;
// complete messages are staged in sPendingMsg and flushed by the UI loop.
void chatHandleData(uint8_t* pData, size_t length) {
  if (!pData || length == 0) return;

  // Framed chunk: 0x01 + type(M/S/C/E) + data
  if (length >= 3 && pData[0] == CHAT_CTRL) {
    char type = (char)pData[1];
    const uint8_t* d = pData + 2;
    size_t dl = length - 2;
    if (type == 'M') {
      sPendingMsg = "";
      for (size_t i = 0; i < dl; ++i) sPendingMsg += (char)d[i];
      sHasPending = true;
    } else if (type == 'S') {
      sRxBuf = "";
      for (size_t i = 0; i < dl; ++i) sRxBuf += (char)d[i];
      sFrameTime = millis();
    } else if (type == 'C') {
      if ((int)sRxBuf.length() < CHAT_MAX_MSG) {
        for (size_t i = 0; i < dl; ++i) sRxBuf += (char)d[i];
      }
      sFrameTime = millis();
    } else if (type == 'E') {
      for (size_t i = 0; i < dl; ++i) sRxBuf += (char)d[i];
      sPendingMsg = sRxBuf;
      sRxBuf = "";
      sHasPending = true;
    }
    return;
  }

  // Raw text from phones / laptops / BLE UART apps (and our own short sends).
  String msg = "";
  for (size_t i = 0; i < length; ++i) {
    char c = (char)pData[i];
    if (c == '\r') continue;
    msg += (c == '\n') ? ' ' : c;
  }
  msg.trim();
  if (!msg.length()) return;
  if ((int)msg.length() > CHAT_MAX_MSG) msg = msg.substring(0, CHAT_MAX_MSG);
  sPendingMsg = msg;
  sHasPending = true;
}

void chatSendMessage(const String& msg) {
  String m = msg;
  m.trim();
  if (!m.length()) return;
  if ((int)m.length() > CHAT_MAX_MSG) m = m.substring(0, CHAT_MAX_MSG);
  chatAdd(0, m);

  std::vector<std::string> frames;
  if ((int)m.length() <= CHAT_CHUNK) {
    // Raw text: readable in any BLE UART terminal app.
    frames.push_back(m.c_str());
  } else {
    std::string f = "\x01" "S";
    f += m.substring(0, CHAT_CHUNK).c_str();
    frames.push_back(f);
    int pos = CHAT_CHUNK;
    while ((int)m.length() - pos > CHAT_CHUNK) {
      std::string c = "\x01" "C";
      c += m.substring(pos, pos + CHAT_CHUNK).c_str();
      frames.push_back(c);
      pos += CHAT_CHUNK;
    }
    std::string e = "\x01" "E";
    e += m.substring(pos).c_str();
    frames.push_back(e);
  }

  for (const auto& f : frames) {
    if (sClientConnected && sRemoteRx) {
      sRemoteRx->writeValue(f, false);
    } else if (sServerHasClient && sTx) {
      sTx->setValue(f);
      sTx->notify();
    }
    delay(8);
  }
}

// ---- BLE callbacks ---------------------------------------------------------
class ChatServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    (void)pServer;
    sServerHasClient = true;
    sChatDirty = true;
    Serial.printf("[CHAT] BLE client CONNECTED (%s)\n",
                  pServer->getConnectedCount() > 0 ? "1+" : "0");
  }
  void onDisconnect(BLEServer* pServer) {
    (void)pServer;
    sServerHasClient = false;
    sChatDirty = true;
    Serial.printf("[CHAT] BLE client DISCONNECTED (%s)\n",
                  pServer->getConnectedCount() > 0 ? "1+" : "0");
  }
};

class ChatRxCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) {
    std::string v = c->getValue();
    Serial.printf("[CHAT] RX write %d bytes\n", (int)v.size());
    chatHandleData((uint8_t*)v.data(), v.size());
  }
};

class ChatClientCB : public BLEClientCallbacks {
  void onConnect(BLEClient* pClient) { (void)pClient; }
  void onDisconnect(BLEClient* pClient) {
    (void)pClient;
    sClientConnected = false;
    sChatDirty = true;
  }
};

// ---- BLE lifecycle ---------------------------------------------------------
void chatBleSetup() {
  if (!sBleReady) {
    BLEDevice::init("POCHITA-CHAT");
    BLEDevice::setMTU(247);
    sServer = BLEDevice::createServer();
    sServer->setCallbacks(new ChatServerCB());
    sService = sServer->createService(BLEUUID(CHAT_SERVICE_UUID));
    sRx = sService->createCharacteristic(BLEUUID(CHAT_RX_UUID),
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_WRITE_NR);
    sRx->setCallbacks(new ChatRxCB());
    sTx = sService->createCharacteristic(BLEUUID(CHAT_TX_UUID),
          BLECharacteristic::PROPERTY_NOTIFY);
    sTx->addDescriptor(new BLE2902());
    sService->start();
    sAdvert = BLEDevice::getAdvertising();
    sAdvert->addServiceUUID(BLEUUID(CHAT_SERVICE_UUID));
    sAdvert->setScanResponse(true);
    sAdvert->setMinPreferred(0x06);
    sAdvert->setMaxPreferred(0x12);
    sBleReady = true;
    Serial.println("[CHAT] BLE server ready, service 6e400001 started, advertising");
  }
  if (sAdvert) sAdvert->start();
}

void chatTeardown() {
  if (sAdvert) sAdvert->stop();
  if (sClientConnected && sClient) sClient->disconnect();
  sClientConnected = false;
  sServerHasClient = false;
  sConnecting = false;
  sChatDirty = false;
  sHasPending = false;
  sRxBuf = "";
  sFrameTime = 0;
  sPendingMsg = "";
}

// ---- scanning --------------------------------------------------------------
void drawChatScan();

void chatScanNow() {
  chatPeers.clear();
  chatSel = 0;
  chatScroll = 0;
  chatBleSetup();
  String own = String(BLEDevice::getAddress().toString().c_str());

  SymbianUI::drawMessageScreen("BLE Chat", SymbianUI::ICON_SEARCH,
                               "Scanning", "Looking for chat peers", "", "Cancel");
  SymbianUI::drawProgressBar(35, 188, 170, 20);
  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  SymbianUI::drawProgressBar(35, 188, 170, 60);
  BLEScanResults results = scan->start(4, false);
  SymbianUI::drawProgressBar(35, 188, 170, 100);

  int cnt = results.getCount();
  for (int i = 0; i < cnt; ++i) {
    BLEAdvertisedDevice d = results.getDevice(i);
    String addr = String(d.getAddress().toString().c_str());
    if (addr == own) continue;
    ChatPeer p;
    p.name = d.haveName() ? String(d.getName().c_str()) : "(no name)";
    p.addr = addr;
    p.rssi = d.getRSSI();
    p.hasService = d.isAdvertisingService(BLEUUID(CHAT_SERVICE_UUID));
    chatPeers.push_back(p);
  }
  scan->clearResults();

  std::sort(chatPeers.begin(), chatPeers.end(),
            [](const ChatPeer& a, const ChatPeer& b) { return a.rssi > b.rssi; });

  chatScreen = CHAT_SCAN;
  drawChatScan();
}

// ---- connecting ------------------------------------------------------------
void drawChatRoom();

void chatConnectTo(int idx) {
  if (idx < 0 || idx >= (int)chatPeers.size()) return;
  ChatPeer& peer = chatPeers[idx];
  chatPeerName = peer.name;
  chatPeerAddr = peer.addr;
  chatLog.clear();
  chatCameFromScan = true;
  sConnecting = true;
  sClientConnected = false;
  chatScreen = CHAT_ROOM;
  drawChatRoom();
  chatBleSetup();

  if (!sClient) {
    sClient = BLEDevice::createClient();
    sClient->setClientCallbacks(new ChatClientCB());
  }
  sClient->setMTU(247);
  bool ok = sClient->connect(BLEAddress(chatPeerAddr.c_str()));
  sConnecting = false;
  if (!ok) {
    chatToast("Connect failed");
    chatScreen = CHAT_SCAN;
    drawChatScan();
    return;
  }

  BLERemoteService* svc = sClient->getService(BLEUUID(CHAT_SERVICE_UUID));
  if (!svc) {
    sClient->disconnect();
    chatToast("Not a chat device");
    chatScreen = CHAT_SCAN;
    drawChatScan();
    return;
  }
  sRemoteRx = svc->getCharacteristic(BLEUUID(CHAT_RX_UUID));
  sRemoteTx = svc->getCharacteristic(BLEUUID(CHAT_TX_UUID));
  if (!sRemoteRx || !sRemoteTx) {
    sClient->disconnect();
    chatToast("Not a chat device");
    chatScreen = CHAT_SCAN;
    drawChatScan();
    return;
  }

  sRemoteTx->registerForNotify([](BLERemoteCharacteristic* c, uint8_t* pData,
                                  size_t length, bool isNotify) {
    (void)c;
    (void)isNotify;
    chatHandleData(pData, length);
  });
  sClientConnected = true;
  chatAdd(2, "Connected to " + chatPeerName);
  drawChatRoom();
}

// ---- UI drawing ------------------------------------------------------------
void drawChatMain() {
  SymbianUI::drawChrome("BLE Chat", "Select", "Back");
  const char* items[] = {"Scan for peers", "My device", "About"};
  const SymbianUI::Icon icons[] = {SymbianUI::ICON_SEARCH,
      SymbianUI::ICON_BLUETOOTH, SymbianUI::ICON_INFO};
  for (int i = 0; i < 3; ++i) {
    String value = ">";
    if (i == 0)
      value = chatPeers.empty() ? ">" : String(chatPeers.size()) + " found";
    else if (i == 1)
      value = "POCHITA";
    SymbianUI::drawListRow(58 + i * 42, 36, items[i], i == chatMenuSel, value,
                           icons[i]);
  }
  SymbianUI::drawSectionLabel(196, "BLE Chat");
  SymbianUI::drawInfoLine(226, "Protocol", "NUS UART");
  SymbianUI::drawInfoLine(248, "Peers", "ESP32 / phone / PC");
  SymbianUI::drawInfoLine(270, "State", chatStatusText());
  SymbianUI::drawSoftkeys("Select", "Back");
}

void drawChatScan() {
  SymbianUI::drawChrome("Peers", "Connect", "Back");
  if (chatPeers.empty()) {
    SymbianUI::drawEmptyState(SymbianUI::ICON_BLUETOOTH, "No peers found",
                              "Press Option to rescan");
    SymbianUI::drawSoftkeys("Rescan", "Back");
    return;
  }
  const int itemH = 42, gap = 2, visible = 5;
  for (int i = 0; i < visible; ++i) {
    int idx = chatScroll + i;
    if (idx >= (int)chatPeers.size()) break;
    int y = 54 + i * (itemH + gap);
    bool sel = (idx == chatSel);
    String label = UiLayout::ellipsize(chatPeers[idx].name, 20);
    String value = String(chatPeers[idx].rssi) + " dBm";
    SymbianUI::drawListRow(y, itemH, label, sel, value,
                           SymbianUI::ICON_BLUETOOTH);
    String detail = chatPeers[idx].addr;
    if (chatPeers[idx].hasService) detail += "  [chat]";
    tft.setTextColor(sel ? TFT_WHITE : SymbianUI::DIM,
                     sel ? SymbianUI::SELECT : SymbianUI::BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(UiLayout::ellipsize(detail, 36), 40, y + 30, 1);
  }
  tft.setTextColor(TFT_SILVER, SymbianUI::BG);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(String(chatPeers.size()) + " peers", 232, 286, 1);
  SymbianUI::drawSoftkeys("Connect", "Back");
}

void drawChatDevice() {
  SymbianUI::drawChrome("My device", "", "Back");
  SymbianUI::drawSectionLabel(58, "Identity");
  SymbianUI::drawInfoLine(88, "Name", "POCHITA-CHAT");
  String addr = sBleReady ? String(BLEDevice::getAddress().toString().c_str())
                          : "-";
  SymbianUI::drawInfoLine(110, "MAC", addr);
  SymbianUI::drawSectionLabel(150, "Chat server");
  SymbianUI::drawInfoLine(180, "Service", "NUS");
  SymbianUI::drawInfoLine(202, "Advertising", sAdvert ? "On" : "Off");
  SymbianUI::drawInfoLine(224, "Peer", sServerHasClient ? "Connected" : "Waiting");
}

void drawChatAbout() {
  SymbianUI::drawChrome("About Chat", "", "Back");
  SymbianUI::drawEmptyState(SymbianUI::ICON_BLUETOOTH, "P2P BLE Chat",
                            "Talk to ESP32, phone or laptop");
  SymbianUI::drawSectionLabel(196, "Compatible peers");
  SymbianUI::drawInfoLine(226, "Protocol", "NUS GATT");
  SymbianUI::drawInfoLine(248, "Phone apps", "nRF Connect / UART");
  SymbianUI::drawInfoLine(270, "Laptops", "ble-serial / Python");
}

void chatWrap(const String& text, int maxChars, std::vector<String>& out) {
  String rem = text;
  rem.trim();
  while (rem.length() > 0) {
    if ((int)rem.length() <= maxChars) {
      out.push_back(rem);
      break;
    }
    int cut = maxChars;
    int sp = rem.lastIndexOf(' ', cut);
    if (sp > maxChars / 2) cut = sp;
    out.push_back(rem.substring(0, cut));
    rem = rem.substring(cut);
    rem.trim();
  }
}

void drawChatRoom() {
  String title = "Chat";
  if (chatPeerName.length()) title = "Chat: " + chatPeerName;
  SymbianUI::drawChrome(title, chatCanSend() ? "Write" : "", "Back");

  tft.fillRect(0, 26, 240, 16, SymbianUI::BG);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(chatStatusColor(), SymbianUI::BG);
  tft.drawString(chatStatusText(), 6, 33, 1);
  tft.drawFastHLine(0, 42, 240, SymbianUI::DIVIDER);

  std::vector<String> lines;
  std::vector<int> kinds;
  for (const auto& e : chatLog) {
    String prefix;
    if (e.kind == 0) prefix = "You: ";
    else if (e.kind == 1) prefix = "Peer: ";
    std::vector<String> w;
    chatWrap(prefix + e.text, 27, w);
    for (const auto& ln : w) {
      lines.push_back(ln);
      kinds.push_back(e.kind);
    }
  }

  int total = (int)lines.size();
  int maxLines = 14;
  int start = (total - maxLines < 0) ? 0 : total - maxLines;
  int y = 48;
  for (int i = start; i < total; ++i) {
    if (y > 286) break;
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(kinds[i] == 2 ? SymbianUI::ACCENT : SymbianUI::FG,
                     SymbianUI::BG);
    tft.drawString(lines[i], 6, y, 2);
    y += 16;
  }
  SymbianUI::drawSoftkeys(chatCanSend() ? "Write" : "", "Back");
}

// ---- app entry / loop ------------------------------------------------------
void initChatApp() {
  chatBleSetup();
  chatScreen = CHAT_MAIN;
  chatMenuSel = 0;
  chatLog.clear();
  sChatDirty = false;
  drawChatMain();
}

void loopChat() {
  // Deliver a long-message assembly that never completed (dropped frame or a
  // text line whose first byte merely looked like a frame header).
  if (sRxBuf.length() && millis() - sFrameTime > 600) {
    sPendingMsg = sRxBuf;
    sRxBuf = "";
    sHasPending = true;
  }

  // Flush messages delivered by the BLE event task into the chat log.
  if (sHasPending) {
    sHasPending = false;
    chatAdd(1, sPendingMsg);
    sPendingMsg = "";
    sChatDirty = true;
  }

  // A peer connected to our chat server while we were idle: hop into the room.
  if (sServerHasClient && sChatDirty &&
      (chatScreen == CHAT_MAIN || chatScreen == CHAT_SCAN)) {
    chatPeerName = "peer";
    chatScreen = CHAT_ROOM;
    drawChatRoom();
    sChatDirty = false;
  }

  if (chatScreen == CHAT_ROOM) {
    if (sChatDirty) {
      sChatDirty = false;
      drawChatRoom();
    }
    if (isBackPressed()) {
      if (sClientConnected && sClient) sClient->disconnect();
      sClientConnected = false;
      if (chatCameFromScan) {
        chatScreen = CHAT_SCAN;
        drawChatScan();
      } else {
        chatScreen = CHAT_MAIN;
        drawChatMain();
      }
      delay(180);
      return;
    }
    if (isSelectPressed()) {
      if (!chatCanSend()) {
        chatToast("Not connected");
        drawChatRoom();
        return;
      }
      String msg = "";
      keyboard.begin();
      keyboard.active = true;
      keyboard.draw(true);
      bool done = false;
      while (!done) {
        int r = keyboard.handleInput(msg);
        if (r == 1) {
          done = true;
          String t = msg;
          t.trim();
          if (t.length()) chatSendMessage(t);
        } else if (r == 2) {
          done = true;
        }
        delay(10);
      }
      keyboard.active = false;
      drawChatRoom();
      return;
    }
    delay(16);
    return;
  }

  if (chatScreen == CHAT_SCAN) {
    if (isBackPressed()) {
      chatScreen = CHAT_MAIN;
      drawChatMain();
      delay(180);
      return;
    }
    if (digitalRead(KEY_OPTION) == LOW) {
      chatScanNow();
      delay(200);
      return;
    }
    if (chatPeers.empty()) {
      delay(16);
      return;
    }
    if (digitalRead(KEY_DOWN) == LOW) {
      if (chatSel < (int)chatPeers.size() - 1) {
        chatSel++;
        if (chatSel >= chatScroll + 5) chatScroll++;
        drawChatScan();
      }
      delay(150);
      return;
    }
    if (digitalRead(KEY_UP) == LOW) {
      if (chatSel > 0) {
        chatSel--;
        if (chatSel < chatScroll) chatScroll--;
        drawChatScan();
      }
      delay(150);
      return;
    }
    if (isSelectPressed()) {
      chatConnectTo(chatSel);
      delay(180);
      return;
    }
    delay(16);
    return;
  }

  if (chatScreen == CHAT_DEVICE || chatScreen == CHAT_ABOUT) {
    if (isBackPressed()) {
      chatScreen = CHAT_MAIN;
      drawChatMain();
      delay(180);
    }
    delay(16);
    return;
  }

  // CHAT_MAIN
  if (isBackPressed()) {
    chatTeardown();
    extern SystemMode currentMode;
    extern void drawLauncherContent();
    currentMode = MODE_LAUNCHER;
    drawLauncherContent();
    return;
  }
  if (digitalRead(KEY_DOWN) == LOW) {
    chatMenuSel = (chatMenuSel + 1) % 3;
    drawChatMain();
    delay(150);
    return;
  }
  if (digitalRead(KEY_UP) == LOW) {
    chatMenuSel = (chatMenuSel + 2) % 3;
    drawChatMain();
    delay(150);
    return;
  }
  if (isSelectPressed()) {
    if (chatMenuSel == 0) {
      chatScanNow();
    } else if (chatMenuSel == 1) {
      chatScreen = CHAT_DEVICE;
      drawChatDevice();
    } else {
      chatScreen = CHAT_ABOUT;
      drawChatAbout();
    }
    delay(180);
    return;
  }
  delay(16);
}

#endif
