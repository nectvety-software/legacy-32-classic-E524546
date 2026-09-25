#pragma once
#include "WiFiClient.h"
class WiFiClientSecure: public WiFiClient { public: const char *trustedPem=nullptr; void setInsecure() = delete; void setCACert(const char *pem){ trustedPem=pem; } void setHandshakeTimeout(int){} };
