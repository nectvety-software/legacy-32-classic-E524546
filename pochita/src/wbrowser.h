#ifndef WBROWSER_H
#define WBROWSER_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <FS.h>
#include <SD.h>
#include "component/Display.h"
#include "component/Config.h"
#include "component/Keyboard.h"
#include "component/ui_utils.h"

extern TFT_eSPI tft;

enum BrowserState { BROWSER_MAIN, BROWSER_LOADING, BROWSER_VIEW, BROWSER_INPUT, BROWSER_NAVIGATE };
BrowserState browserState = BROWSER_MAIN;

String currentURL = "";
String pageContent = "";
int scrollY = 0;
String inputURL = "";
bool pageLoaded = false;
int browserMenuSel = 0;
int selectedLink = 0;
int pointerX = 10;
int pointerY = 50;
int pageScroll = 0;

bool isLocalHTML(const String &url) {
    String lower = url;
    lower.toLowerCase();
    return lower.startsWith("file://") || lower.endsWith(".html") || lower.endsWith(".htm");
}

String localFilePath(const String &url) {
    String path = url;
    if (path.startsWith("file:///")) {
        path = path.substring(7);
    } else if (path.startsWith("file://")) {
        path = path.substring(7);
    }
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    return path;
}

String resolveRelativeURL(const String &baseURL, const String &relativeURL) {
    if (relativeURL.startsWith("http://") || relativeURL.startsWith("https://") || 
        relativeURL.startsWith("file://") || relativeURL.startsWith("/")) {
        return relativeURL;
    }
    
    String base = baseURL;
    if (base.startsWith("file://")) {
        int lastSlash = base.lastIndexOf("/");
        if (lastSlash != -1) {
            return base.substring(0, lastSlash + 1) + relativeURL;
        }
        return "file://" + relativeURL;
    }
    
    if (base.startsWith("http://") || base.startsWith("https://")) {
        int lastSlash = base.lastIndexOf("/");
        if (lastSlash != -1) {
            return base.substring(0, lastSlash + 1) + relativeURL;
        }
    }
    
    return relativeURL;
}

String extractURL(const String &baseURL, const String &href) {
    String url = href;
    while (url.length() > 0 && (url.charAt(0) == '"' || url.charAt(0) == '\'')) {
        url = url.substring(1);
    }
    while (url.length() > 0 && (url.charAt(url.length()-1) == '"' || url.charAt(url.length()-1) == '\'')) {
        url = url.substring(0, url.length()-1);
    }
    
    if (!url.startsWith("http://") && !url.startsWith("https://") && !url.startsWith("file://")) {
        url = resolveRelativeURL(baseURL, url);
    }
    return url;
}

void viewPageContent();

int linkCount = 0;
std::vector<String> linkURLs;
std::vector<String> pageLines;

bool pointerOverLink() {
    int linkAreaY = 232;
    int visibleLinks = min(linkCount, 3);
    if (pointerY < linkAreaY || pointerY > linkAreaY + visibleLinks * 16 + 4) return false;
    int index = (pointerY - linkAreaY - 2) / 16;
    return index >= 0 && index < visibleLinks && index < linkCount;
}

int linkIndexAtPointer() {
    int linkAreaY = 232;
    int index = (pointerY - linkAreaY - 2) / 16;
    if (index < 0) return -1;
    if (index >= linkCount) return -1;
    return index;
}

void drawPointer(bool handCursor = false) {
    tft.setTextDatum(MC_DATUM);
    if (handCursor) {
        tft.setTextColor(TFT_YELLOW);
        tft.drawString("🖐", pointerX, pointerY);
    } else {
        tft.setTextColor(TFT_CYAN);
        tft.drawString(">", pointerX, pointerY);
    }
}

void drawBrowserUI() {
    SymbianUI::drawChrome("Browser", "Open", "Back");

    // Address/Search bar
    tft.fillRect(5, 57, 230, 25, TFT_BLACK);
    tft.drawRect(5, 57, 230, 25, SymbianUI::ACCENT);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(ML_DATUM);
    String disp = currentURL;
    if (disp.length() == 0) disp = "http://";
    if (disp.length() > 28) disp = disp.substring(0, 25) + "...";
    tft.drawString(disp, 11, 69, 1);
    tft.drawFastVLine(205, 58, 23, SymbianUI::DIVIDER);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Go", 220, 69, 1);
}

void drawBrowserMain() {
    drawBrowserUI();

    const char* menu[] = {"Go to Address", "Bookmarks", "History", "Browser Settings"};
    const SymbianUI::Icon icons[] = {
        SymbianUI::ICON_BROWSER, SymbianUI::ICON_BOOKMARK,
        SymbianUI::ICON_HISTORY, SymbianUI::ICON_SETTINGS
    };
    int menuCount = 4;
    for (int i = 0; i < menuCount; i++) {
        bool sel = (i == browserMenuSel);
        SymbianUI::drawListRow(88 + i * 38, 34, menu[i], sel, ">", icons[i]);
    }

    SymbianUI::drawInfoLine(252, "Engine", "HTTP / local HTML");
    SymbianUI::drawInfoLine(270, "Status", WiFi.status() == WL_CONNECTED ? "Online" : "Offline");

    // Bottom navigation row
    SymbianUI::drawSoftkeys("Open", "Back");
}

void loadURL(String url) {
    browserState = BROWSER_LOADING;
    currentURL = url;
    pageContent = "";
    scrollY = 0;
    linkCount = 0;
    linkURLs.clear();
    
    SymbianUI::drawMessageScreen("Web", SymbianUI::ICON_BROWSER,
                                 "Loading", UiLayout::ellipsize(url, 32), "", "Cancel");
    
    // Progress bar
    for (int i = 0; i <= 100; i += 5) {
        SymbianUI::drawProgressBar(35, 188, 170, i);
        delay(30);
    }
    
    bool localPage = isLocalHTML(url) && !url.startsWith("http://") && !url.startsWith("https://");
    if (localPage) {
        String path = localFilePath(url);
        File file = SD.open(path);
        if (!file) {
            tft.setTextColor(TFT_RED);
            tft.drawString("File not found", 120, 160, 2);
            delay(2000);
            browserState = BROWSER_MAIN;
            drawBrowserMain();
            return;
        }

        pageContent = "";
        while (file.available()) {
            pageContent += (char)file.read();
        }
        file.close();
        pageLoaded = true;
        browserState = BROWSER_VIEW;
        
        linkURLs.clear();
        pageLines.clear();
        
        // Extract page title
        String pageTitle = "";
        int titlePos = pageContent.indexOf("<title>");
        if (titlePos != -1) {
            int titleEnd = pageContent.indexOf("</title>", titlePos);
            if (titleEnd != -1) {
                pageTitle = pageContent.substring(titlePos + 7, titleEnd);
                if (pageTitle.length() > 0) {
                    pageLines.push_back("[Title: " + pageTitle + "]");
                }
            }
        }
        
        // Parse links
        int pos = 0;
        while ((pos = pageContent.indexOf("href=\"", pos)) != -1) {
            int endPos = pageContent.indexOf("\"", pos + 6);
            if (endPos != -1) {
                String link = pageContent.substring(pos + 6, endPos);
                link = extractURL(currentURL, link);
                linkURLs.push_back(link);
            }
            pos = endPos + 1;
        }
        
        // Parse href without quotes
        pos = 0;
        while ((pos = pageContent.indexOf("href='", pos)) != -1) {
            int endPos = pageContent.indexOf("'", pos + 6);
            if (endPos != -1) {
                String link = pageContent.substring(pos + 6, endPos);
                link = extractURL(currentURL, link);
                bool found = false;
                for (const auto &l : linkURLs) {
                    if (l == link) { found = true; break; }
                }
                if (!found) linkURLs.push_back(link);
            }
            pos = endPos + 1;
        }
        
        // Parse page content and handle HTML tags
        String currentLine = "";
        for (int i = 0; i < pageContent.length(); i++) {
            char c = pageContent.charAt(i);
            
            if (c == '<') {
                // Skip <style> blocks
                if (pageContent.substring(i).startsWith("<style")) {
                    int styleEnd = pageContent.indexOf("</style>", i);
                    if (styleEnd != -1) {
                        i = styleEnd + 7;
                    }
                    continue;
                }
                // Handle <img> tags
                if (pageContent.substring(i).startsWith("<img")) {
                    int imgEnd = pageContent.indexOf(">", i);
                    if (currentLine.length() > 0) {
                        pageLines.push_back(currentLine);
                        currentLine = "";
                    }
                    pageLines.push_back("[Image]");
                    i = imgEnd;
                    continue;
                }
                // Handle <br> and <hr>
                if (pageContent.substring(i).startsWith("<br") || pageContent.substring(i).startsWith("<hr")) {
                    if (currentLine.length() > 0) {
                        pageLines.push_back(currentLine);
                        currentLine = "";
                    }
                    int tagEnd = pageContent.indexOf(">", i);
                    i = tagEnd;
                    continue;
                }
                // Handle <table>, <tr> - treat as line breaks
                if (pageContent.substring(i).startsWith("</tr") || pageContent.substring(i).startsWith("<tr")) {
                    if (currentLine.length() > 0) {
                        pageLines.push_back(currentLine);
                        currentLine = "";
                    }
                    int tagEnd = pageContent.indexOf(">", i);
                    i = tagEnd;
                    continue;
                }
                // Skip other tags
                while (i < pageContent.length() && pageContent.charAt(i) != '>') i++;
                continue;
            }
            
            if (c == '\r') continue;
            if (c == '\n' || currentLine.length() >= 32) {
                if (currentLine.length() > 0) {
                    pageLines.push_back(currentLine);
                    currentLine = "";
                }
                if (c == '\n') continue;
            }
            currentLine += c;
        }
        if (currentLine.length() > 0) pageLines.push_back(currentLine);

        linkCount = linkURLs.size();
        pageScroll = 0;
        pointerX = 10;
        pointerY = 50;
        selectedLink = 0;
        
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Page Loaded!", 120, 80, 2);
        tft.setTextColor(TFT_SILVER);
        tft.drawString(String(pageContent.length()) + " chars", 120, 110, 1);
        if (linkCount > 0) {
            tft.setTextColor(TFT_YELLOW);
            tft.drawString(String(linkCount) + " links found", 120, 130, 1);
        }
        delay(1500);
        browserState = BROWSER_NAVIGATE;
        viewPageContent();
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        tft.setTextColor(TFT_RED);
        tft.drawString("WiFi Not Connected!", 120, 160, 2);
        delay(1500);
        browserState = BROWSER_MAIN;
        drawBrowserMain();
        return;
    }
    
    HTTPClient http;
    http.begin(url.c_str());
    http.setTimeout(10000);
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        pageContent = http.getString();
        pageLoaded = true;
        browserState = BROWSER_VIEW;
        http.end();
        
        linkURLs.clear();
        pageLines.clear();
        
        // Extract page title
        String pageTitle = "";
        int titlePos = pageContent.indexOf("<title>");
        if (titlePos != -1) {
            int titleEnd = pageContent.indexOf("</title>", titlePos);
            if (titleEnd != -1) {
                pageTitle = pageContent.substring(titlePos + 7, titleEnd);
                if (pageTitle.length() > 0) {
                    pageLines.push_back("[Title: " + pageTitle + "]");
                }
            }
        }
        
        // Parse links
        int pos = 0;
        while ((pos = pageContent.indexOf("href=\"", pos)) != -1) {
            int endPos = pageContent.indexOf("\"", pos + 6);
            if (endPos != -1) {
                String link = pageContent.substring(pos + 6, endPos);
                link = extractURL(url, link);
                linkURLs.push_back(link);
            }
            pos = endPos + 1;
        }
        
        // Parse href without quotes
        pos = 0;
        while ((pos = pageContent.indexOf("href='", pos)) != -1) {
            int endPos = pageContent.indexOf("'", pos + 6);
            if (endPos != -1) {
                String link = pageContent.substring(pos + 6, endPos);
                link = extractURL(url, link);
                bool found = false;
                for (const auto &l : linkURLs) {
                    if (l == link) { found = true; break; }
                }
                if (!found) linkURLs.push_back(link);
            }
            pos = endPos + 1;
        }
        
        // Parse page content and handle HTML tags
        String currentLine = "";
        for (int i = 0; i < pageContent.length(); i++) {
            char c = pageContent.charAt(i);
            
            if (c == '<') {
                // Skip <style> blocks
                if (pageContent.substring(i).startsWith("<style")) {
                    int styleEnd = pageContent.indexOf("</style>", i);
                    if (styleEnd != -1) {
                        i = styleEnd + 7;
                    }
                    continue;
                }
                // Handle <img> tags
                if (pageContent.substring(i).startsWith("<img")) {
                    int imgEnd = pageContent.indexOf(">", i);
                    if (currentLine.length() > 0) {
                        pageLines.push_back(currentLine);
                        currentLine = "";
                    }
                    pageLines.push_back("[Image]");
                    i = imgEnd;
                    continue;
                }
                // Handle <br> and <hr>
                if (pageContent.substring(i).startsWith("<br") || pageContent.substring(i).startsWith("<hr")) {
                    if (currentLine.length() > 0) {
                        pageLines.push_back(currentLine);
                        currentLine = "";
                    }
                    int tagEnd = pageContent.indexOf(">", i);
                    i = tagEnd;
                    continue;
                }
                // Handle <table>, <tr> - treat as line breaks
                if (pageContent.substring(i).startsWith("</tr") || pageContent.substring(i).startsWith("<tr")) {
                    if (currentLine.length() > 0) {
                        pageLines.push_back(currentLine);
                        currentLine = "";
                    }
                    int tagEnd = pageContent.indexOf(">", i);
                    i = tagEnd;
                    continue;
                }
                // Skip other tags
                while (i < pageContent.length() && pageContent.charAt(i) != '>') i++;
                continue;
            }
            
            if (c == '\r') continue;
            if (c == '\n' || currentLine.length() >= 32) {
                if (currentLine.length() > 0) {
                    pageLines.push_back(currentLine);
                    currentLine = "";
                }
                if (c == '\n') continue;
            }
            currentLine += c;
        }
        if (currentLine.length() > 0) pageLines.push_back(currentLine);

        linkCount = linkURLs.size();
        pageScroll = 0;
        pointerX = 10;
        pointerY = 50;
        selectedLink = 0;
        
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Page Loaded!", 120, 80, 2);
        tft.setTextColor(TFT_SILVER);
        tft.drawString(String(pageContent.length()) + " chars", 120, 110, 1);
        if (linkCount > 0) {
            tft.setTextColor(TFT_YELLOW);
            tft.drawString(String(linkCount) + " links found", 120, 130, 1);
        }
        delay(1500);
        browserState = BROWSER_NAVIGATE;
        viewPageContent();
    } else if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND || httpCode == HTTP_CODE_SEE_OTHER || httpCode == HTTP_CODE_TEMPORARY_REDIRECT || httpCode == HTTP_CODE_PERMANENT_REDIRECT) {
        String location = http.header("Location");
        http.end();
        if (location.length() > 0 && location != url) {
            loadURL(location);
            return;
        }
        tft.setTextColor(TFT_RED);
        tft.drawString("Redirect failed", 120, 160, 2);
        delay(2000);
        browserState = BROWSER_MAIN;
        drawBrowserMain();
    } else {
        tft.setTextColor(TFT_RED);
        tft.drawString("Load Failed!", 120, 160, 2);
        tft.setTextColor(TFT_SILVER);
        tft.drawString("Error: " + String(httpCode), 120, 180, 1);
        delay(2000);
        browserState = BROWSER_MAIN;
        drawBrowserMain();
    }
}


void viewPageContent() {
    SymbianUI::drawChrome("Web", "Open", "Back");
    tft.fillRect(6, 58, 228, 26, 0x0842);
    tft.drawRect(6, 58, 228, 26, SymbianUI::DIVIDER);
    tft.setTextColor(SymbianUI::FG, 0x0842);
    tft.setTextDatum(ML_DATUM);
    String disp = currentURL;
    if (disp.length() > 28) disp = disp.substring(0, 25) + "...";
    tft.drawString(disp, 10, 71, 1);
    tft.drawFastVLine(204, 62, 18, SymbianUI::DIVIDER);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("GO", 216, 71, 1);

    // Content area
    int contentY = 88;
    int contentHeight = 140;
    tft.fillRect(4, contentY, 232, contentHeight, SymbianUI::BG);
    tft.drawRect(4, contentY, 232, contentHeight, SymbianUI::DIVIDER);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(TL_DATUM);

    int lineCount = 0;
    int maxLines = 6;
    for (int i = pageScroll; i < pageLines.size() && lineCount < maxLines; i++) {
        tft.drawString(pageLines[i], 10, contentY + 8 + lineCount * 16, 1);
        lineCount++;
    }

    if (pageLines.size() > maxLines) {
        int barHeight = ::map(maxLines, 0, pageLines.size(), 0, contentHeight - 16);
        int barY = ::map(pageScroll, 0, pageLines.size() - maxLines, contentY + 8, contentY + 8 + contentHeight - 16 - barHeight);
        tft.fillRect(236, contentY + 8, 4, contentHeight - 16, SymbianUI::MUTED);
        tft.fillRect(236, barY, 4, barHeight, SymbianUI::ACCENT);
    }

    // Links footer panel
    int linkTitleY = contentY + contentHeight + 8;
    tft.setTextColor(SymbianUI::ACCENT);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("Links:", 8, linkTitleY, 1);

    int visibleLinks = min(linkCount, 3);
    for (int i = 0; i < visibleLinks; i++) {
        int y = linkTitleY + 12 + i * 16;
        bool sel = (pointerOverLink() && i == linkIndexAtPointer());
        tft.fillRect(8, y - 2, 220, 14, sel ? SymbianUI::SELECT : SymbianUI::BG);
        tft.setTextColor(sel ? TFT_WHITE : SymbianUI::ACCENT);
        String linkText = linkURLs[i];
        if (linkText.length() > 28) linkText = linkText.substring(0, 25) + "...";
        tft.drawString(String(i + 1) + ". " + linkText, 10, y + 8, 1);
    }

    bool overLink = pointerOverLink();
    drawPointer(overLink);

    SymbianUI::drawSoftkeys("Open", "Back");
}

void enterURL() {
    inputURL = "";
    keyboard.begin();
    keyboard.active = true;
    keyboard.draw(true);
    
    bool gettingURL = true;
    while (gettingURL) {
        buttonManager.update();
        int result = keyboard.handleInput(inputURL);
        
        if (result == 1) {
            if (inputURL.length() > 0) {
                String url = inputURL;
                if (!url.startsWith("http://") && !url.startsWith("https://")) {
                    url = "https://" + url;
                }
                keyboard.active = false;
                gettingURL = false;
                loadURL(url);
            }
        } else if (result == 2) {
            keyboard.active = false;
            gettingURL = false;
            drawBrowserMain();
        }
    }
}

void loopWBrowser() {
    buttonManager.update();
    
    if (browserState == BROWSER_MAIN) {
        const int menuCount = 4;
        if (buttonManager.isJustPressed(KEY_DOWN)) {
            browserMenuSel = (browserMenuSel + 1) % menuCount;
            drawBrowserMain();
            delay(150);
        }
        if (buttonManager.isJustPressed(KEY_UP)) {
            browserMenuSel = (browserMenuSel + menuCount - 1) % menuCount;
            drawBrowserMain();
            delay(150);
        }
        if (isSelectPressed()) {
            switch (browserMenuSel) {
                case 0:
                    enterURL();
                    break;
                case 1:
                    loadURL("https://www.google.com");
                    break;
                case 2:
                    loadURL("https://www.bing.com");
                    break;
                case 3:
                    currentMode = MODE_LAUNCHER;
                    drawLauncherContent();
                    break;
            }
            delay(200);
        }
        if (isBackPressed()) {
            currentMode = MODE_LAUNCHER;
            drawLauncherContent();
            delay(200);
        }
    }
    else if (browserState == BROWSER_NAVIGATE || browserState == BROWSER_VIEW) {
        int linkAreaY = 42 + 8 * 16 + 8;
        int linkBottom = linkAreaY + min(linkCount, 6) * 18;

        if (buttonManager.isJustPressed(KEY_DOWN)) {
            if (pointerY < 170) {
                pointerY += 16;
                if (pointerY > 170) pointerY = 170;
            } else if (pointerY < linkBottom && linkCount > 0) {
                pointerY += 18;
                if (pointerY > linkBottom - 6) pointerY = linkBottom - 6;
            } else if (pageScroll + 8 < pageLines.size()) {
                pageScroll++;
            }
            viewPageContent();
            delay(150);
        }
        if (buttonManager.isJustPressed(KEY_UP)) {
            if (pointerY > 50) {
                pointerY -= 16;
                if (pointerY < 50) pointerY = 50;
            } else if (pageScroll > 0) {
                pageScroll--;
            }
            viewPageContent();
            delay(150);
        }
        if (digitalRead(KEY_RIGHT) == LOW) {
            pointerX += 10;
            if (pointerX > 220) pointerX = 220;
            viewPageContent();
            delay(150);
        }
        if (digitalRead(KEY_LEFT) == LOW) {
            pointerX -= 10;
            if (pointerX < 10) pointerX = 10;
            viewPageContent();
            delay(150);
        }

        if (isSelectPressed()) {
            int idx = linkIndexAtPointer();
            if (idx >= 0 && idx < linkCount) {
                loadURL(linkURLs[idx]);
                delay(200);
                return;
            }
            if (pointerY <= 170 && pageScroll + 8 < pageLines.size()) {
                pageScroll++;
                viewPageContent();
            }
            delay(200);
        }

        if (isBackPressed()) {
            browserState = BROWSER_MAIN;
            drawBrowserMain();
            delay(200);
        }
    }
    else if (browserState == BROWSER_LOADING) {
        if (isBackPressed()) {
            browserState = BROWSER_MAIN;
            drawBrowserMain();
        }
    }
}

void initWBrowser() {
    if (WiFi.status() == WL_CONNECTED) {
        browserState = BROWSER_MAIN;
        selectedLink = 0;
        drawBrowserMain();
    } else {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("WiFi Not Connected!", 120, 80, 2);
        tft.setTextColor(TFT_SILVER);
        tft.drawString("Go to Router app first", 120, 110, 1);
        tft.drawString("to connect to WiFi", 120, 130, 1);
        
        tft.setTextColor(THEME_COLOR);
        tft.drawString("Press any key...", 120, 180, 1);
        
        delay(3000);
        currentMode = MODE_LAUNCHER;
        drawLauncherContent();
    }
}

#endif
