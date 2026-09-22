#ifndef POCHITA_MEDIA_PLAYER_H
#define POCHITA_MEDIA_PLAYER_H

#include <Audio.h>
#include <SD.h>
#include <WiFi.h>
#include <vector>
#include "component/BoardPins.h"
#include "component/SymbianUI.h"

struct RadioStation {
  String name;
  String url;
};

Audio mediaAudio;
std::vector<RadioStation> radioStations;
std::vector<String> musicTracks;
int radioSelected = 0;
int radioScroll = 0;
int musicSelected = 0;
int musicScroll = 0;
int mediaVolume = 12;
bool mediaInitialized = false;
bool mediaPaused = false;
bool mediaIsRadio = false;
String mediaTitle = "Not playing";
String mediaDetail = "";
unsigned long mediaLastUiRefresh = 0;

inline String mediaFileName(const String &path) {
  int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

inline bool supportedAudioFile(String name) {
  name.toLowerCase();
  return name.endsWith(".mp3") || name.endsWith(".aac") ||
         name.endsWith(".flac") || name.endsWith(".wav") ||
         name.endsWith(".m4a");
}

inline void initMediaAudio() {
  if (mediaInitialized) return;
  mediaAudio.setPinout(SPEAKER_BCLK, SPEAKER_LRCLK, SPEAKER_DIN);
  mediaAudio.setVolume(mediaVolume);
  mediaInitialized = true;
  Serial.printf("Audio I2S ready: BCLK=%d LRCLK=%d DOUT=%d\n",
                SPEAKER_BCLK, SPEAKER_LRCLK, SPEAKER_DIN);
}

inline void serviceMediaAudio() {
  if (mediaInitialized) mediaAudio.loop();
}

inline void stopMediaAudio() {
  if (mediaInitialized) mediaAudio.stopSong();
  mediaPaused = false;
  mediaTitle = "Not playing";
  mediaDetail = "";
}

inline void changeMediaVolume(int delta) {
  mediaVolume = constrain(mediaVolume + delta, 0, 21);
  initMediaAudio();
  mediaAudio.setVolume(mediaVolume);
}

inline void toggleMediaPause() {
  if (!mediaInitialized || !mediaAudio.isRunning()) return;
  if (mediaAudio.pauseResume()) mediaPaused = !mediaPaused;
}

inline void loadRadioStations() {
  radioStations.clear();
  const char *paths[] = {"/stations.csv", "/station.csv"};
  File f;
  for (const char *path : paths) {
    f = SD.open(path);
    if (f) break;
  }
  if (f) {
    while (f.available() && radioStations.size() < 64) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (!line.length() || line.startsWith("#")) continue;
      int comma = line.indexOf(',');
      if (comma <= 0) continue;
      String name = line.substring(0, comma);
      String url = line.substring(comma + 1);
      name.trim();
      url.trim();
      if (url.startsWith("http://") || url.startsWith("https://"))
        radioStations.push_back({name, url});
    }
    f.close();
  }
  if (radioStations.empty()) {
    radioStations.push_back({"BBC Radio 1", "http://stream.live.vc.bbcmedia.co.uk/bbc_radio_one"});
    radioStations.push_back({"Smooth Chill", "http://media-the.musicradio.com/SmoothChillMP3"});
  }
}

inline void scanMusicFolder(File dir, const String &base, int depth = 0) {
  if (!dir || depth > 5 || musicTracks.size() >= 128) return;
  File entry = dir.openNextFile();
  while (entry && musicTracks.size() < 128) {
    String name = entry.name();
    String path = name.startsWith("/") ? name : base + "/" + name;
    if (entry.isDirectory()) {
      scanMusicFolder(entry, path, depth + 1);
    } else if (supportedAudioFile(path)) {
      musicTracks.push_back(path);
    }
    entry.close();
    entry = dir.openNextFile();
  }
}

inline void loadMusicTracks() {
  musicTracks.clear();
  File root = SD.open("/music");
  if (root && root.isDirectory()) scanMusicFolder(root, "/music");
  if (root) root.close();
}

inline void drawMediaNowPlaying(const String &appTitle) {
  SymbianUI::drawChrome(appTitle, "Pause", "Back");
  SymbianUI::drawIcon(mediaIsRadio ? SymbianUI::ICON_WIFI : SymbianUI::ICON_FILE,
                      109, 75, SymbianUI::ACCENT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.drawString(UiLayout::ellipsize(mediaTitle, 27), 120, 119, 2);
  tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
  tft.drawString(UiLayout::ellipsize(mediaDetail, 34), 120, 143, 1);
  SymbianUI::drawSectionLabel(173, "Playback");
  SymbianUI::drawInfoLine(202, "Status", mediaPaused ? "Paused" :
                          (mediaAudio.isRunning() ? "Playing" : "Stopped"));
  SymbianUI::drawInfoLine(224, "Volume", String(mediaVolume) + " / 21");
  if (!mediaIsRadio) {
    uint32_t total = mediaAudio.getAudioFileDuration();
    uint32_t current = mediaAudio.getAudioCurrentTime();
    int progress = total ? min(100U, current * 100U / total) : 0;
    SymbianUI::drawProgressBar(20, 252, 200, progress);
  }
  SymbianUI::drawSoftkeys("Pause", "Back", "Stop");
}

inline void drawRadioList() {
  SymbianUI::drawChrome("Internet radio", "Play", "Back");
  if (WiFi.status() != WL_CONNECTED) {
    SymbianUI::drawMessageScreen("Internet radio", SymbianUI::ICON_WIFI,
                                 "WiFi not connected", "Connect in the WiFi application");
    return;
  }
  constexpr int visible = 6;
  if (radioSelected < radioScroll) radioScroll = radioSelected;
  if (radioSelected >= radioScroll + visible) radioScroll = radioSelected - visible + 1;
  for (int row = 0; row < visible; ++row) {
    int index = radioScroll + row;
    if (index >= (int)radioStations.size()) break;
    SymbianUI::drawListRow(58 + row * 36, 34, radioStations[index].name,
                           index == radioSelected, "Stream", SymbianUI::ICON_WIFI);
  }
  SymbianUI::drawInfoLine(280, "Volume", String(mediaVolume));
}

inline void drawMusicList() {
  SymbianUI::drawChrome("Music", "Play", "Back");
  if (musicTracks.empty()) {
    SymbianUI::drawEmptyState(SymbianUI::ICON_FILE, "No audio files",
                              "Copy MP3, AAC or FLAC to /music");
    return;
  }
  constexpr int visible = 6;
  if (musicSelected < musicScroll) musicScroll = musicSelected;
  if (musicSelected >= musicScroll + visible) musicScroll = musicSelected - visible + 1;
  for (int row = 0; row < visible; ++row) {
    int index = musicScroll + row;
    if (index >= (int)musicTracks.size()) break;
    String ext = musicTracks[index].substring(musicTracks[index].lastIndexOf('.') + 1);
    ext.toUpperCase();
    SymbianUI::drawListRow(58 + row * 36, 34, mediaFileName(musicTracks[index]),
                           index == musicSelected, ext, SymbianUI::ICON_FILE);
  }
  SymbianUI::drawInfoLine(280, "Tracks", String(musicTracks.size()));
}

inline void playRadioStation(int index) {
  if (index < 0 || index >= (int)radioStations.size()) return;
  if (WiFi.status() != WL_CONNECTED) {
    drawRadioList();
    return;
  }
  initMediaAudio();
  mediaAudio.stopSong();
  mediaIsRadio = true;
  mediaPaused = false;
  mediaTitle = radioStations[index].name;
  mediaDetail = "Connecting to stream";
  bool ok = mediaAudio.connecttohost(radioStations[index].url.c_str());
  if (!ok) mediaDetail = "Unable to open stream";
  drawMediaNowPlaying("Internet radio");
}

inline void playMusicTrack(int index) {
  if (index < 0 || index >= (int)musicTracks.size()) return;
  initMediaAudio();
  mediaAudio.stopSong();
  mediaIsRadio = false;
  mediaPaused = false;
  mediaTitle = mediaFileName(musicTracks[index]);
  mediaDetail = musicTracks[index];
  bool ok = mediaAudio.connecttoFS(SD, musicTracks[index].c_str());
  if (!ok) mediaDetail = "Unable to open audio file";
  drawMediaNowPlaying("Music player");
}

inline void initRadioApp() {
  initMediaAudio();
  loadRadioStations();
  radioSelected = constrain(radioSelected, 0, max(0, (int)radioStations.size() - 1));
  drawRadioList();
}

inline void initMusicApp() {
  initMediaAudio();
  loadMusicTracks();
  musicSelected = constrain(musicSelected, 0, max(0, (int)musicTracks.size() - 1));
  drawMusicList();
}

inline void loopRadioApp() {
  if (buttonManager.isJustPressed(KEY_UP) && radioSelected > 0) {
    --radioSelected; drawRadioList();
  } else if (buttonManager.isJustPressed(KEY_DOWN) &&
             radioSelected + 1 < (int)radioStations.size()) {
    ++radioSelected; drawRadioList();
  } else if (buttonManager.isJustPressed(KEY_LEFT)) {
    changeMediaVolume(-1); drawRadioList();
  } else if (buttonManager.isJustPressed(KEY_RIGHT)) {
    changeMediaVolume(1); drawRadioList();
  } else if (buttonManager.isJustPressed(KEY_START)) {
    playRadioStation(radioSelected);
  } else if (buttonManager.isJustPressed(KEY_OPTION)) {
    toggleMediaPause(); drawMediaNowPlaying("Internet radio");
  } else if (buttonManager.isJustPressed(KEY_B)) {
    stopMediaAudio(); drawRadioList();
  } else if (buttonManager.isJustPressed(KEY_A)) {
    currentMode = MODE_LAUNCHER; drawLauncherContent();
  }
}

inline void loopMusicApp() {
  if (buttonManager.isJustPressed(KEY_UP) && musicSelected > 0) {
    --musicSelected; drawMusicList();
  } else if (buttonManager.isJustPressed(KEY_DOWN) &&
             musicSelected + 1 < (int)musicTracks.size()) {
    ++musicSelected; drawMusicList();
  } else if (buttonManager.isJustPressed(KEY_LEFT)) {
    changeMediaVolume(-1); drawMusicList();
  } else if (buttonManager.isJustPressed(KEY_RIGHT)) {
    changeMediaVolume(1); drawMusicList();
  } else if (buttonManager.isJustPressed(KEY_START)) {
    playMusicTrack(musicSelected);
  } else if (buttonManager.isJustPressed(KEY_OPTION)) {
    toggleMediaPause(); drawMediaNowPlaying("Music player");
  } else if (buttonManager.isJustPressed(KEY_B)) {
    stopMediaAudio(); drawMusicList();
  } else if (buttonManager.isJustPressed(KEY_A)) {
    currentMode = MODE_LAUNCHER; drawLauncherContent();
  }
}

// Optional callbacks used by ESP32-audioI2S.
void audio_info(const char *info) { Serial.printf("audio: %s\n", info); }
void audio_id3data(const char *info) { if (info && *info) mediaDetail = info; }
void audio_showstation(const char *info) { if (info && *info) mediaDetail = info; }
void audio_showstreamtitle(const char *info) { if (info && *info) mediaDetail = info; }
void audio_bitrate(const char *info) { Serial.printf("bitrate: %s\n", info); }
void audio_eof_mp3(const char *info) { mediaPaused = false; }

#endif
