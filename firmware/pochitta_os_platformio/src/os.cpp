#include "os.h"
#include "board_config.h"
#include <WiFi.h>
#include <LittleFS.h>

void PochittaOS::begin(){Serial.begin(115200);delay(500);Serial.println("[BOOT] Starting...");ui_.begin();keys_.begin();Serial.println("[BOOT] Init services...");services_.begin();Serial.println("[BOOT] Services ready");open(AppId::HOME);Serial.println("[BOOT] Home menu drawn");}
std::vector<MenuItem> PochittaOS::appMenu(AppId id){
 switch(id){
 case AppId::HOME:return {{"WiFi"},{"Bluetooth"},{"Files"},{"Terminal"},{"Notes"},{"LoRa"},{"IR"},{"Browser"},{"Settings"}};
 case AppId::WIFI:return {{"Scan Networks"},{"WiFi Status"},{"Disconnect"},{"Advanced"}};
 case AppId::BLUETOOTH:return {{"Scan BLE Devices"},{"Device Name",DEVICE_NAME,false},{"About BLE"}};
 case AppId::FILES:return {{"Internal Files"},{"SD Card",services_.sdReady()?"Ready":"Unavailable",true},{"Storage Info"}};
 case AppId::TERMINAL:return {{"Run: help"},{"Run: info"},{"Run: ls"},{"Run: wifi"},{"Clear Screen"}};
 case AppId::NOTES:return {{"New Demo Note"},{"All Notes"}};
 case AppId::LORA:return {{"LoRa Status",USE_LORA?"On":"Disabled",false},{"Send Demo"},{"Inbox"},{"Settings"}};
 case AppId::IR:return {{"IR Status",USE_IR?"On":"Disabled",false},{"TV Power"},{"Saved Codes"},{"Settings"}};
 case AppId::BROWSER:return {{"Open example.com"},{"Open local gateway"},{"Bookmarks"},{"History"}};
 case AppId::SETTINGS:return {{"Brightness +"},{"Brightness -"},{"WiFi"},{"Bluetooth"},{"About Device"}};
 default:return {};
 }
}
void PochittaOS::open(AppId id){app_=id;selected_=scroll_=0;items_=appMenu(id);render();}
void PochittaOS::render(){static const char* names[]={"Menu","WiFi","Bluetooth","Files","Terminal","Notes","LoRa","IR","Browser","Settings"};ui_.menu(names[(int)app_],items_,selected_,scroll_);}
void PochittaOS::move(int d){if(items_.empty())return;selected_=(selected_+d+(int)items_.size())%items_.size();int maxRows=5;if(selected_<scroll_)scroll_=selected_;if(selected_>=scroll_+maxRows)scroll_=selected_-maxRows+1;render();}
void PochittaOS::goBack(){if(app_==AppId::HOME)return;open(AppId::HOME);}
void PochittaOS::activate(){
 if(app_==AppId::HOME){open((AppId)(selected_+1));return;}
 if(app_==AppId::WIFI){if(selected_==0){items_=services_.wifiScan();selected_=scroll_=0;ui_.menu("Available Networks",items_,0,0);}else if(selected_==1){ui_.info("WiFi Status",{{"Status",WiFi.status()==WL_CONNECTED?"Connected":"Disconnected",false},{"SSID",WiFi.SSID(),false},{"IP",WiFi.localIP().toString(),false},{"RSSI",String(WiFi.RSSI())+" dBm",false}});}else if(selected_==2){WiFi.disconnect();ui_.message("WiFi","Disconnected");}}
 else if(app_==AppId::BLUETOOTH&&selected_==0){items_=services_.bleScan();selected_=scroll_=0;ui_.menu("Available Devices",items_,0,0);}
 else if(app_==AppId::FILES){if(selected_==0){items_=services_.listFiles();selected_=scroll_=0;ui_.menu("Internal Files",items_,0,0);}else if(selected_==1){items_=services_.listSDCard();selected_=scroll_=0;ui_.menu("SD Card",items_,0,0);}else ui_.info("Storage Info",{{"Flash total",String(LittleFS.totalBytes()/1024)+" KB",false},{"Flash used",String(LittleFS.usedBytes()/1024)+" KB",false},{"SD status",services_.sdReady()?"Ready":"Unavailable",false},{"SD total",String(services_.sdTotalBytes()/1024/1024)+" MB",false},{"SD used",String(services_.sdUsedBytes()/1024/1024)+" MB",false}});}
 else if(app_==AppId::TERMINAL){String cmd=selected_==0?"help":selected_==1?"info":selected_==2?"ls":selected_==3?"wifi":"";ui_.message("Terminal",cmd.length()?services_.terminalCommand(cmd):"Screen cleared");}
 else if(app_==AppId::NOTES){if(selected_==0){services_.saveNote("Demo note created at "+String(millis()/1000)+" seconds");ui_.message("Notes","Note saved to LittleFS.");}else{items_=services_.listNotes();selected_=scroll_=0;ui_.menu("All Notes",items_,0,0);}}
 else if(app_==AppId::LORA)ui_.message("LoRa",USE_LORA?"RadioLib hardware hook is enabled.":"Enable USE_LORA and set SX127x pins in board_config.h");
 else if(app_==AppId::IR)ui_.message("IR",USE_IR?"IR hardware hook is enabled.":"Enable USE_IR and set IR pins in board_config.h");
 else if(app_==AppId::BROWSER){String url=selected_==0?"http://example.com":"http://192.168.1.1";ui_.message("Browser",selected_<2?services_.httpGet(url):"No saved entries");}
 else if(app_==AppId::SETTINGS){if(selected_==0)services_.setBrightness(min(255,(int)services_.brightness()+25));else if(selected_==1)services_.setBrightness(max(15,(int)services_.brightness()-25));else if(selected_==2)open(AppId::WIFI);else if(selected_==3)open(AppId::BLUETOOTH);else ui_.info("About Device",{{"Name",DEVICE_NAME,false},{"Model",DEVICE_MODEL,false},{"MCU","ESP32-S3 N16R8",false},{"Display","ST7789 240x320",false},{"Build",__DATE__,false}});if(selected_<2)render();}
}
void PochittaOS::loop(){Key k=keys_.poll();if(k==Key::NONE)return;if(k==Key::UP)move(-1);else if(k==Key::DOWN)move(1);else if(k==Key::START)activate();else if(k==Key::A)goBack();else if(k==Key::MENU)open(AppId::HOME);}
