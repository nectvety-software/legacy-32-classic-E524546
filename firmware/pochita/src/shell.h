#ifndef SHELL_H
#define SHELL_H

#include "component/Display.h"
#include "component/Keyboard.h"
#include "component/ui_utils.h"
#include "component/Config.h"
#include "lua_interpreter.h"
#include "ai_chat.h"
#include "system_launch.h"
#include <WiFi.h>
#include <SD.h>

extern TFT_eSPI tft;

String shellBuffer = ""; // Current input
std::vector<String> shellHistory;
int shellScroll = 0;

// `ai models` prints a numbered pick list; typing that number loads the model.
String aiPickModels[24];
int aiPickCount = 0;

void drawShellApp();
void appendShell(String line);

String shellWifiModeName(uint8_t m) {
  switch (m) {
    case WIFI_STA: return "STA";
    case WIFI_AP: return "AP";
    case WIFI_AP_STA: return "AP+STA";
    default: return "OFF";
  }
}

void shellKeysTest() {
  struct Btn { const char* name; int pin; };
  const Btn btns[] = {
      {"MENU", KEY_SELECT},   {"UP", KEY_UP},   {"LEFT", KEY_LEFT},
      {"RIGHT", KEY_RIGHT}, {"DOWN", KEY_DOWN}, {"A", KEY_OPTION},
      {"B", KEY_A},         {"START", KEY_B}, {"SELECT", KEY_START},
      {"OPTION", KEY_OPTION},
  };
  const int n = sizeof(btns) / sizeof(btns[0]);
  const int top = UiLayout::CONTENT_Y + 6;
  tft.fillRect(0, UiLayout::CONTENT_Y, UiLayout::WIDTH,
               UiLayout::FOOTER_Y - UiLayout::CONTENT_Y, SymbianUI::BG);
  tft.setTextDatum(ML_DATUM);
  bool done = false;
  while (!done) {
    for (int i = 0; i < n; i++) {
      int y = top + i * 24;
      bool pressed = digitalRead(btns[i].pin) == LOW;
      tft.fillRect(4, y, UiLayout::WIDTH - 8, 21, SymbianUI::BG);
      tft.setTextColor(pressed ? SymbianUI::ACCENT : SymbianUI::DIM,
                       SymbianUI::BG);
      tft.drawString(btns[i].name, 10, y + 10, 2);
      tft.setTextColor(pressed ? TFT_WHITE : SymbianUI::DIM, SymbianUI::BG);
      tft.drawString(pressed ? "[PRESSED]" : "[ ...... ]", 72, y + 10, 2);
    }
    buttonManager.update();
    if (buttonManager.isJustPressed(KEY_A) ||
        buttonManager.isJustPressed(KEY_OPTION)) done = true;
    delay(25);
  }
  appendShell("keys: done");
  drawShellApp();
}

void appendShell(String line) {
    shellHistory.push_back(line);
    if (shellHistory.size() > 50) shellHistory.erase(shellHistory.begin());
    // Auto scroll to bottom
    if (shellHistory.size() > 10) { 
        shellScroll = shellHistory.size() - 10;
    }
}

void appendShellMulti(String out) {
    int start = 0;
    while (true) {
        int nl = out.indexOf('\n', start);
        if (nl == -1) {
            appendShell(out.substring(start));
            break;
        }
        appendShell(out.substring(start, nl));
        start = nl + 1;
    }
}

// Streaming sink for the `ai <prompt>` terminal command: each emitted chunk is
// pushed into the shell history and the terminal is redrawn live.
void aiShellSink(const char *text, void *ctx) {
    (void)ctx;
    appendShellMulti(String(text));
    drawShellApp();
}

void aiCmd(String rest) {
    rest.trim();
    String cmd = "", arg = "";
    int sp = rest.indexOf(' ');
    if (sp != -1) {
        arg = rest.substring(sp + 1);
        arg.trim();
        cmd = rest.substring(0, sp);
    } else {
        cmd = rest;
    }
    cmd.toLowerCase();

    if (cmd == "models") {
        String tmp[24];
        int m = aiScanModels(tmp, 24);
        aiPickCount = min(m, 24);
        for (int i = 0; i < aiPickCount; i++) aiPickModels[i] = tmp[i];
        if (m == 0) {
            aiPickCount = 0;
            appendShell("no models found on SD");
        } else {
            appendShell(String(m) + " model(s) - pick a number:");
            for (int i = 0; i < m; i++) {
                String n = String(i + 1) + " ";
                if (i < 9) n = " " + n;
                appendShell(n + tmp[i]);
            }
            appendShell("(type the number to load it)");
        }
    } else if (cmd == "apps") {
        int n = systemAppCount();
        appendShell(String(n) + " app(s):");
        for (int i = 0; i < n; i++) appendShell("  " + String(systemAppName(i)));
    } else if (cmd == "status") {
        appendShell("PSRAM free: " + String(ESP.getFreePsram() / 1024) + " KB");
        if (!aiModelLoaded()) {
            appendShell("model: none (ai load <path>)");
        } else {
            char info[96];
            aiModelInfo(info, sizeof(info));
            appendShell("model: " + String(info));
        }
        appendShell("apps: " + String(systemAppCount()));
    } else if (cmd == "load") {
        if (arg.length() == 0) {
            appendShell("usage: ai load <path>");
        } else {
            char err[96];
            int rc = aiModelLoad(arg, err, sizeof(err));
            if (rc == 0) appendShell("loaded: " + String(err));
            else appendShell("load failed: " + String(err));
        }
    } else if (cmd == "free") {
        aiModelFree();
        appendShell("model freed; PSRAM free " +
                    String(ESP.getFreePsram() / 1024) + " KB");
    } else if (cmd == "run") {
        if (arg.length() == 0) {
            appendShell("usage: ai run <app>");
        } else {
            int id = findSystemAppByName(arg);
            if (id < 0) {
                appendShell("no app: " + arg + " (ai apps)");
            } else {
                if (launchSystemApp(id)) {
                    drawShellApp();
                    delay(200);
                    return;  // early-return: do not redraw shell on top
                }
                appendShell("launch failed");
            }
        }
    } else if (cmd == "sync") {
        String tmp[24];
        int m = aiScanModels(tmp, 24);
        appendShell("sync: " + String(m) + " model(s), " +
                    String(systemAppCount()) + " app(s)");
        for (int i = 0; i < m && i < 4; i++) appendShell("  " + tmp[i]);
    } else {
        // `ai <prompt>`: answer with the loaded model, streaming to the shell.
        if (rest.length() == 0) {
            appendShell("usage: ai <load|run|apps|models|status|sync|free|prompt>");
        } else if (!aiModelLoaded()) {
            appendShell("no model loaded. use: ai models (pick a number)");
        } else {
            char info[96];
            aiModelInfo(info, sizeof(info));
            appendShell("AI [" + String(info) + "] ...");
            drawShellApp();
            delay(100);
            if (aiGenerate(rest.c_str(), 200, aiShellSink, NULL)) {
                appendShell("");
                appendShell("(done)");
            } else {
                appendShell("generation failed");
            }
        }
    }
    shellBuffer = "";
    drawShellApp();
}

void lsDir(String path) {
    File root = SD.open(path);
    if (!root || !root.isDirectory()) {
        appendShell("ls: not a dir: " + path);
        return;
    }
    int n = 0;
    File f = root.openNextFile();
    while (f) {
        String name = String(f.name());
        int slash = name.lastIndexOf('/');
        if (slash != -1) name = name.substring(slash + 1);
        if (f.isDirectory()) name += "/";
        else name += " (" + String(f.size()) + "B)";
        appendShell(name);
        n++;
        f = root.openNextFile();
    }
    if (n == 0) appendShell("(empty)");
    else appendShell(String(n) + " entr" + String(n == 1 ? "y" : "ies"));
}

void catFile(String path) {
    File f = SD.open(path);
    if (!f) {
        appendShell("cat: cannot open " + path);
        return;
    }
    int lines = 0;
    while (f.available() && lines < 40) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length()) {
            appendShell(line);
            lines++;
        }
    }
    f.close();
}

void executeCommand(String cmd) {
    appendShell("> " + cmd);
    cmd.trim();
    
    String arg = "";
    int sp = cmd.indexOf(' ');
    if (sp != -1) {
        arg = cmd.substring(sp + 1);
        arg.trim();
        cmd = cmd.substring(0, sp);
    }
    cmd.toLowerCase();

    // `ai models` pick: a bare number loads that model.
    if (aiPickCount > 0) {
        int pick = cmd.toInt();
        if (pick >= 1 && pick <= aiPickCount && arg.length() == 0) {
            String path = aiPickModels[pick - 1];
            char err[96];
            int rc = aiModelLoad(path, err, sizeof(err));
            aiPickCount = 0;
            if (rc == 0) {
                appendShell("model " + String(pick) + " loaded: " + String(err));
                appendShell("now type: ai <prompt>");
            } else {
                appendShell("load failed: " + String(err));
            }
            shellBuffer = "";
            drawShellApp();
            return;
        }
        aiPickCount = 0;  // non-numeric command clears the pick list
    }

    if (cmd == "help") {
        appendShellMulti("Cmds: help clear reboot ls cat\necho free date theme sys sd\nwhoami hw mem pins led temp\nwifi bat bl keys mkdir rm touch\nlua <file>\nai <load|run|apps|models|status|sync|free|prompt>");
    } else if (cmd == "clear") {
        shellHistory.clear();
        shellScroll = 0;
    } else if (cmd == "reboot") {
        appendShell("rebooting...");
        drawShellApp();
        delay(300);
        ESP.restart();
    } else if (cmd == "whoami") {
        appendShell("root@pochita");
    } else if (cmd == "ls") {
        if (arg.length() == 0) arg = "/sd";
        lsDir(arg);
    } else if (cmd == "cat") {
        if (arg.length() == 0) appendShell("usage: cat <file>");
        else catFile(arg);
    } else if (cmd == "echo") {
        appendShell(arg.length() ? arg : "");
    } else if (cmd == "mkdir") {
        if (arg.length() == 0) appendShell("usage: mkdir <dir>");
        else if (SD.mkdir(arg)) appendShell("mkdir: " + arg);
        else appendShell("mkdir: failed");
    } else if (cmd == "rm") {
        if (arg.length() == 0) appendShell("usage: rm <file>");
        else if (SD.remove(arg)) appendShell("rm: removed " + arg);
        else appendShell("rm: failed");
    } else if (cmd == "touch") {
        if (arg.length() == 0) appendShell("usage: touch <file>");
        else {
            File f = SD.open(arg, FILE_APPEND);
            if (f) {
                f.close();
                appendShell("touch: " + arg);
            } else appendShell("touch: failed");
        }
    } else if (cmd == "lua") {
        if (arg.length() == 0) appendShell("usage: lua <file>");
        else {
            appendShell("running " + arg + " ...");
            drawShellApp();
            delay(200);
            tft.fillScreen(SymbianUI::BG);
            String r = lua.runScript(arg);
            if (r.length() == 0) appendShell("(no output)");
            else appendShellMulti(r);
        }
    } else if (cmd == "free") {
        appendShell("Heap: " + String(ESP.getFreeHeap()) + " / " +
                    String(ESP.getHeapSize()) + " B");
    } else if (cmd == "date") {
        appendShell("Time: " + SymbianUI::clockText());
    } else if (cmd == "theme") {
        appendShell("Theme: " + String(currentTheme.name));
    } else if (cmd == "sys") {
        appendShell("Chip: " + String(ESP.getChipModel()));
        appendShell("Freq: " + String(ESP.getCpuFreqMHz()) + " MHz");
        appendShell("Uptime: " + String(millis() / 1000) + " s");
    } else if (cmd == "hw") {
        appendShell("Board: PochitaOS E524546");
        appendShell("SoC: " + String(ESP.getChipModel()) +
                    " rev" + String(ESP.getChipRevision()));
        appendShell("Cores: " + String(ESP.getChipCores()) + " @ " +
                    String(ESP.getCpuFreqMHz()) + " MHz");
        appendShell("Flash: " + String(ESP.getFlashChipSize() / 1024 / 1024) +
                    " MB");
        appendShell("PSRAM: " + String(ESP.getPsramSize() / 1024 / 1024) +
                    " MB");
        appendShell("SDK: " + String(ESP.getSdkVersion()));
    } else if (cmd == "mem") {
        appendShell("Heap free: " + String(ESP.getFreeHeap()) + " B");
        appendShell("Heap min: " + String(ESP.getMinFreeHeap()) + " B");
        if (ESP.getPsramSize() > 0) {
            appendShell("PSRAM free: " + String(ESP.getFreePsram()) + " B");
            appendShell("PSRAM used: " +
                        String(ESP.getPsramSize() - ESP.getFreePsram()) + " B");
        } else appendShell("PSRAM: none");
    } else if (cmd == "pins") {
        appendShellMulti(String("TFT SCLK=") + TFT_SCLK_PIN +
            " MOSI=" + TFT_MOSI_PIN + " CS=" + TFT_CS_PIN +
            " DC=" + TFT_DC_PIN + "\nSD SCLK=" + SD_SCLK +
            " MISO=" + SD_MISO + " MOSI=" + SD_MOSI + " CS=" + SD_CS +
            "\nSPK BCLK=" + SPEAKER_BCLK + " LRCLK=" + SPEAKER_LRCLK +
            " DIN=" + SPEAKER_DIN +
            "\nMIC WS=" + MIC_WS + " SCK=" + MIC_SCK + " DIN=" + MIC_SD +
            "\nLED=" + STATUS_LED + " BL=" + TFT_BL);
    } else if (cmd == "led") {
        pinMode(STATUS_LED, OUTPUT);
        if (arg == "on") {
            digitalWrite(STATUS_LED, HIGH);
            appendShell("LED (GPIO" + String(STATUS_LED) + ") on");
        } else if (arg == "off") {
            digitalWrite(STATUS_LED, LOW);
            appendShell("LED (GPIO" + String(STATUS_LED) + ") off");
        } else if (arg == "blink") {
            appendShell("LED blinking (GPIO" + String(STATUS_LED) + ")");
            drawShellApp();
            for (int i = 0; i < 8; i++) {
                digitalWrite(STATUS_LED, i % 2);
                delay(150);
            }
            digitalWrite(STATUS_LED, LOW);
            appendShell("led: done");
        } else appendShell("usage: led on|off|blink");
    } else if (cmd == "temp") {
        appendShell("Chip temp: " + String((float)temperatureRead(), 1) + " C");
    } else if (cmd == "wifi") {
        wl_status_t st = WiFi.status();
        appendShell("Mode: " + shellWifiModeName(WiFi.getMode()));
        appendShell("Status: " + String((int)st));
        if (st == WL_CONNECTED) {
            appendShell("SSID: " + WiFi.SSID());
            appendShell("IP: " + WiFi.localIP().toString());
            appendShell("RSSI: " + String(WiFi.RSSI()) + " dBm");
        }
        appendShell("MAC: " + WiFi.macAddress());
    } else if (cmd == "bat") {
        appendShell("Battery: " + String(SymbianUI::batteryPercent()) +
                    "% (USB/sim)");
    } else if (cmd == "bl") {
        int v = arg.toInt();
        if (arg.length() > 0 && v >= 0 && v <= 255) {
            tft.setBrightness(v);
            appendShell("Backlight: " + String(v));
        } else appendShell("usage: bl <0-255>");
    } else if (cmd == "keys") {
        shellKeysTest();
    } else if (cmd == "sd") {
        if (SD.cardType() == CARD_NONE) appendShell("SD: not mounted");
        else {
            const char* types[] = {"", "MMC", "SDSC", "SDHC/SDXC", "SDUC"};
            int t = (int)SD.cardType();
            appendShell("SD: " + String(t >= 1 && t <= 4 ? types[t] : "?"));
            appendShell("Size: " + String((float)SD.cardSize() / 1048576.0f, 1) +
                        " MB");
            appendShell("Free: " + String((float)(SD.totalBytes() - SD.usedBytes()) / 1048576.0f, 1) +
                        " MB");
        }
    } else if (cmd == "ai") {
        aiCmd(arg);
        return;  // aiCmd already redraws / early-returns on launch
    } else {
        appendShell("Unknown command.");
    }
    
    shellBuffer = ""; 
    drawShellApp();
}

void drawShellApp() {
    SymbianUI::drawChrome("Terminal", "Type", "Home");
    SymbianUI::drawSectionLabel(54, "root@pochita:~");
    
    // Output Area (Lines 0 to 9 visible? fit 240px?)
    int yStart = 76;
    int maxLines = 16;
    
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(TL_DATUM);
    
    for (int i = 0; i < maxLines; i++) {
        int idx = shellScroll + i;
        if (idx >= shellHistory.size()) break;
        tft.drawString(shellHistory[idx], 5, yStart + i*12, 1);
    }
    drawScrollBar(230, yStart, 10, maxLines * 12, shellHistory.size(), shellScroll, maxLines);
    
    // Footer / Input status
    if (!keyboard.active) {
        tft.fillRect(0, 270, UiLayout::WIDTH, 24, TFT_BLACK);
        tft.drawRect(3, 271, 234, 22, SymbianUI::ACCENT);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(ML_DATUM);
        String prompt = "$ " + shellBuffer;
        if (prompt.length() > 30) prompt = prompt.substring(prompt.length()-30);
        tft.drawString(prompt + "_", 8, 282, 1);
        SymbianUI::drawSoftkeys("Type", "Home");
    }
}

void loopShell() {
    if (keyboard.active) {
        int result = keyboard.handleInput(shellBuffer);
        
        if (result == 1) { // OK -> Execute
            keyboard.active = false;
            executeCommand(shellBuffer);
        } else if (result == 2) { // Cancel
            keyboard.active = false;
            drawShellApp();
        }
        return;
    }

    if (isSelectPressed()) { 
        keyboard.begin();
        keyboard.active = true;
        keyboard.draw(true);
        delay(300);
    }
    
    // Scrolling
    if (buttonManager.isJustPressed(KEY_UP)) {
        if (shellScroll > 0) shellScroll--;
        drawShellApp();
        delay(50);
    }
    else if (buttonManager.isJustPressed(KEY_DOWN)) {
        if (shellScroll < shellHistory.size() - 1) shellScroll++;
        drawShellApp();
        delay(50);
    }
}

#endif
