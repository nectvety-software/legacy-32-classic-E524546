#ifndef WEB_H
#define WEB_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

////////////// WiFi Config //////////////
#ifndef WIFI_SSID
#define WIFI_SSID "YourWiFi"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "YourPassword"
#endif

////////////// Web State //////////////
enum WebState { WEB_MAIN, WEB_WIFI, WEB_URL, WEB_VIEW, WEB_API };
enum ApiMethod { API_GET, API_POST, API_PUT, API_DELETE };

////////////// Web Global Variables //////////////
extern WebState webState;
extern int webCursor;
extern bool wifiConnected;
extern String wifiSSID;
extern String wifiPass;
extern String currentURL;
extern String lastResponse;
extern int scrollOffset;
extern String apiEndpoint;
extern String apiBody;
extern ApiMethod apiMethod;
extern String apiResponse;

////////////// WiFi Functions //////////////
void webInitWiFi();
bool webConnectWiFi(const String& ssid, const String& pass);
void webDisconnectWiFi();
bool webIsConnected();

////////////// HTTP Functions //////////////
String webHTTPGet(const String& url);
String webHTTPPost(const String& url, const String& body);
String webHTTPRequest(const String& url, const String& method, const String& body);

////////////// Web UI Functions //////////////
void webDrawMain();
void webDrawWiFi();
void webDrawURLInput();
void webDrawView();
void webDrawAPI();
void webInputHandler();
void appWeb();

////////////// Utility Functions //////////////
String webExtractDomain(const String& url);
String webFormatJSON(const String& json);
void webClearResponse();

#endif
