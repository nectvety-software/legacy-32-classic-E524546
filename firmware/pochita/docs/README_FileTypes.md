# File Types Supported

## Game Consoles / Retro Gaming

| Extension | Console | Tag |
|----------|---------|-----|
| `.nes` | Nintendo NES | [NES] |
| `.snes` `.smc` `.sfc` | Super Nintendo | [SNES] |
| `.gb` | GameBoy | [GB] |
| `.gbc` | GameBoy Color | [GBC] |
| `.gba` | GameBoy Advance | [GBA] |
| `.sms` | Master System | [SMS] |
| `.sg1000` `.sc` | SG-1000 | [SG1K] |
| `.md` `.gen` `.bin` | Mega Drive/Genesis | [MD] |
| `.gg` | Game Gear | [GG] |
| `.col` `.cv` | ColecoVision | [COL] |
| `.pce` | PC Engine | [PCE] |
| `.lynx` | Atari Lynx | [LYNX] |
| `.wad` `.pwad` | DOOM | [DOOM] |

## Scripts & Code

| Extension | Language | Tag |
|----------|----------|-----|
| `.lua` | Lua Script | [LUA] |
| `.py` | Python | [PY] |
| `.js` | JavaScript | [SCR] |
| `.html` `.htm` | HTML | [HTM] |
| `.json` | JSON | [JSON] |
| `.php` | PHP | [SCR] |
| `.css` | CSS | [SCR] |

## Documents

| Extension | Type | Tag |
|-----------|------|-----|
| `.txt` | Text | [TXT] |
| `.log` | Log | [TXT] |
| `.ino` `.cpp` `.h` | Arduino/Code | [TXT] |

## Images

| Extension | Type | Tag |
|-----------|------|-----|
| `.jpg` `.jpeg` | JPEG Image | [IMG] |
| `.png` | PNG Image | [IMG] |
| `.bmp` | Bitmap | [IMG] |

## SD Card Folder Structure

```
/sdcard/
├── /notes/           # Lưu ghi chú Notes app
├── /retro/           # ROM games
│   ├── /nes/
│   ├── /snes/
│   ├── /gb/
│   ├── /gbc/
│   ├── /gba/
│   ├── /sms/
│   ├── /md/
│   └── /doom/
├── /theme/           # Theme files
├── /icons/           # Icons
├── /system/          # System files
│   └── /cache/
├── /data/
│   ├── /images/
│   ├── /music/
│   └── /documents/
└── /scripts/         # Lua scripts
```

## Emulator Requirements

Để chơi game, cần cài đặt **Retro-Go** firmware:
- Website: https://retro-go.github.io
- Hỗ trợ: NES, SNES, GB, GBC, GBA, SMS, MD, GG, Coleco, PC Engine, Lynx, DOOM
