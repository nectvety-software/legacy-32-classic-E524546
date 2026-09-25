#include "MusicService.h"

#if SYMBIAN_AUDIO_ENABLED
static uint32_t readLE32(File &f) {
  uint8_t b[4]; if (f.read(b,4) != 4) return 0;
  return uint32_t(b[0]) | (uint32_t(b[1])<<8) | (uint32_t(b[2])<<16) | (uint32_t(b[3])<<24);
}
static uint16_t readLE16(File &f) {
  uint8_t b[2]; if (f.read(b,2) != 2) return 0;
  return uint16_t(b[0]) | (uint16_t(b[1])<<8);
}
#endif

bool MusicService::begin(fs::FS *filesystem) {
  fs = filesystem;
#if SYMBIAN_AUDIO_ENABLED
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = 44100;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  // Six shorter DMA blocks keep audio stable while saving several KB of internal DMA RAM.
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 192;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = Board::AUDIO_BCLK;
  pins.ws_io_num = Board::AUDIO_WS;
  pins.data_out_num = Board::AUDIO_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  if (err == ESP_OK) err = i2s_set_pin(I2S_NUM_0, &pins);
  if (err != ESP_OK) {
    lastStatus = "I2S init failed";
    hwReady = false;
    return false;
  }
  i2s_zero_dma_buffer(I2S_NUM_0);
  hwReady = true;
  lastStatus = fs ? "Audio ready" : "Audio ready / no SD";
  return true;
#else
  lastStatus = "Audio disabled at build time";
  hwReady = false;
  return false;
#endif
}

bool MusicService::play(const String &path) {
#if SYMBIAN_AUDIO_ENABLED
  if (!hwReady) { lastStatus = "I2S not ready"; return false; }
  return openWav(path);
#else
  (void)path;
  lastStatus = "Audio disabled";
  return false;
#endif
}

uint32_t MusicService::durationSeconds() const {
  const uint32_t bytesPerSecond = sampleRate * uint32_t(channels) * 2UL;
  return bytesPerSecond ? dataTotal/bytesPerSecond : 0;
}
uint32_t MusicService::playedSeconds() const {
  const uint32_t bytesPerSecond = sampleRate * uint32_t(channels) * 2UL;
  return bytesPerSecond ? (dataTotal-dataRemaining)/bytesPerSecond : 0;
}
void MusicService::stop() {
  if (file) file.close();
  isPlaying = false;
  isPaused = false;
  dataRemaining = 0;
  dataTotal = 0;
  playingPath = "";
  finishPending = false;
  naturalEnd = false;
#if SYMBIAN_AUDIO_ENABLED
  if (hwReady) i2s_zero_dma_buffer(I2S_NUM_0);
#endif
  lastStatus = hwReady ? "Stopped" : "Audio unavailable";
}

void MusicService::togglePause() {
  if (!isPlaying) return;
  isPaused = !isPaused;
  lastStatus = isPaused ? "Paused" : "Playing";
}

void MusicService::update() {
#if SYMBIAN_AUDIO_ENABLED
  if (!hwReady || !isPlaying || isPaused || !file) return;
  if (finishPending) {
    if ((int32_t)(millis()-finishDeadline) >= 0) {
      stop(); naturalEnd=true; lastStatus="Finished";
    }
    return;
  }
  if (dataRemaining == 0) {
    finishPending=true; finishDeadline=millis()+150UL; return;
  }

  // Read complete 16-bit samples. Mono is expanded to L/R for MAX98357A/PCM5102.
  // Working buffers are deliberately small because UI redraws are now partial.
  // This cuts persistent internal RAM from ~3 KB to ~1.5 KB.
  static int16_t in[256];
  static int16_t out[512];
  size_t want = min((uint32_t)sizeof(in), dataRemaining);
  want &= ~size_t(1);
  size_t got = file.read((uint8_t*)in, want);
  if (got == 0) { stop(); return; }
  dataRemaining -= min((uint32_t)got, dataRemaining);

  const int samples = got / 2;
  size_t outBytes = 0;
  if (channels == 1) {
    for (int i=0;i<samples;++i) {
      int32_t v = (int32_t(in[i]) * volume) / 100;
      out[i*2] = (int16_t)v;
      out[i*2+1] = (int16_t)v;
    }
    outBytes = samples * 4;
  } else {
    for (int i=0;i<samples;++i) {
      int32_t v = (int32_t(in[i]) * volume) / 100;
      out[i] = (int16_t)v;
    }
    outBytes = got;
  }

  size_t written = 0;
  i2s_write(I2S_NUM_0, out, outBytes, &written, 20 / portTICK_PERIOD_MS);
  if (dataRemaining == 0) {
    // I2S is queued; zeroing DMA immediately would cut the last samples.
    finishPending=true; finishDeadline=millis()+150UL;
  }
#endif
}

#if SYMBIAN_AUDIO_ENABLED
bool MusicService::openWav(const String &path) {
  stop();
  if (!fs) { lastStatus = "microSD unavailable"; return false; }
  file = fs->open(path, FILE_READ);
  if (!file) { lastStatus = "Open failed"; return false; }

  char id[5] = {0};
  if (file.read((uint8_t*)id, 4) != 4 || String(id) != "RIFF") {
    file.close(); lastStatus = "Not WAV"; return false;
  }
  (void)readLE32(file);
  memset(id, 0, sizeof(id));
  if (file.read((uint8_t*)id, 4) != 4 || String(id) != "WAVE") {
    file.close(); lastStatus = "Bad WAV"; return false;
  }

  bool haveFmt = false, haveData = false;
  while (file.available()) {
    memset(id, 0, sizeof(id));
    if (file.read((uint8_t*)id, 4) != 4) break;
    uint32_t sz = readLE32(file);
    if (String(id) == "fmt ") {
      if (sz < 16) { file.close(); lastStatus = "Bad fmt chunk"; return false; }
      uint16_t format = readLE16(file);
      channels = readLE16(file);
      sampleRate = readLE32(file);
      (void)readLE32(file); // byte rate
      (void)readLE16(file); // block align
      bits = readLE16(file);
      if (sz > 16) file.seek(file.position() + (sz - 16));
      if (sz & 1) file.seek(file.position() + 1);
      haveFmt = (format == 1 && bits == 16 && (channels == 1 || channels == 2) &&
                 sampleRate >= 8000 && sampleRate <= 48000);
    } else if (String(id) == "data") {
      dataRemaining = sz;
      haveData = true;
      break;
    } else {
      file.seek(file.position() + sz + (sz & 1));
    }
  }

  if (!haveFmt || !haveData) {
    file.close(); lastStatus = "Need PCM16 WAV 8-48kHz"; return false;
  }

  // Physical bus is always stereo. Mono samples are duplicated in update().
  if (i2s_set_clk(I2S_NUM_0, sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO) != ESP_OK) {
    file.close(); lastStatus = "I2S clock failed"; return false;
  }
  i2s_zero_dma_buffer(I2S_NUM_0);
  dataTotal = dataRemaining;
  playingPath = path;
  naturalEnd = false;
  finishPending = false;
  isPlaying = true;
  isPaused = false;
  lastStatus = "Playing";
  return true;
}
#endif
