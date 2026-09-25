#include "WiFiProfileStore.h"

constexpr int WiFiProfileStore::MAX_PROFILES;

// Reject corrupted/stale NVS entries on boot instead of retrying invalid WPA
// credentials forever. A one-time compaction restores a consistent profile list.
static bool validSavedWiFi(const WiFiProfile &p) {
  if (!p.ssid.length() || p.ssid.length() > 32 || p.password.length() > 64) return false;
  if (p.open) return p.password.length() == 0;
  return p.password.length() >= 8;
}
void WiFiProfileStore::begin() {
  prefs.begin("symbian-net", false);
  const int savedCount = (int)prefs.getUChar("count", 0);
  const int oldCount = min(savedCount, MAX_PROFILES);
  used = 0;
  bool repair = savedCount > MAX_PROFILES;
  for (int i = 0; i < oldCount; ++i) {
    String suffix = String(i);
    WiFiProfile loaded;
    loaded.ssid = prefs.getString((String("s") + suffix).c_str(), "");
    loaded.password = prefs.getString((String("p") + suffix).c_str(), "");
    loaded.open = prefs.getBool((String("o") + suffix).c_str(), false);
    bool duplicate = false;
    for (int n = 0; n < used; ++n)
      if (profiles[n].ssid == loaded.ssid) duplicate = true;
    if (!validSavedWiFi(loaded) || duplicate) { repair = true; continue; }
    profiles[used++] = loaded;
  }
  if (used != oldCount) repair = true;
  last = prefs.getString("last", "");
  if (!has(last)) {
    if (last.length()) repair = true;
    last = used ? profiles[0].ssid : String();
    if (repair) prefs.putString("last", last);
  }
  if (repair) persist(); // only writes NVS when corruption was detected
}

const WiFiProfile &WiFiProfileStore::at(int index) const {
  static WiFiProfile empty;
  if (index < 0 || index >= used) return empty;
  return profiles[index];
}

int WiFiProfileStore::find(const String &ssid) const {
  for (int i = 0; i < used; ++i) if (profiles[i].ssid == ssid) return i;
  return -1;
}

void WiFiProfileStore::persist() {
  prefs.putUChar("count", used);
  for (int i = 0; i < MAX_PROFILES; ++i) {
    String suffix = String(i);
    String sk = String("s") + suffix;
    String pk = String("p") + suffix;
    String ok = String("o") + suffix;
    if (i < used) {
      prefs.putString(sk.c_str(), profiles[i].ssid);
      prefs.putString(pk.c_str(), profiles[i].password);
      prefs.putBool(ok.c_str(), profiles[i].open);
    } else {
      prefs.remove(sk.c_str()); prefs.remove(pk.c_str()); prefs.remove(ok.c_str());
    }
  }
}

void WiFiProfileStore::saveProfile(const String &ssid, const String &password, bool open) {
  WiFiProfile p;
  p.ssid = ssid;
  p.password = open ? String() : password;
  p.open = open;
  if (!validSavedWiFi(p)) return;
  int pos = find(ssid);
  // A successful auto-join can fire repeatedly after a weak-signal roam.
  // Do not rewrite five NVS profiles for unchanged credentials already at #0.
  if (pos == 0 && profiles[0].password == p.password && profiles[0].open == open) {
    if (last != ssid) { last = ssid; prefs.putString("last", last); }
    return;
  }
  if (pos < 0) pos = min(used, MAX_PROFILES - 1);
  if (pos >= used && used < MAX_PROFILES) ++used;
  for (int i = pos; i > 0; --i) profiles[i] = profiles[i - 1];
  profiles[0] = p;
  if (last != ssid) { last = ssid; prefs.putString("last", last); }
  persist();
}

void WiFiProfileStore::remove(const String &ssid) {
  int pos = find(ssid);
  if (pos < 0) return;
  for (int i = pos; i < used - 1; ++i) profiles[i] = profiles[i + 1];
  if (used) --used;
  if (last == ssid) {
    last = used ? profiles[0].ssid : String();
    prefs.putString("last", last);
  }
  persist();
}

void WiFiProfileStore::setLastSSID(const String &ssid) {
  if (!has(ssid) || last == ssid) return; // reduce unnecessary NVS writes
  last = ssid;
  prefs.putString("last", last);
}
