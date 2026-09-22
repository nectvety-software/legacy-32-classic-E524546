#pragma once
#include "../sim_arduino.h"
// Simulator shim: chi de test logic HTTPS/redirect trong che do --mock.
// --live van la raw Winsock TCP, khong co Schannel/TLS.
struct WiFiClientSecure : public WiFiClient {
  void setInsecure() {}
  void setHandshakeTimeout(unsigned long) {}
  bool connect(const char *host, int port) { return WiFiClient::connect(host, port, 8000); }
};
