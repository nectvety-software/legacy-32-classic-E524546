#include "web.h"
#include "modbox_main.h"
#include <algorithm>

////////////// Web Global Variables //////////////
WebState webState = WEB_MAIN;
int webCursor = 0;
String wifiSSID = "";
String wifiPass = "";
String currentURL = "https://httpbin.org/get";
String lastResponse = "";
int scrollOffset = 0;
String apiEndpoint = "https://httpbin.org";
String apiBody = "{\"test\": true}";
ApiMethod apiMethod = API_GET;
String apiResponse = "";

static int urlInputCursor = 0;
static bool wifiConnecting = false;
static unsigned long wifiConnectStart = 0;
static int apiCursor = 0;
static bool loading = false;

////////////// WiFi Functions //////////////
void webInitWiFi() {
    wifiConnected = WiFi.status() == WL_CONNECTED;
}

bool webConnectWiFi(const String& ssid, const String& pass) {
    wifiConnecting = true;
    wifiConnectStart = millis();
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
        if (attempts % 2 == 0) {
            gfx->setTextColor(COLOR_GRAY);
            gfx->setCursor(20, 150 + (attempts % 4) * 15);
            gfx->print(".");
        }
    }
    
    wifiConnecting = false;
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        wifiSSID = ssid;
        return true;
    }
    
    wifiConnected = false;
    return false;
}

void webDisconnectWiFi() {
    WiFi.disconnect();
    wifiConnected = false;
    wifiSSID = "";
}

bool webIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

////////////// HTTP Functions //////////////
String webHTTPGet(const String& url) {
    if (!webIsConnected()) return "ERROR: WiFi not connected";
    
    HTTPClient http;
    http.setTimeout(10000);
    
    if (http.begin(url)) {
        int httpCode = http.GET();
        
        if (httpCode > 0) {
            String payload = http.getString();
            http.end();
            return payload;
        } else {
            String error = "ERROR: " + http.errorToString(httpCode);
            http.end();
            return error;
        }
    }
    
    http.end();
    return "ERROR: Failed to connect";
}

String webHTTPPost(const String& url, const String& body) {
    if (!webIsConnected()) return "ERROR: WiFi not connected";
    
    HTTPClient http;
    http.setTimeout(10000);
    
    if (http.begin(url)) {
        http.addHeader("Content-Type", "application/json");
        int httpCode = http.POST(body);
        
        if (httpCode > 0) {
            String payload = http.getString();
            http.end();
            return payload;
        } else {
            String error = "ERROR: " + http.errorToString(httpCode);
            http.end();
            return error;
        }
    }
    
    http.end();
    return "ERROR: Failed to connect";
}

String webHTTPRequest(const String& url, const String& method, const String& body) {
    if (!webIsConnected()) return "ERROR: WiFi not connected";
    
    HTTPClient http;
    http.setTimeout(10000);
    
    if (!http.begin(url)) return "ERROR: Failed to connect";
    
    int httpCode = -1;
    
    if (method == "GET") {
        httpCode = http.GET();
    } else if (method == "POST") {
        http.addHeader("Content-Type", "application/json");
        httpCode = http.POST(body);
    } else if (method == "PUT") {
        http.addHeader("Content-Type", "application/json");
        httpCode = http.PUT(body);
    } else if (method == "DELETE") {
        httpCode = http.sendRequest("DELETE", body);
    }
    
    if (httpCode > 0) {
        String payload = http.getString();
        http.end();
        return payload;
    } else {
        String error = "ERROR: " + http.errorToString(httpCode);
        http.end();
        return error;
    }
}

////////////// Utility Functions //////////////
String webExtractDomain(const String& url) {
    String temp = url;
    temp.replace("https://", "");
    temp.replace("http://", "");
    int slash = temp.indexOf('/');
    if (slash > 0) {
        return temp.substring(0, slash);
    }
    return temp;
}

String webFormatJSON(const String& json) {
    String result = "";
    int indent = 0;
    bool inString = false;
    
    for (int i = 0; i < json.length(); i++) {
        char c = json.charAt(i);
        
        if (c == '"' && (i == 0 || json.charAt(i-1) != '\\')) {
            inString = !inString;
            result += c;
        } else if (!inString) {
            if (c == '{' || c == '[') {
                result += c;
                result += '\n';
                indent++;
                for (int j = 0; j < indent; j++) result += "  ";
            } else if (c == '}' || c == ']') {
                result += '\n';
                indent--;
                for (int j = 0; j < indent; j++) result += "  ";
                result += c;
            } else if (c == ',') {
                result += c;
                result += '\n';
                for (int j = 0; j < indent; j++) result += "  ";
            } else if (c == ':') {
                result += ": ";
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                result += c;
            }
        } else {
            result += c;
        }
    }
    
    return result;
}

void webClearResponse() {
    lastResponse = "";
    scrollOffset = 0;
}

////////////// Web UI Drawing //////////////
void webDrawMain() {
    webState = WEB_MAIN;
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 8);
    gfx->print("Trinh duyet Web");
    
    const char* items[] = {"WiFi", "Nhap URL", "Xem Web", "API Test"};
    for (int i = 0; i < 4; i++) {
        int y = 50 + i * 40;
        if (i == webCursor) {
            gfx->fillRect(10, y, 220, 32, COLOR_BLUE);
            gfx->setTextColor(COLOR_WHITE);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        gfx->setCursor(20, y + 10);
        gfx->print(items[i]);
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 280);
    gfx->print("OK:Chon  BACK:Thoat");
}

void webDrawWiFi() {
    webState = WEB_WIFI;
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 8);
    gfx->print("Cai dat WiFi");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 35);
    gfx->print("Trang thai:");
    
    if (wifiConnected) {
        gfx->setTextColor(COLOR_GREEN);
        gfx->setCursor(10, 50);
        gfx->print("Da ket noi!");
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 70);
        gfx->print("SSID:");
        gfx->print(wifiSSID);
        gfx->setCursor(10, 85);
        gfx->print("IP:");
        gfx->print(WiFi.localIP().toString());
        
        gfx->fillRect(10, 120, 220, 30, COLOR_RED);
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(30, 128);
        gfx->print("Ngat ket noi");
    } else {
        gfx->setTextColor(COLOR_RED);
        gfx->setCursor(10, 50);
        gfx->print("Chua ket noi");
        
        gfx->fillRect(10, 80, 220, 30, COLOR_GREEN);
        gfx->setTextColor(COLOR_BG);
        gfx->setCursor(50, 88);
        gfx->print("Ket noi WiFi");
        
        gfx->fillRect(10, 120, 220, 30, COLOR_YELLOW);
        gfx->setTextColor(COLOR_BG);
        gfx->setCursor(30, 128);
        gfx->print("Nhap SSID/PASS");
    }
    
    if (wifiConnecting) {
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(10, 170);
        gfx->print("Dang ket noi...");
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 280);
    gfx->print("A:OK  B:Thoat");
}

void webDrawURLInput() {
    webState = WEB_URL;
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 8);
    gfx->print("Nhap URL");
    
    gfx->fillRect(10, 30, 220, 35, COLOR_GRAY);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(15, 38);
    if (currentURL.length() > 0) {
        String display = currentURL.substring(0, std::min(30, (int)currentURL.length()));
        gfx->print(display);
    }
    int cursorX = 15 + std::min(30, (int)currentURL.length()) * 6;
    gfx->fillRect(cursorX, 38, 8, 16, COLOR_WHITE);
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 80);
    gfx->print("Nut A: Nhap ky tu");
    gfx->setCursor(10, 60);
    gfx->print("Nut B: Xoa ky tu");
    gfx->setCursor(10, 80);
    gfx->print("OK: Gui yeu cau");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 280);
    gfx->print("A:Ky tu  B:Xoa  OK:GET  MENU:Thoat");
}

void webDrawView() {
    webState = WEB_VIEW;
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 8);
    
    String title = currentURL.length() > 25 ? currentURL.substring(0, 25) + "..." : currentURL;
    gfx->print(title);
    
    if (loading) {
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(150, 8);
        gfx->print("...");
    }
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 28);
    gfx->print("--- Response ---");
    
    if (lastResponse.length() == 0) {
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(20, 50);
        gfx->print("Chua co du lieu");
        gfx->setCursor(20, 70);
        gfx->print("Nhan OK de tai");
    } else {
        int y = 45;
        int maxLines = 11;
        int lineHeight = 14;
        int startPos = scrollOffset * 40;
        
        for (int i = 0; i < maxLines && startPos + i * 40 < lastResponse.length(); i++) {
            int lineEnd = startPos + (i + 1) * 40;
            if (lineEnd > lastResponse.length()) lineEnd = lastResponse.length();
            
            String line = lastResponse.substring(startPos + i * 40, lineEnd);
            line.replace("\n", " ");
            line.replace("\r", "");
            
            if (line.length() > 0) {
                gfx->setTextColor(COLOR_WHITE);
                gfx->setCursor(10, y + i * lineHeight);
                
                if (line.length() > 38) {
                    gfx->print(line.substring(0, 38));
                } else {
                    gfx->print(line);
                }
            }
        }
    }
    
    int totalLines = lastResponse.length() / 40;
    int scrollBarH = max(10, 200 / max(1, totalLines + 1));
    int scrollBarY = 30 + (scrollOffset * 200) / max(1, totalLines);
    
    gfx->fillRect(230, 30, 6, 200, COLOR_GRAY);
    gfx->fillRect(230, scrollBarY, 6, scrollBarH, COLOR_BLUE);
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 280);
    gfx->print("UP/DOWN:Cuon  OK:Tai lai  A:URL  B:Thoat");
}

void webDrawAPI() {
    webState = WEB_API;
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 8);
    gfx->print("Kiem tra REST API");
    
    const char* methods[] = {"GET", "POST", "PUT", "DELETE"};
    int colors[] = {COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE, COLOR_RED};
    
    for (int i = 0; i < 4; i++) {
        int x = 10 + i * 55;
        if (i == apiCursor) {
            gfx->fillRect(x, 30, 50, 20, colors[i]);
            gfx->setTextColor(COLOR_BG);
        } else {
            gfx->drawRect(x, 30, 50, 20, colors[i]);
            gfx->setTextColor(colors[i]);
        }
        gfx->setCursor(x + 5, 35);
        gfx->print(methods[i]);
    }
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 60);
    gfx->print("Diem cuoi:");
    
    gfx->fillRect(10, 75, 220, 25, COLOR_GRAY);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(15, 80);
    String endDisplay = apiEndpoint.length() > 35 ? apiEndpoint.substring(0, 35) : apiEndpoint;
    gfx->print(endDisplay);
    
    gfx->setCursor(10, 110);
    gfx->print("Noi dung (POST/PUT):");
    
    gfx->fillRect(10, 125, 220, 50, COLOR_GRAY);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(15, 130);
    String bodyDisplay = apiBody.length() > 35 ? apiBody.substring(0, 35) : apiBody;
    gfx->print(bodyDisplay);
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 185);
    gfx->print("A:Gui  LEFT/RIGHT:Phuong thuc");
    
    if (apiResponse.length() > 0) {
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 205);
        gfx->print("Phan hoi:");
        
        int maxShow = 7;
        String respLine = apiResponse.length() > 100 ? apiResponse.substring(0, 100) + "..." : apiResponse;
        respLine.replace("\n", " ");
        
        int y = 220;
        for (int i = 0; i < maxShow && i * 38 < respLine.length(); i++) {
            int start = i * 38;
            int end = std::min(start + 38, (int)respLine.length());
            String line = respLine.substring(start, end);
            gfx->setCursor(15, y + i * 12);
            gfx->print(line);
        }
    }
    
    if (loading) {
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(100, 280);
        gfx->print("Dang gui...");
    }
}

////////////// Web Input Handler //////////////
void webInputHandler() {
    switch (webState) {
        case WEB_MAIN:
            if (buttonPressed(KEY_UP)) {
                webCursor = (webCursor + 3) % 4;
                webDrawMain();
            }
            if (buttonPressed(KEY_DOWN)) {
                webCursor = (webCursor + 1) % 4;
                webDrawMain();
            }
            if (buttonPressed(KEY_START)) {
                switch (webCursor) {
                    case 0: webDrawWiFi(); break;
                    case 1: urlInputCursor = currentURL.length(); webDrawURLInput(); break;
                    case 2: 
                        if (wifiConnected) {
                            loading = true;
                            webDrawView();
                            lastResponse = webHTTPGet(currentURL);
                            scrollOffset = 0;
                            loading = false;
                            webDrawView();
                        } else {
                            webDrawWiFi();
                        }
                        break;
                    case 3: 
                        if (wifiConnected) {
                            apiResponse = "";
                            webDrawAPI();
                        } else {
                            webDrawWiFi();
                        }
                        break;
                }
            }
            if (buttonPressed(KEY_A)) {
                systemState = SYS_MAIN;
                drawMainUI();
            }
            break;
            
        case WEB_WIFI:
            if (buttonPressed(KEY_UP) || buttonPressed(KEY_DOWN)) {
                webDrawWiFi();
            }
            if (buttonPressed(KEY_START)) {
                if (wifiConnected) {
                    webDisconnectWiFi();
                } else {
                    wifiSSID = "MyWiFi";
                    wifiPass = "password";
                    if (webConnectWiFi(wifiSSID, wifiPass)) {
                        tone(BUZZER, 2000, 100);
                        delay(100);
                        tone(BUZZER, 2500, 150);
                    } else {
                        tone(BUZZER, 500, 200);
                        delay(200);
                        tone(BUZZER, 300, 200);
                    }
                }
                webDrawWiFi();
            }
            if (buttonPressed(KEY_A)) {
                webCursor = 0;
                webDrawMain();
            }
            break;
            
        case WEB_URL:
            if (buttonPressed(KEY_UP)) {
                currentURL = "";
                webDrawURLInput();
            }
            if (buttonPressed(KEY_DOWN)) {
                currentURL = "https://example.com";
                webDrawURLInput();
            }
            if (buttonPressed(KEY_LEFT)) {
                if (currentURL.length() > 0) {
                    currentURL.remove(currentURL.length() - 1);
                    webDrawURLInput();
                }
            }
            if (buttonPressed(KEY_RIGHT)) {
                currentURL += " ";
                webDrawURLInput();
            }
            if (buttonPressed(KEY_START)) {
                currentURL += "a";
                webDrawURLInput();
            }
            if (buttonPressed(KEY_START)) {
                if (wifiConnected && currentURL.length() > 0) {
                    loading = true;
                    webDrawView();
                    lastResponse = webHTTPGet(currentURL);
                    scrollOffset = 0;
                    loading = false;
                    webDrawView();
                }
            }
            if (buttonPressed(KEY_A)) {
                webCursor = 1;
                webDrawMain();
            }
            if (buttonPressed(KEY_MENU)) {
                webCursor = 0;
                webDrawMain();
            }
            break;
            
        case WEB_VIEW:
            if (buttonPressed(KEY_UP)) {
                if (scrollOffset > 0) scrollOffset--;
                webDrawView();
            }
            if (buttonPressed(KEY_DOWN)) {
                int maxScroll = std::max(0, (int)(lastResponse.length() / 40) - 10);
                if (scrollOffset < maxScroll) scrollOffset++;
                webDrawView();
            }
            if (buttonPressed(KEY_START)) {
                urlInputCursor = currentURL.length();
                webDrawURLInput();
            }
            if (buttonPressed(KEY_START)) {
                if (wifiConnected) {
                    loading = true;
                    webDrawView();
                    lastResponse = webHTTPGet(currentURL);
                    scrollOffset = 0;
                    loading = false;
                    webDrawView();
                }
            }
            if (buttonPressed(KEY_A)) {
                webCursor = 2;
                webDrawMain();
            }
            break;
            
        case WEB_API:
            if (buttonPressed(KEY_LEFT)) {
                apiCursor = (apiCursor + 3) % 4;
                webDrawAPI();
            }
            if (buttonPressed(KEY_RIGHT)) {
                apiCursor = (apiCursor + 1) % 4;
                webDrawAPI();
            }
            if (buttonPressed(KEY_UP)) {
                apiEndpoint = "";
                webDrawAPI();
            }
            if (buttonPressed(KEY_DOWN)) {
                apiEndpoint = "https://httpbin.org";
                webDrawAPI();
            }
            if (buttonPressed(KEY_START)) {
                if (wifiConnected) {
                    const char* methods[] = {"GET", "POST", "PUT", "DELETE"};
                    apiMethod = (ApiMethod)apiCursor;
                    
                    loading = true;
                    webDrawAPI();
                    
                    if (apiMethod == API_GET) {
                        apiResponse = webHTTPRequest(apiEndpoint, "GET", "");
                    } else if (apiMethod == API_POST) {
                        apiResponse = webHTTPRequest(apiEndpoint, "POST", apiBody);
                    } else if (apiMethod == API_PUT) {
                        apiResponse = webHTTPRequest(apiEndpoint, "PUT", apiBody);
                    } else if (apiMethod == API_DELETE) {
                        apiResponse = webHTTPRequest(apiEndpoint, "DELETE", "");
                    }
                    
                    loading = false;
                    webDrawAPI();
                    
                    tone(BUZZER, 2000, 50);
                }
            }
            if (buttonPressed(KEY_A)) {
                webCursor = 3;
                webDrawMain();
            }
            break;
    }
}

void appWeb() {
    webState = WEB_MAIN;
    webCursor = 0;
    scrollOffset = 0;
    apiCursor = 0;
    loading = false;
    webInitWiFi();
    webDrawMain();
    
    while (true) {
        webInputHandler();
        delay(50);
        if (systemState == SYS_MAIN) {
            break;
        }
    }
}
