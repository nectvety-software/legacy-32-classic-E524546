# Architecture

- `main.cpp`: state router + boot + global Home key.
- `core/SymbianUI`: chrome/status/softkeys/list + S60 3×3 grid primitives, no full-frame framebuffer.
- `core/InputManager`: 10 GPIO buttons with debounce, long-press event and D-pad auto-repeat.
- `core/TextKeyboard`: low-memory on-screen keyboard used by WiFi password input.
- `services/StorageService`: SD_MMC mount, directory list, bounded recursive media index.
- `services/SettingsStore`: NVS Preferences.
- `services/MusicService`: optional I2S WAV backend.
- `apps/Apps`: 3×3 launcher and initial system apps.

Memory policy: fixed-size arrays, no thumbnail framebuffer, no unbounded directory recursion, maximum 64 file entries / 48 tracks / 24 wireless scan results.

## v0.4 portrait shell

The current boot flow is `Splash -> Idle -> Launcher/App`. Global long-press shortcuts are resolved in `main.cpp` before per-app handlers, except while `TextKeyboard` is active. `SymbianUI` owns all portrait geometry, modal dialogs and transition effects. Audio is isolated in `MusicService`; the app layer only controls playback and volume.
