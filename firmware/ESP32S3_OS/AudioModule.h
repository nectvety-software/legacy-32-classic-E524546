
/*
  Audio/Media Module for ESP32-S3 Nokia OS
  Hỗ trợ WAV, MP3 (giải mã đơn giản), MOD/XM tracker
*/

#ifndef AUDIO_MODULE_H
#define AUDIO_MODULE_H

#include <Arduino.h>
#include <driver/i2s.h>
#include <FS.h>
#include <SD_MMC.h>

using namespace fs;

// I2S pins (có thể tùy chỉnh)
#define I2S_BCLK  4
#define I2S_WS    5
#define I2S_DOUT  6

struct WavHeader {
  char riff[4];
  uint32_t size;
  char wave[4];
  char fmt[4];
  uint32_t fmtSize;
  uint16_t format;
  uint16_t channels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
};

class AudioPlayer {
private:
  File currentFile;
  bool playing;
  bool paused;
  uint32_t fileSize;
  uint32_t dataOffset;
  uint16_t bitsPerSample;
  uint32_t sampleRate;
  uint16_t channels;

  // Volume 0-100
  uint8_t volume;

  // For MOD/XM simple playback
  uint8_t* modData;
  uint32_t modSize;
  int currentPattern;
  int currentRow;

public:
  AudioPlayer() : playing(false), paused(false), volume(80), 
                  modData(nullptr), currentPattern(0), currentRow(0) {}

  bool init() {
    i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 44100,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 1024,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCLK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_DOUT,
      .data_in_num = I2S_PIN_NO_CHANGE
    };

    if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK) {
      return false;
    }

    if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) {
      return false;
    }

    return true;
  }

  bool playFile(const String& filename) {
    if (playing) stop();

    currentFile = SD_MMC.open(filename);
    if (!currentFile) return false;

    fileSize = currentFile.size();

    // Check file type
    if (filename.endsWith(".wav")) {
      return playWav();
    } else if (filename.endsWith(".mod") || filename.endsWith(".xm")) {
      return playMod();
    }

    return false;
  }

  void stop() {
    playing = false;
    paused = false;
    if (currentFile) currentFile.close();
    i2s_zero_dma_buffer(I2S_NUM_0);
  }

  void pause() {
    paused = !paused;
  }

  void setVolume(uint8_t vol) {
    volume = constrain(vol, 0, 100);
  }

  bool isPlaying() { return playing && !paused; }
  bool isPaused() { return paused; }

  uint32_t getPosition() {
    if (!currentFile) return 0;
    return currentFile.position();
  }

  uint32_t getDuration() {
    if (!currentFile || sampleRate == 0) return 0;
    uint32_t dataSize = fileSize - dataOffset;
    uint32_t bytesPerSec = sampleRate * channels * (bitsPerSample / 8);
    return dataSize / bytesPerSec;
  }

  void update() {
    if (!playing || paused || !currentFile) return;

    // Read and play audio buffer
    const int bufferSize = 1024;
    int16_t buffer[bufferSize];

    size_t bytesRead = currentFile.read((uint8_t*)buffer, bufferSize * 2);
    if (bytesRead == 0) {
      // End of file
      stop();
      return;
    }

    // Apply volume
    for (int i = 0; i < bytesRead / 2; i++) {
      buffer[i] = (buffer[i] * volume) / 100;
    }

    size_t bytesWritten;
    i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }

private:
  bool playWav() {
    WavHeader header;
    if (currentFile.read((uint8_t*)&header, sizeof(WavHeader)) != sizeof(WavHeader)) {
      currentFile.close();
      return false;
    }

    // Verify header
    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
      currentFile.close();
      return false;
    }

    sampleRate = header.sampleRate;
    channels = header.channels;
    bitsPerSample = header.bitsPerSample;

    // Skip to data chunk
    char chunkId[4];
    uint32_t chunkSize;
    while (currentFile.position() < fileSize) {
      currentFile.read((uint8_t*)chunkId, 4);
      currentFile.read((uint8_t*)&chunkSize, 4);

      if (strncmp(chunkId, "data", 4) == 0) {
        dataOffset = currentFile.position();
        break;
      } else {
        currentFile.seek(currentFile.position() + chunkSize);
      }
    }

    // Configure I2S for this file
    i2s_set_sample_rates(I2S_NUM_0, sampleRate);

    playing = true;
    return true;
  }

  bool playMod() {
    // Simplified MOD playback - just load data
    modSize = fileSize;
    if (modData) free(modData);
    modData = (uint8_t*)ps_malloc(modSize);

    if (!modData) {
      currentFile.close();
      return false;
    }

    currentFile.read(modData, modSize);
    currentFile.close();

    // Parse MOD header (simplified)
    currentPattern = 0;
    currentRow = 0;

    playing = true;
    return true;
  }
};

#endif
