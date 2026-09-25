#pragma once
#include <Arduino.h>
#include <FS.h>
#include "BoardConfig.h"

#if SYMBIAN_AUDIO_ENABLED
#include <driver/i2s.h>
#endif

class MusicService {
public:
  bool begin(fs::FS *filesystem);
  bool play(const String &path);
  void stop();
  void togglePause();
  void update();
  void setVolume(uint8_t value) { volume = constrain(value, 0, 100); }
  uint8_t getVolume() const { return volume; }
  bool playing() const { return isPlaying; }
  bool paused() const { return isPaused; }
  bool hardwareReady() const { return hwReady; }
  uint32_t currentSampleRate() const { return sampleRate; }
  uint16_t currentChannels() const { return channels; }
  String status() const { return lastStatus; }
  const String &currentPath() const { return playingPath; }
  uint32_t playedSeconds() const;
  uint32_t durationSeconds() const;
  bool consumeFinished() { bool value=naturalEnd; naturalEnd=false; return value; }
private:
  fs::FS *fs = nullptr;
  File file;
  bool isPlaying = false;
  bool isPaused = false;
  bool hwReady = false;
  uint8_t volume = 80;
  uint32_t sampleRate = 0;
  uint16_t channels = 0;
  uint16_t bits = 0;
  uint32_t dataRemaining = 0;
  uint32_t dataTotal = 0;
  String playingPath;
  bool naturalEnd = false;
  bool finishPending = false;
  uint32_t finishDeadline = 0;
  String lastStatus = "Idle";
#if SYMBIAN_AUDIO_ENABLED
  bool openWav(const String &path);
#endif
};
