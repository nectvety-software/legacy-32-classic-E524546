# Audio hardware — Symbian S3 OS v0.4

The E524546 hardware documents define TFT, keypad and microSD pins, but do not define a speaker/DAC connection. v0.4 therefore adds a non-conflicting default I2S pin set for an external DAC/amplifier.

## Default I2S mapping

| Signal | ESP32-S3 GPIO | Connect to MAX98357A |
|---|---:|---|
| BCLK | 4 | BCLK |
| WS / LRCLK | 1 | LRC |
| DATA OUT | 2 | DIN |
| GND | GND | GND |
| Power | according to module | VIN |
| Speaker | — | SPK+ / SPK- |

GPIO 4/1/2 do not overlap the documented TFT, keypad or SD pins. USB GPIO19/20 and flash/PSRAM-related pins are deliberately avoided.

## Firmware support

`MusicService` now:

- initializes I2S TX during boot,
- reads PCM RIFF/WAVE chunks correctly,
- accepts 16-bit PCM mono/stereo from 8–48 kHz,
- duplicates mono samples to stereo I2S,
- applies software volume 0–100%,
- stops exactly at the WAV data chunk,
- clears DMA on stop/change track.

The current media browser indexes `.wav` files. MP3/AAC decoding is not included in v0.4.

## Changing pins

Override in `platformio.ini` or before including `BoardConfig.h`:

```ini
build_flags =
  ...
  -D SYMBIAN_AUDIO_ENABLED=1
  -D SYMBIAN_AUDIO_BCLK=4
  -D SYMBIAN_AUDIO_WS=1
  -D SYMBIAN_AUDIO_DOUT=2
```

If your physical PCB already has an audio amplifier on different pins, replace these values with the board's real wiring before flashing.
