#ifndef POCHITA_OPENRHYNN_GAME_H
#define POCHITA_OPENRHYNN_GAME_H

#include <Arduino.h>
#include <SD.h>
#include <cstring>
#include <vector>
#include "component/Config.h"
#include "component/ButtonManager.h"

// Offline RPG for POCHITA OS, inspired by the entity/combat/drop architecture
// of steel-team/OpenrhynnJavaServer (MIT). No original assets are embedded.
namespace OpenRhynnOffline {

constexpr uint8_t PACKAGE_MAGIC[8] = {'P', 'O', 'R', 'H', 'Y', 'N', 'N', '1'};
constexpr uint8_t PAYLOAD_MAGIC[8] = {'R', 'H', 'Y', 'D', 'A', 'T', 'A', '1'};
constexpr uint16_t PACKAGE_VERSION = 1;
constexpr uint16_t PAYLOAD_VERSION = 1;
constexpr int TILE = 20;
constexpr int MAP_W = 30;
constexpr int MAP_H = 24;
constexpr int VIEW_COLS = 12;
constexpr int VIEW_ROWS = 11;
constexpr int MAP_TOP = 22;
constexpr int HUD_TOP = 242;

struct __attribute__((packed)) PackageHeader {
  uint8_t magic[8];
  uint16_t version;
  uint16_t headerSize;
  uint32_t payloadSize;
  uint32_t payloadCrc32;
  uint32_t flags;
  char title[32];
  uint8_t reserved[8];
};

struct __attribute__((packed)) GamePayloadV1 {
  uint8_t magic[8];
  uint16_t version;
  uint16_t recordSize;
  uint16_t mapCount;
  uint16_t questCount;
  uint16_t mobCount;
  uint16_t itemCount;
  uint16_t startLevel;
  uint16_t startHp;
  uint16_t startAttack;
  uint16_t startDefense;
  uint16_t startGold;
  uint16_t startPotions;
  uint32_t contentRevision;
  uint32_t worldSeed;
};

static_assert(sizeof(PackageHeader) == 64, "OpenRhynn package header must be 64 bytes");
static_assert(sizeof(GamePayloadV1) == 40, "OpenRhynn payload header must be 40 bytes");

struct PackageInfo {
  PackageHeader header{};
  GamePayloadV1 game{};
  String title;
};

struct Player {
  String name = "HERO";
  int level = 1;
  int xp = 0;
  int hp = 40;
  int maxHp = 40;
  int attack = 7;
  int defense = 2;
  int gold = 15;
  int potions = 2;
  int herbs = 0;
  int crystals = 0;
  int weaponTier = 0;
  int armorTier = 0;
  int quest = 0;
  int wolfKills = 0;
  int map = 0;
  int x = 12;
  int y = 13;
};

struct Mob {
  int map;
  int x;
  int y;
  int spawnX;
  int spawnY;
  int hp;
  int maxHp;
  int attack;
  int defense;
  int xp;
  int gold;
  uint8_t type; // 0 wolf, 1 slime, 2 skeleton, 3 guardian
  bool alive;
  unsigned long respawnAt;
};

static Player player;
static std::vector<Mob> mobs;
static String savePath;
static String notice;
static unsigned long noticeUntil = 0;
static unsigned long lastMobTick = 0;
static bool running = false;
static PackageInfo activePackage;

inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return tft.color565(r, g, b);
}

inline String valueFor(const String &line, const String &key) {
  String prefix = key + "=";
  if (!line.startsWith(prefix)) return "";
  String value = line.substring(prefix.length());
  value.trim();
  return value;
}

inline void setNotice(const String &text, unsigned long duration = 1800) {
  notice = text;
  noticeUntil = millis() + duration;
}

inline String gameDirectory(const String &binPath) {
  int slash = binPath.lastIndexOf('/');
  return slash > 0 ? binPath.substring(0, slash) : "/games/openrhynn";
}

inline uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t length) {
  while (length--) {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
  }
  return crc;
}

inline bool loadPackage(const String &path, PackageInfo &info) {
  String lower = path;
  lower.toLowerCase();
  if (!lower.endsWith(".bin")) return false;

  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  PackageHeader header{};
  if (f.size() < sizeof(PackageHeader) + sizeof(GamePayloadV1) ||
      f.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) ||
      std::memcmp(header.magic, PACKAGE_MAGIC, sizeof(PACKAGE_MAGIC)) != 0 ||
      header.version != PACKAGE_VERSION || header.headerSize != sizeof(PackageHeader) ||
      header.payloadSize < sizeof(GamePayloadV1) ||
      static_cast<uint64_t>(header.headerSize) + header.payloadSize != f.size() ||
      !f.seek(header.headerSize)) {
    f.close();
    return false;
  }

  GamePayloadV1 game{};
  if (f.read(reinterpret_cast<uint8_t *>(&game), sizeof(game)) != sizeof(game) ||
      std::memcmp(game.magic, PAYLOAD_MAGIC, sizeof(PAYLOAD_MAGIC)) != 0 ||
      game.version != PAYLOAD_VERSION || game.recordSize != sizeof(GamePayloadV1) ||
      game.mapCount == 0 || game.questCount == 0 || game.mobCount == 0 ||
      game.startLevel == 0 || game.startHp < 10 || game.startAttack == 0) {
    f.close();
    return false;
  }

  if (!f.seek(header.headerSize)) {
    f.close();
    return false;
  }
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t remaining = header.payloadSize;
  uint8_t buffer[256];
  while (remaining) {
    size_t wanted = min(static_cast<uint32_t>(sizeof(buffer)), remaining);
    size_t count = f.read(buffer, wanted);
    if (count != wanted) {
      f.close();
      return false;
    }
    crc = crc32Update(crc, buffer, count);
    remaining -= count;
  }
  f.close();
  if ((crc ^ 0xFFFFFFFFUL) != header.payloadCrc32) return false;

  info.header = header;
  info.game = game;
  char safeTitle[33]{};
  std::memcpy(safeTitle, header.title, sizeof(header.title));
  info.title = String(safeTitle);
  if (!info.title.length()) info.title = "OPENRHYNN";
  return true;
}

inline bool isPackage(const String &path) {
  PackageInfo info;
  return loadPackage(path, info);
}

inline void saveGame() {
  int slash = savePath.lastIndexOf('/');
  String dir = slash > 0 ? savePath.substring(0, slash) : "/games/openrhynn";
  if (!SD.exists("/games")) SD.mkdir("/games");
  if (!SD.exists(dir)) SD.mkdir(dir);
  if (SD.exists(savePath)) SD.remove(savePath);
  File f = SD.open(savePath, FILE_WRITE);
  if (!f) {
    Serial.println("OpenRhynn: cannot write save file");
    return;
  }
  f.println("# POCHITA OS OpenRhynn Offline save data");
  f.println("version=1");
  f.println("name=" + player.name);
  f.println("level=" + String(player.level));
  f.println("xp=" + String(player.xp));
  f.println("hp=" + String(player.hp));
  f.println("max_hp=" + String(player.maxHp));
  f.println("attack=" + String(player.attack));
  f.println("defense=" + String(player.defense));
  f.println("gold=" + String(player.gold));
  f.println("potions=" + String(player.potions));
  f.println("herbs=" + String(player.herbs));
  f.println("crystals=" + String(player.crystals));
  f.println("weapon_tier=" + String(player.weaponTier));
  f.println("armor_tier=" + String(player.armorTier));
  f.println("quest=" + String(player.quest));
  f.println("wolf_kills=" + String(player.wolfKills));
  f.println("map=" + String(player.map));
  f.println("x=" + String(player.x));
  f.println("y=" + String(player.y));
  f.close();
  Serial.println("OpenRhynn: saved to " + savePath);
}

inline bool loadGame() {
  if (!SD.exists(savePath)) return false;
  File f = SD.open(savePath, FILE_READ);
  if (!f) return false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length() || line.startsWith("#")) continue;
    int equal = line.indexOf('=');
    if (equal < 1) continue;
    String key = line.substring(0, equal);
    String val = line.substring(equal + 1);
    if (key == "name") player.name = val;
    else if (key == "level") player.level = val.toInt();
    else if (key == "xp") player.xp = val.toInt();
    else if (key == "hp") player.hp = val.toInt();
    else if (key == "max_hp") player.maxHp = val.toInt();
    else if (key == "attack") player.attack = val.toInt();
    else if (key == "defense") player.defense = val.toInt();
    else if (key == "gold") player.gold = val.toInt();
    else if (key == "potions") player.potions = val.toInt();
    else if (key == "herbs") player.herbs = val.toInt();
    else if (key == "crystals") player.crystals = val.toInt();
    else if (key == "weapon_tier") player.weaponTier = val.toInt();
    else if (key == "armor_tier") player.armorTier = val.toInt();
    else if (key == "quest") player.quest = val.toInt();
    else if (key == "wolf_kills") player.wolfKills = val.toInt();
    else if (key == "map") player.map = val.toInt();
    else if (key == "x") player.x = val.toInt();
    else if (key == "y") player.y = val.toInt();
  }
  f.close();
  player.level = constrain(player.level, 1, 50);
  player.maxHp = max(20, player.maxHp);
  player.hp = constrain(player.hp, 1, player.maxHp);
  player.map = constrain(player.map, 0, 2);
  player.x = constrain(player.x, 1, MAP_W - 2);
  player.y = constrain(player.y, 1, MAP_H - 2);
  if (!player.name.length()) player.name = "HERO";
  return true;
}

inline void addMob(int map, int x, int y, int hp, int atk, int def,
                   int xp, int gold, uint8_t type) {
  mobs.push_back({map, x, y, x, y, hp, hp, atk, def, xp, gold, type, true, 0});
}

// Real OpenRhynn bestiary (steel-team/OpenrhynnServerContent). Species id is
// stored in Mob::type; stats are the server values scaled down to this engine's
// small integer range while preserving the original level/difficulty ordering.
// 0 Sheep, 1 Mosquito, 2 Cursed Sheep, 3 Mummy, 4 Spider, 5 Zombie,
// 6 Zenethor (boss).
inline const char *mobName(uint8_t type) {
  switch (type) {
    case 0: return "Sheep";
    case 1: return "Mosquito";
    case 2: return "Cursed Sheep";
    case 3: return "Mummy";
    case 4: return "Spider";
    case 5: return "Zombie";
    default: return "Zenethor";
  }
}

// Sheep are peaceful in the source data: they never chase or strike the player.
inline bool mobPeaceful(uint8_t type) { return type == 0; }
inline bool mobIsBoss(uint8_t type) { return type == 6; }

inline void createWorld() {
  mobs.clear();
  // Coast (map 1): low-level fauna.
  addMob(1, 7, 7, 18, 3, 1, 8, 3, 0);    // Sheep (peaceful)
  addMob(1, 15, 5, 18, 3, 1, 8, 3, 0);   // Sheep
  addMob(1, 23, 10, 22, 5, 1, 12, 6, 1); // Mosquito
  addMob(1, 10, 18, 28, 6, 2, 16, 6, 2); // Cursed Sheep
  addMob(1, 21, 19, 28, 6, 2, 16, 6, 2); // Cursed Sheep
  // Akana (map 2): undead and the boss Zenethor.
  addMob(2, 8, 6, 40, 8, 3, 24, 12, 3);   // Mummy
  addMob(2, 18, 8, 40, 8, 3, 24, 12, 3);  // Mummy
  addMob(2, 11, 18, 52, 10, 4, 34, 16, 4); // Spider
  addMob(2, 5, 14, 52, 10, 4, 34, 16, 5);  // Zombie
  addMob(2, 25, 12, 120, 15, 6, 120, 90, 6); // Zenethor (boss)
}

inline bool villageBlocked(int x, int y) {
  if (x <= 0 || y <= 0 || x >= MAP_W - 1 || y >= MAP_H - 1) return true;
  if (x >= 3 && x <= 9 && y >= 3 && y <= 6) return true;
  if (x >= 18 && x <= 25 && y >= 4 && y <= 8) return true;
  if (x >= 4 && x <= 10 && y >= 17 && y <= 21) return true;
  if ((x == 14 || x == 15) && y >= 2 && y <= 9) return true;
  return false;
}

inline bool forestBlocked(int x, int y) {
  if (y <= 0 || y >= MAP_H - 1) return true;
  if (x <= 0 || x >= MAP_W - 1) return false;
  if (abs(y - 12) <= 1 || abs(x - 15) <= 1) return false;
  return ((x * 7 + y * 11) % 13 == 0) || ((x * 3 + y * 5) % 19 == 0);
}

inline bool ruinsBlocked(int x, int y) {
  if (y <= 0 || y >= MAP_H - 1) return true;
  if (x <= 0 || x >= MAP_W - 1) return false;
  if ((x == 6 || x == 14 || x == 22) && y > 3 && y < 20 && y != 11 && y != 12)
    return true;
  if ((y == 5 || y == 18) && x > 3 && x < 27 && x % 5 != 0) return true;
  return false;
}

inline bool blocked(int map, int x, int y) {
  if (map == 0) return villageBlocked(x, y);
  if (map == 1) return forestBlocked(x, y);
  return ruinsBlocked(x, y);
}

inline bool occupiedByMob(int map, int x, int y, int ignore = -1) {
  for (int i = 0; i < (int)mobs.size(); ++i) {
    if (i != ignore && mobs[i].alive && mobs[i].map == map &&
        mobs[i].x == x && mobs[i].y == y) return true;
  }
  return false;
}

inline const char *mapName() {
  if (player.map == 0) return "ISLE OF SABRA";
  if (player.map == 1) return "COAST";
  return "AKANA";
}

inline int xpNeeded() { return player.level * 35; }

inline void levelUpIfNeeded() {
  while (player.xp >= xpNeeded()) {
    player.xp -= xpNeeded();
    player.level++;
    player.maxHp += 9;
    player.attack += 2;
    if (player.level % 2 == 0) player.defense++;
    player.hp = player.maxHp;
    setNotice("LEVEL UP! Level " + String(player.level), 2500);
  }
}

inline String questText() {
  switch (player.quest) {
    case 0: return "Talk to Zelma on the Isle of Sabra.";
    case 1: return "Slay Cursed Sheep on the Coast " + String(player.wolfKills) + "/3.";
    case 2: return "Return to Zelma.";
    case 3: return "Collect Morph Scrolls " + String(player.herbs) + "/3.";
    case 4: return "Bring the scrolls to Zelma.";
    case 5: return "Defeat Zenethor in Akana.";
    case 6: return "Return to Zelma with the sigil.";
    default: return "The Isle of Sabra is safe. Explore freely.";
  }
}

inline void drawBar(int x, int y, int w, int h, int value, int maximum,
                    uint16_t color) {
  tft.fillRect(x, y, w, h, rgb(40, 42, 48));
  int fill = maximum > 0 ? constrain(value * w / maximum, 0, w) : 0;
  if (fill) tft.fillRect(x, y, fill, h, color);
  tft.drawRect(x, y, w, h, rgb(220, 220, 210));
}

inline void drawTree(int sx, int sy) {
  tft.fillRect(sx + 8, sy + 11, 4, 9, rgb(92, 56, 31));
  tft.fillCircle(sx + 10, sy + 8, 7, rgb(28, 112, 48));
  tft.fillCircle(sx + 6, sy + 10, 5, rgb(39, 145, 61));
}

inline void drawWall(int sx, int sy, uint16_t base) {
  tft.fillRect(sx, sy, TILE, TILE, base);
  tft.drawLine(sx, sy + 9, sx + TILE - 1, sy + 9, rgb(66, 58, 52));
  tft.drawLine(sx + 9, sy, sx + 9, sy + 9, rgb(66, 58, 52));
  tft.drawLine(sx + 5, sy + 10, sx + 5, sy + 19, rgb(66, 58, 52));
}

inline void drawPlayerSprite(int sx, int sy) {
  tft.fillCircle(sx + 10, sy + 6, 4, rgb(244, 196, 145));
  tft.fillRect(sx + 6, sy + 10, 9, 8, rgb(50, 92, 190));
  tft.fillRect(sx + 5, sy + 12, 2, 6, rgb(235, 203, 135));
  tft.fillRect(sx + 15, sy + 12, 2, 6, rgb(235, 203, 135));
  tft.drawPixel(sx + 8, sy + 5, TFT_BLACK);
  tft.drawPixel(sx + 12, sy + 5, TFT_BLACK);
}

inline void drawNpcSprite(int sx, int sy) {
  tft.fillCircle(sx + 10, sy + 6, 4, rgb(223, 181, 135));
  tft.fillRect(sx + 6, sy + 10, 9, 9, rgb(135, 53, 151));
  tft.fillTriangle(sx + 5, sy + 4, sx + 15, sy + 4, sx + 10, sy, rgb(238, 220, 180));
  tft.setTextColor(TFT_YELLOW);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("!", sx + 10, sy - 2, 1);
}

inline void drawMobSprite(const Mob &m, int sx, int sy) {
  switch (m.type) {
    case 0: { // Sheep: white fluffy body, dark head
      tft.fillCircle(sx + 10, sy + 12, 7, rgb(238, 238, 232));
      tft.fillCircle(sx + 5, sy + 10, 3, rgb(238, 238, 232));
      tft.fillCircle(sx + 15, sy + 10, 3, rgb(238, 238, 232));
      tft.fillCircle(sx + 15, sy + 7, 3, rgb(60, 60, 66));
      tft.drawPixel(sx + 16, sy + 6, TFT_WHITE);
      break;
    }
    case 1: { // Mosquito: tiny dark body with translucent wings
      tft.fillTriangle(sx + 10, sy + 10, sx + 2, sy + 5, sx + 2, sy + 13, rgb(150, 170, 185));
      tft.fillTriangle(sx + 10, sy + 10, sx + 18, sy + 5, sx + 18, sy + 13, rgb(150, 170, 185));
      tft.fillCircle(sx + 10, sy + 11, 3, rgb(58, 66, 58));
      tft.drawLine(sx + 10, sy + 8, sx + 10, sy + 2, rgb(58, 66, 58));
      break;
    }
    case 2: { // Cursed Sheep: dark fleece, red eye
      tft.fillCircle(sx + 10, sy + 12, 7, rgb(74, 62, 84));
      tft.fillCircle(sx + 5, sy + 10, 3, rgb(74, 62, 84));
      tft.fillCircle(sx + 15, sy + 10, 3, rgb(74, 62, 84));
      tft.fillCircle(sx + 15, sy + 7, 3, rgb(28, 24, 32));
      tft.drawPixel(sx + 16, sy + 6, TFT_RED);
      break;
    }
    case 3: { // Mummy: bandaged humanoid
      tft.fillRoundRect(sx + 5, sy + 2, 11, 17, 3, rgb(214, 205, 170));
      tft.drawLine(sx + 5, sy + 6, sx + 15, sy + 7, rgb(150, 140, 110));
      tft.drawLine(sx + 5, sy + 10, sx + 15, sy + 11, rgb(150, 140, 110));
      tft.drawLine(sx + 5, sy + 14, sx + 15, sy + 15, rgb(150, 140, 110));
      tft.drawPixel(sx + 8, sy + 5, TFT_BLACK);
      tft.drawPixel(sx + 12, sy + 5, TFT_BLACK);
      break;
    }
    case 4: { // Spider: round body and legs
      tft.drawLine(sx + 6, sy + 11, sx + 1, sy + 8, rgb(36, 36, 44));
      tft.drawLine(sx + 6, sy + 13, sx + 1, sy + 16, rgb(36, 36, 44));
      tft.drawLine(sx + 14, sy + 11, sx + 19, sy + 8, rgb(36, 36, 44));
      tft.drawLine(sx + 14, sy + 13, sx + 19, sy + 16, rgb(36, 36, 44));
      tft.fillCircle(sx + 10, sy + 12, 5, rgb(36, 36, 44));
      tft.fillCircle(sx + 10, sy + 7, 3, rgb(36, 36, 44));
      tft.drawPixel(sx + 8, sy + 6, TFT_RED);
      tft.drawPixel(sx + 12, sy + 6, TFT_RED);
      break;
    }
    case 5: { // Zombie: sickly green humanoid, arm reaching out
      tft.fillCircle(sx + 10, sy + 6, 4, rgb(120, 150, 96));
      tft.fillRect(sx + 6, sy + 10, 9, 9, rgb(90, 120, 72));
      tft.fillRect(sx + 3, sy + 11, 4, 3, rgb(120, 150, 96));
      tft.drawPixel(sx + 8, sy + 5, TFT_BLACK);
      tft.drawPixel(sx + 12, sy + 5, TFT_RED);
      break;
    }
    default: { // Zenethor: dark sorcerer with pointed hat
      tft.fillTriangle(sx + 10, sy - 1, sx + 3, sy + 8, sx + 17, sy + 8, rgb(74, 34, 116));
      tft.fillCircle(sx + 10, sy + 11, 5, rgb(64, 44, 96));
      tft.fillRect(sx + 5, sy + 13, 11, 6, rgb(52, 32, 82));
      tft.drawPixel(sx + 8, sy + 10, TFT_RED);
      tft.drawPixel(sx + 12, sy + 10, TFT_RED);
      break;
    }
  }
  drawBar(sx + 2, sy, 16, 2, m.hp, m.maxHp, TFT_RED);
}

inline void drawTile(int map, int wx, int wy, int sx, int sy) {
  uint16_t ground = map == 0 ? rgb(110, 164, 82)
                    : map == 1 ? rgb(55, 126, 62)
                               : rgb(138, 118, 82);
  tft.fillRect(sx, sy, TILE, TILE, ground);
  if ((wx + wy * 3) % 7 == 0) tft.drawPixel(sx + 4, sy + 6, rgb(176, 207, 103));
  if (blocked(map, wx, wy)) {
    if (map == 1) drawTree(sx, sy);
    else drawWall(sx, sy, map == 0 ? rgb(145, 101, 61) : rgb(105, 96, 82));
  } else if (map == 0 && wy == 12) {
    tft.fillRect(sx, sy + 6, TILE, 8, rgb(183, 157, 103));
  } else if (map == 2 && (wx + wy) % 11 == 0) {
    tft.drawCircle(sx + 10, sy + 10, 5, rgb(199, 175, 104));
  }
}

inline void drawGame() {
  int camX = constrain(player.x - VIEW_COLS / 2, 0, MAP_W - VIEW_COLS);
  int camY = constrain(player.y - VIEW_ROWS / 2, 0, MAP_H - VIEW_ROWS);
  tft.startWrite();
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 240, MAP_TOP, rgb(31, 42, 57));
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, rgb(31, 42, 57));
  tft.drawString(mapName(), 4, 11, 1);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("Lv " + String(player.level) + "  " + String(player.gold) + "G", 236, 11, 1);

  for (int row = 0; row < VIEW_ROWS; ++row)
    for (int col = 0; col < VIEW_COLS; ++col)
      drawTile(player.map, camX + col, camY + row,
               col * TILE, MAP_TOP + row * TILE);

  if (player.map == 0) {
    int nx = (7 - camX) * TILE;
    int ny = MAP_TOP + (8 - camY) * TILE;
    if (nx > -TILE && nx < 240 && ny >= MAP_TOP && ny < HUD_TOP) drawNpcSprite(nx, ny);
    int sx = (22 - camX) * TILE;
    int sy = MAP_TOP + (14 - camY) * TILE;
    if (sx > -TILE && sx < 240 && sy >= MAP_TOP && sy < HUD_TOP) {
      tft.fillRect(sx + 2, sy + 6, 16, 12, rgb(115, 77, 38));
      tft.fillTriangle(sx, sy + 7, sx + 10, sy, sx + 20, sy + 7, rgb(182, 59, 45));
    }
  }

  for (const auto &m : mobs) {
    if (!m.alive || m.map != player.map) continue;
    int sx = (m.x - camX) * TILE;
    int sy = MAP_TOP + (m.y - camY) * TILE;
    if (sx > -TILE && sx < 240 && sy >= MAP_TOP && sy < HUD_TOP) drawMobSprite(m, sx, sy);
  }
  drawPlayerSprite((player.x - camX) * TILE, MAP_TOP + (player.y - camY) * TILE);

  tft.fillRect(0, HUD_TOP, 240, 78, rgb(24, 27, 34));
  tft.drawFastHLine(0, HUD_TOP, 240, rgb(208, 185, 91));
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, rgb(24, 27, 34));
  tft.drawString(player.name, 5, HUD_TOP + 11, 1);
  drawBar(55, HUD_TOP + 6, 105, 8, player.hp, player.maxHp, rgb(205, 48, 52));
  tft.setTextDatum(MR_DATUM);
  tft.drawString(String(player.hp) + "/" + String(player.maxHp), 235, HUD_TOP + 11, 1);
  drawBar(55, HUD_TOP + 20, 105, 6, player.xp, xpNeeded(), rgb(55, 132, 224));
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(rgb(219, 219, 205), rgb(24, 27, 34));
  String line = millis() < noticeUntil ? notice : questText();
  if (line.length() > 37) line = line.substring(0, 36) + "...";
  tft.drawString(line, 5, HUD_TOP + 39, 1);
  tft.setTextColor(rgb(160, 170, 180), rgb(24, 27, 34));
  tft.drawString("A:Action  OPT:Potion  MENU:Quest", 5, HUD_TOP + 57, 1);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("Hold B: Exit", 235, HUD_TOP + 71, 1);
  tft.endWrite();
}

inline void waitRelease() {
  unsigned long until = millis() + 800;
  while (millis() < until) {
    buttonManager.update();
    if (!buttonManager.isPressed(KEY_OPTION) && !buttonManager.isPressed(KEY_A) &&
        !buttonManager.isPressed(KEY_START) && !buttonManager.isPressed(KEY_SELECT)) break;
    delay(10);
  }
}

inline void drawPanel(const String &title) {
  tft.fillScreen(rgb(18, 24, 31));
  tft.fillRect(0, 0, 240, 28, rgb(41, 69, 91));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, rgb(41, 69, 91));
  tft.drawString(title, 120, 14, 2);
}

inline void showStory(const String &title, const String &body,
                      const String &footer = "A / Select: Continue") {
  drawPanel(title);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(rgb(231, 224, 197), rgb(18, 24, 31));
  int y = 48;
  int start = 0;
  while (start < (int)body.length() && y < 260) {
    int end = min(start + 34, (int)body.length());
    if (end < (int)body.length()) {
      int space = body.lastIndexOf(' ', end);
      if (space > start) end = space;
    }
    String row = body.substring(start, end);
    row.trim();
    tft.drawString(row, 10, y, 1);
    start = end;
    while (start < (int)body.length() && body[start] == ' ') start++;
    y += 18;
  }
  tft.fillRect(0, 288, 240, 32, rgb(41, 69, 91));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, rgb(41, 69, 91));
  tft.drawString(footer, 120, 304, 1);
  waitRelease();
  while (true) {
    buttonManager.update();
    if (buttonManager.isJustPressed(KEY_START) ||
        buttonManager.isJustPressed(KEY_A)) break;
    delay(10);
  }
  waitRelease();
}

inline void createName() {
  String chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  int selected = 0;
  player.name = "";
  waitRelease();
  while (true) {
    drawPanel("CREATE HERO");
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(rgb(202, 209, 216), rgb(18, 24, 31));
    tft.drawString("Choose your adventurer name", 120, 62, 1);
    tft.setTextColor(TFT_YELLOW, rgb(18, 24, 31));
    tft.drawString(player.name.length() ? player.name : "_", 120, 105, 4);
    tft.fillRoundRect(75, 142, 90, 58, 7, rgb(47, 82, 106));
    tft.setTextColor(TFT_WHITE, rgb(47, 82, 106));
    tft.drawString(String(chars[selected]), 120, 171, 4);
    tft.setTextColor(rgb(180, 190, 200), rgb(18, 24, 31));
    tft.drawString("Left/Right: letter", 120, 225, 1);
    tft.drawString("A: add   B: delete", 120, 245, 1);
    tft.drawString("Select: finish", 120, 265, 1);
    buttonManager.update();
    if (buttonManager.isJustPressed(KEY_LEFT)) selected = (selected + chars.length() - 1) % chars.length();
    else if (buttonManager.isJustPressed(KEY_RIGHT)) selected = (selected + 1) % chars.length();
    else if (buttonManager.isJustPressed(KEY_OPTION) && player.name.length() < 10) player.name += chars[selected];
    else if (buttonManager.isJustPressed(KEY_B) && player.name.length()) player.name.remove(player.name.length() - 1);
    else if (buttonManager.isJustPressed(KEY_START) && player.name.length() >= 2) break;
    delay(35);
  }
  waitRelease();
}

inline void usePotion() {
  if (player.potions <= 0) {
    setNotice("No Heal Scrolls");
    return;
  }
  if (player.hp >= player.maxHp) {
    setNotice("Health is already full");
    return;
  }
  player.potions--;
  player.hp = min(player.maxHp, player.hp + 28 + player.level * 2);
  setNotice("Heal Scroll used");
  saveGame();
}

inline void respawnPlayer() {
  player.gold = max(0, player.gold - max(5, player.gold / 10));
  player.hp = player.maxHp;
  player.map = 0;
  player.x = 12;
  player.y = 13;
  showStory("YOU FELL", "A healer on the Isle of Sabra found you near the road. Some gold was lost, but your journey continues.");
  saveGame();
}

inline void enemyStrike(Mob &m) {
  int damage = max(1, m.attack + (int)random(-1, 3) -
                          (player.defense + player.armorTier * 2));
  player.hp -= damage;
  setNotice(String(mobName(m.type)) + " hits for " + String(damage));
  if (player.hp <= 0) respawnPlayer();
}

inline void rewardKill(Mob &m) {
  player.xp += m.xp;
  player.gold += m.gold + random(0, 4);
  if (m.type == 2 && player.quest == 1) {
    player.wolfKills++;
    if (player.wolfKills >= 3) player.quest = 2;
  }
  if (m.type <= 2 && player.quest == 3 && player.herbs < 3) {
    player.herbs++;
    if (player.herbs >= 3) player.quest = 4;
    setNotice("Morph Scroll found " + String(player.herbs) + "/3");
  } else if (mobIsBoss(m.type) && player.quest == 5) {
    player.crystals++;
    player.quest = 6;
    setNotice("Zenethor's Sigil obtained!", 2500);
  } else if (random(100) < 22) {
    player.potions++;
    setNotice("Heal Scroll dropped");
  } else {
    setNotice(String(mobName(m.type)) + " defeated! +" + String(m.xp) + " XP");
  }
  levelUpIfNeeded();
  m.alive = false;
  m.respawnAt = millis() + (mobIsBoss(m.type) ? 120000UL : 18000UL);
  saveGame();
}

inline bool attackAdjacent() {
  int best = -1;
  int distance = 99;
  for (int i = 0; i < (int)mobs.size(); ++i) {
    Mob &m = mobs[i];
    if (!m.alive || m.map != player.map) continue;
    int d = abs(m.x - player.x) + abs(m.y - player.y);
    if (d < distance && d <= 1) { best = i; distance = d; }
  }
  if (best < 0) return false;
  Mob &m = mobs[best];
  int damage = max(1, player.attack + player.weaponTier * 3 +
                          (int)random(-2, 4) - m.defense);
  m.hp -= damage;
  setNotice("You strike for " + String(damage));
  if (m.hp <= 0) rewardKill(m);
  else enemyStrike(m);
  return true;
}

inline bool nearPoint(int x, int y) {
  return abs(player.x - x) + abs(player.y - y) <= 1;
}

inline void interactElder() {
  if (player.quest == 0) {
    showStory("ZELMA", "Welcome to the Isle of Sabra, traveler. Cursed Sheep roam the Coast. Slay three of them and return to me.");
    player.quest = 1;
  } else if (player.quest == 2) {
    showStory("QUEST COMPLETE", "The Coast is calmer now. Take this blade. Next, gather three Morph Scrolls from the creatures along the Coast.");
    player.weaponTier = max(player.weaponTier, 1);
    player.gold += 40;
    player.quest = 3;
  } else if (player.quest == 4) {
    showStory("ZELMA", "These scrolls hold great power. The sorcerer Zenethor has risen in Akana. Defeat him and bring back his sigil.");
    player.herbs -= 3;
    player.armorTier = max(player.armorTier, 1);
    player.quest = 5;
  } else if (player.quest == 6) {
    showStory("RHYNN IS SAFE", "Zenethor is banished and Rhynn breathes again. You are a true hero of the Isle of Sabra. Adventure on!");
    player.crystals = max(0, player.crystals - 1);
    player.gold += 150;
    player.weaponTier = max(player.weaponTier, 2);
    player.quest = 7;
  } else {
    showStory("ZELMA", questText());
  }
  saveGame();
}

inline void interactShop() {
  if (player.gold >= 18) {
    player.gold -= 18;
    player.potions++;
    setNotice("Bought a Heal Scroll for 18G");
    saveGame();
  } else setNotice("Heal Scroll costs 18 gold");
}

inline void action() {
  if (attackAdjacent()) return;
  if (player.map == 0 && nearPoint(7, 8)) { interactElder(); return; }
  if (player.map == 0 && nearPoint(22, 14)) { interactShop(); return; }
  setNotice("Nothing to interact with");
}

inline void movePlayer(int dx, int dy) {
  int nx = player.x + dx;
  int ny = player.y + dy;
  if (player.map == 0 && nx >= MAP_W - 1 && abs(ny - 12) <= 2) {
    player.map = 1; player.x = 1; player.y = 12;
    setNotice("Entered the Coast"); saveGame(); return;
  }
  if (player.map == 1 && nx <= 0 && abs(ny - 12) <= 2) {
    player.map = 0; player.x = MAP_W - 2; player.y = 12;
    setNotice("Returned to Sabra"); saveGame(); return;
  }
  if (player.map == 1 && nx >= MAP_W - 1 && abs(ny - 12) <= 2) {
    if (player.quest < 5) { setNotice("Akana is sealed"); return; }
    player.map = 2; player.x = 1; player.y = 12;
    setNotice("Entered Akana"); saveGame(); return;
  }
  if (player.map == 2 && nx <= 0 && abs(ny - 12) <= 2) {
    player.map = 1; player.x = MAP_W - 2; player.y = 12;
    setNotice("Returned to the Coast"); saveGame(); return;
  }
  if (nx < 1 || ny < 1 || nx >= MAP_W - 1 || ny >= MAP_H - 1 ||
      blocked(player.map, nx, ny)) {
    setNotice("Path blocked", 700);
    return;
  }
  if (occupiedByMob(player.map, nx, ny)) {
    attackAdjacent();
    return;
  }
  player.x = nx;
  player.y = ny;
}

inline void updateMobs() {
  if (millis() - lastMobTick < 650) return;
  lastMobTick = millis();
  for (int i = 0; i < (int)mobs.size(); ++i) {
    Mob &m = mobs[i];
    if (!m.alive) {
      if (!mobIsBoss(m.type) && millis() >= m.respawnAt) {
        m.alive = true; m.hp = m.maxHp; m.x = m.spawnX; m.y = m.spawnY;
      }
      continue;
    }
    if (m.map != player.map) continue;
    if (mobPeaceful(m.type)) continue; // Sheep graze; they never attack
    int dist = abs(m.x - player.x) + abs(m.y - player.y);
    if (dist == 1) {
      enemyStrike(m);
      return;
    }
    int dx = 0, dy = 0;
    if (dist <= 6) {
      if (abs(player.x - m.x) > abs(player.y - m.y)) dx = player.x > m.x ? 1 : -1;
      else dy = player.y > m.y ? 1 : -1;
    } else {
      int direction = random(4);
      dx = direction == 0 ? 1 : direction == 1 ? -1 : 0;
      dy = direction == 2 ? 1 : direction == 3 ? -1 : 0;
    }
    int nx = m.x + dx, ny = m.y + dy;
    if (!blocked(m.map, nx, ny) && !(nx == player.x && ny == player.y) &&
        !occupiedByMob(m.map, nx, ny, i)) { m.x = nx; m.y = ny; }
  }
}

inline void showGameMenu(bool questFirst = false) {
  int tab = questFirst ? 1 : 0;
  waitRelease();
  while (true) {
    drawPanel(tab == 0 ? "INVENTORY" : tab == 1 ? "QUEST LOG" : "CHARACTER");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(rgb(226, 226, 214), rgb(18, 24, 31));
    if (tab == 0) {
      tft.drawString("Heal Scroll       x" + String(player.potions), 14, 55, 2);
      tft.drawString("Morph Scroll     x" + String(player.herbs), 14, 88, 2);
      tft.drawString("Zenethor Sigil   x" + String(player.crystals), 14, 121, 2);
      tft.drawString("Weapon tier       " + String(player.weaponTier), 14, 154, 2);
      tft.drawString("Armor tier        " + String(player.armorTier), 14, 187, 2);
      tft.setTextColor(TFT_YELLOW, rgb(18, 24, 31));
      tft.drawString("Option: use Heal Scroll", 14, 240, 1);
    } else if (tab == 1) {
      tft.setTextColor(TFT_YELLOW, rgb(18, 24, 31));
      tft.drawString("FANTASY WORLDS: RHYNN", 12, 52, 1);
      tft.setTextColor(rgb(226, 226, 214), rgb(18, 24, 31));
      String q = questText();
      int pos = 0, y = 82;
      while (pos < (int)q.length()) {
        String row = q.substring(pos, min(pos + 31, (int)q.length()));
        tft.drawString(row, 12, y, 2); pos += 31; y += 27;
      }
      tft.setTextColor(rgb(150, 170, 180), rgb(18, 24, 31));
      tft.drawString("Chapter " + String(min(player.quest + 1, 8)) + " / 8", 12, 205, 1);
    } else {
      tft.drawString("Name:       " + player.name, 14, 53, 2);
      tft.drawString("Level:      " + String(player.level), 14, 84, 2);
      tft.drawString("Experience: " + String(player.xp) + "/" + String(xpNeeded()), 14, 115, 2);
      tft.drawString("Attack:     " + String(player.attack + player.weaponTier * 3), 14, 146, 2);
      tft.drawString("Defense:    " + String(player.defense + player.armorTier * 2), 14, 177, 2);
      tft.drawString("Gold:       " + String(player.gold), 14, 208, 2);
    }
    tft.fillRect(0, 288, 240, 32, rgb(41, 69, 91));
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, rgb(41, 69, 91));
    tft.drawString("Left/Right: Tab     B: Back", 120, 304, 1);
    buttonManager.update();
    if (buttonManager.isJustPressed(KEY_LEFT)) tab = (tab + 2) % 3;
    else if (buttonManager.isJustPressed(KEY_RIGHT)) tab = (tab + 1) % 3;
    else if (buttonManager.isJustPressed(KEY_OPTION) && tab == 0) usePotion();
    else if (buttonManager.isJustPressed(KEY_A) || buttonManager.isJustPressed(KEY_START) ||
             buttonManager.isJustPressed(KEY_SELECT)) break;
    delay(35);
  }
  waitRelease();
}

inline bool confirmExit() {
  bool yes = false;
  waitRelease();
  while (true) {
    tft.fillScreen(rgb(18, 24, 31));
    tft.fillRoundRect(18, 82, 204, 145, 8, rgb(235, 235, 221));
    tft.drawRoundRect(18, 82, 204, 145, 8, rgb(64, 76, 88));
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(rgb(28, 35, 42), rgb(235, 235, 221));
    tft.drawString("EXIT OPENRHYNN?", 120, 110, 2);
    tft.drawString("Progress will be saved.", 120, 137, 1);
    uint16_t yesBg = yes ? rgb(48, 112, 72) : rgb(205, 207, 198);
    uint16_t noBg = !yes ? rgb(48, 112, 72) : rgb(205, 207, 198);
    tft.fillRoundRect(40, 168, 70, 34, 5, yesBg);
    tft.fillRoundRect(130, 168, 70, 34, 5, noBg);
    tft.setTextColor(yes ? TFT_WHITE : TFT_BLACK, yesBg); tft.drawString("YES", 75, 185, 2);
    tft.setTextColor(!yes ? TFT_WHITE : TFT_BLACK, noBg); tft.drawString("NO", 165, 185, 2);
    buttonManager.update();
    if (buttonManager.isJustPressed(KEY_LEFT) || buttonManager.isJustPressed(KEY_RIGHT)) yes = !yes;
    else if (buttonManager.isJustPressed(KEY_A)) return false;
    else if (buttonManager.isJustPressed(KEY_START)) return yes;
    delay(35);
  }
}

// Nokia-style hold-to-close overlay: draws a filling bar while B is held. The
// caller exits once the hold reaches HOLD_EXIT_MS.
constexpr unsigned long HOLD_EXIT_MS = 900;
inline void drawExitProgress(unsigned long held) {
  int pct = constrain((int)(held * 100 / HOLD_EXIT_MS), 0, 100);
  const int bw = 168, bh = 20;
  const int bx = (240 - bw) / 2, by = 150;
  tft.fillRoundRect(bx - 8, by - 30, bw + 16, bh + 52, 7, rgb(20, 26, 34));
  tft.drawRoundRect(bx - 8, by - 30, bw + 16, bh + 52, 7, rgb(208, 185, 91));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, rgb(20, 26, 34));
  tft.drawString("CLOSING GAME", 120, by - 15, 2);
  tft.fillRect(bx, by, bw, bh, rgb(40, 42, 48));
  tft.fillRect(bx, by, bw * pct / 100, bh, rgb(205, 48, 52));
  tft.drawRect(bx, by, bw, bh, rgb(220, 220, 210));
  tft.setTextColor(rgb(165, 180, 190), rgb(20, 26, 34));
  tft.drawString("Release to cancel", 120, by + bh + 12, 1);
}

inline void drawBoot(bool continuing) {
  tft.fillScreen(rgb(9, 17, 24));
  for (int y = 0; y < 210; y += 20)
    for (int x = 0; x < 240; x += 20)
      tft.fillRect(x, y, 19, 19, ((x / 20 + y / 20) % 2) ? rgb(43, 91, 54) : rgb(51, 109, 62));
  tft.fillCircle(120, 99, 38, rgb(214, 181, 78));
  tft.fillCircle(120, 99, 29, rgb(47, 74, 93));
  tft.fillTriangle(120, 65, 100, 126, 140, 126, rgb(225, 198, 106));
  tft.fillTriangle(120, 77, 108, 118, 132, 118, rgb(64, 107, 142));
  tft.fillRect(0, 210, 240, 110, rgb(18, 24, 31));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_YELLOW, rgb(18, 24, 31));
  tft.drawString("OPENRHYNN", 120, 234, 4);
  tft.setTextColor(TFT_WHITE, rgb(18, 24, 31));
  tft.drawString("FANTASY WORLDS: RHYNN", 120, 263, 1);
  tft.setTextColor(rgb(165, 180, 190), rgb(18, 24, 31));
  tft.drawString(continuing ? "Loading saved hero..." : "Starting a new legend...", 120, 291, 1);
  delay(1200);
}

inline void run(const String &binPath) {
  PackageInfo package;
  if (!loadPackage(binPath, package)) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("INVALID GAME PACKAGE", 120, 138, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("CRC or payload error", 120, 169, 1);
    delay(1800);
    return;
  }
  activePackage = package;
  savePath = gameDirectory(binPath) + "/save.txt";
  player = Player();
  player.level = constrain(static_cast<int>(package.game.startLevel), 1, 50);
  player.maxHp = max(20, static_cast<int>(package.game.startHp));
  player.hp = player.maxHp;
  player.attack = max(1, static_cast<int>(package.game.startAttack));
  player.defense = static_cast<int>(package.game.startDefense);
  player.gold = static_cast<int>(package.game.startGold);
  player.potions = static_cast<int>(package.game.startPotions);
  bool continuing = loadGame();
  createWorld();
  randomSeed(package.game.worldSeed ^ micros());
  drawBoot(continuing);
  if (!continuing) {
    createName();
    showStory("ISLE OF SABRA", "Welcome to the world of Rhynn. Your journey begins on the Isle of Sabra. Seek out Zelma, then face the creatures of the Coast and the sorcerer Zenethor in Akana.");
    saveGame();
  }
  running = true;
  waitRelease();
  drawGame();
  unsigned long bHoldStart = 0;   // Nokia-style hold-B-to-exit tracker
  bool exitOverlay = false;
  while (running) {
    buttonManager.update();
    bool redraw = false;

    // Hold A (Back, ~0.9s) to close the game. Progress
    // is shown as a filling bar; releasing early cancels and resumes play.
    if (buttonManager.isPressed(KEY_A)) {
      if (bHoldStart == 0) bHoldStart = millis();
      unsigned long held = millis() - bHoldStart;
      if (held >= HOLD_EXIT_MS) { saveGame(); running = false; break; }
      if (held >= 140) { drawExitProgress(held); exitOverlay = true; }
      delay(18);
      continue;
    } else if (bHoldStart != 0) {
      bHoldStart = 0;
      if (exitOverlay) { exitOverlay = false; drawGame(); }
    }

    if (buttonManager.isJustPressed(KEY_UP)) { movePlayer(0, -1); redraw = true; }
    else if (buttonManager.isJustPressed(KEY_DOWN)) { movePlayer(0, 1); redraw = true; }
    else if (buttonManager.isJustPressed(KEY_LEFT)) { movePlayer(-1, 0); redraw = true; }
    else if (buttonManager.isJustPressed(KEY_RIGHT)) { movePlayer(1, 0); redraw = true; }
    else if (buttonManager.isJustPressed(KEY_OPTION)) { action(); redraw = true; }
    else if (buttonManager.isJustPressed(KEY_OPTION)) { usePotion(); redraw = true; }
    else if (buttonManager.isJustPressed(KEY_START)) { showGameMenu(false); redraw = true; }
    else if (buttonManager.isJustPressed(KEY_MENU)) { showGameMenu(true); redraw = true; }
    int beforeX = player.x, beforeY = player.y, beforeHp = player.hp;
    updateMobs();
    if (beforeX != player.x || beforeY != player.y || beforeHp != player.hp) redraw = true;
    if (redraw || (notice.length() && millis() < noticeUntil)) drawGame();
    delay(18);
  }
  tft.fillScreen(TFT_BLACK);
}

} // namespace OpenRhynnOffline

inline bool isOpenRhynnGame(const String &path) {
  return OpenRhynnOffline::isPackage(path);
}

inline bool isOpenRhynnFilename(const String &path) {
  String lower = path;
  lower.toLowerCase();
  return lower == "openrhynn.bin" || lower.endsWith("/openrhynn.bin");
}

inline void runOpenRhynnGame(const String &path) {
  OpenRhynnOffline::run(path);
}

#endif
