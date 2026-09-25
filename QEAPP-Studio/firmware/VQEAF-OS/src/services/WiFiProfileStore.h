#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct WiFiProfile {
  String ssid;
  String password;
  bool open;

  WiFiProfile() : ssid(), password(), open(false) {}
};

class WiFiProfileStore {
public:
  static constexpr int MAX_PROFILES = 5;
  void begin();
  int count() const { return used; }
  const WiFiProfile &at(int index) const;
  int find(const String &ssid) const;
  bool has(const String &ssid) const { return find(ssid) >= 0; }
  void saveProfile(const String &ssid, const String &password, bool open);
  void remove(const String &ssid);
  String lastSSID() const { return last; }
  void setLastSSID(const String &ssid);

private:
  Preferences prefs;
  WiFiProfile profiles[MAX_PROFILES];
  int used = 0;
  String last;
  void persist();
};
