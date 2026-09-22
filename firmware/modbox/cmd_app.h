#ifndef CMD_APP_H
#define CMD_APP_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>

#define MAX_COMMAND_LEN 128
#define MAX_HISTORY 20
#define MAX_LINES 10
#define CMD_FS_ROOT "/spiffs"

enum CmdState {
    CMD_MAIN,
    CMD_KEYBOARD,
    CMD_HELP
};

struct CmdHistory {
    char commands[MAX_HISTORY][MAX_COMMAND_LEN];
    int count;
    int current;
};

void appCMDTerminal();
void cmdInit();
void cmdDraw();
void cmdInputHandler();

void cmdExecute(const String& input);
String cmdParseCommand(const String& input, String& args);
void cmdPrint(const String& text, uint16_t color = 0xFFFF);
void cmdPrintln(const String& text, uint16_t color = 0xFFFF);
void cmdClear();

void cmdKeyboardDraw();
void cmdKeyboardHandle();

void cmdHelp();

extern CmdState cmdState;
extern CmdHistory cmdHistory;
extern String currentPath;
extern String cmdInputBuffer;
extern int cmdScrollOffset;
extern int cmdLineCount;
extern String cmdOutput[50];

extern int scanResultCount;
extern String scanSSIDs[20];
extern int scanRSSIs[20];
extern bool scanEncrypted[20];
extern int scanEnctypes[20];

#endif
