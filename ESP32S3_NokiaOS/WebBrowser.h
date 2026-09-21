
/*
  Web Browser Module for ESP32-S3 Nokia OS
  Lightweight HTTP client và HTML renderer
*/

#ifndef WEB_BROWSER_H
#define WEB_BROWSER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

class WebBrowser {
private:
  String currentURL;
  String pageTitle;
  String pageContent;
  std::vector<String> history;
  int historyIndex;
  bool loading;

  // HTML Parser state
  enum ParseState { TEXT, TAG, SCRIPT, STYLE };

public:
  WebBrowser() : historyIndex(-1), loading(false) {}

  bool navigate(const String& url) {
    if (!url.startsWith("http://") && !url.startsWith("https://")) {
      currentURL = "http://" + url;
    } else {
      currentURL = url;
    }

    loading = true;

    HTTPClient http;
    http.setTimeout(10000);
    http.begin(currentURL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      pageContent = http.getString();
      parseHTML();
      addToHistory(currentURL);
      loading = false;
      http.end();
      return true;
    }

    loading = false;
    http.end();
    pageContent = "Error: " + String(httpCode);
    return false;
  }

  void goBack() {
    if (historyIndex > 0) {
      historyIndex--;
      navigate(history[historyIndex]);
    }
  }

  void goForward() {
    if (historyIndex < (int)history.size() - 1) {
      historyIndex++;
      navigate(history[historyIndex]);
    }
  }

  String getVisibleText(int maxLines = 20) {
    String text = "";
    int lines = 0;
    int i = 0;

    while (i < pageContent.length() && lines < maxLines) {
      // Skip tags
      if (pageContent[i] == '<') {
        while (i < pageContent.length() && pageContent[i] != '>') i++;
        i++;
        continue;
      }

      // Skip scripts
      if (pageContent.substring(i, i+7) == "<script") {
        while (i < pageContent.length() && pageContent.substring(i, i+9) != "</script>") i++;
        i += 9;
        continue;
      }

      // Add visible text
      if (pageContent[i] == '\n') {
        lines++;
        text += '\n';
      } else if (isPrintable(pageContent[i])) {
        text += pageContent[i];
      }
      i++;
    }

    return text;
  }

  String getTitle() { return pageTitle; }
  String getURL() { return currentURL; }
  bool isLoading() { return loading; }

private:
  void parseHTML() {
    // Extract title
    int titleStart = pageContent.indexOf("<title>");
    int titleEnd = pageContent.indexOf("</title>");
    if (titleStart >= 0 && titleEnd > titleStart) {
      pageTitle = pageContent.substring(titleStart + 7, titleEnd);
    }

    // Remove unnecessary whitespace
    pageContent.replace("  ", " ");
    pageContent.replace("\t", " ");
  }

  void addToHistory(const String& url) {
    // Remove forward history if any
    while ((int)history.size() > historyIndex + 1) {
      history.pop_back();
    }

    history.push_back(url);
    historyIndex++;

    // Limit history size
    if (history.size() > 50) {
      history.erase(history.begin());
      historyIndex--;
    }
  }
};

#endif
