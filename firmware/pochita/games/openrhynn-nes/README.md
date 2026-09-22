# OpenRhynn NES

An original offline NES homebrew adaptation for POCHITA Retro-Go. This is a
real mapper-0 iNES ROM, not the ESP32 native game renamed to `.nes`.

Features in this cartridge build:

- 240p tile map and 8x8 NES sprites
- Elder NPC and three-wolf quest
- experience, level, gold and quest item
- D-pad movement and A-button interaction/attack
- Select opens an in-game exit confirmation
- battery-backed 8 KiB SRAM
- an ASCII save mirror beginning at SRAM `$6000`

Controls:

- D-pad: move
- A: talk or attack
- Select: exit confirmation
- Left/Right: select Yes/No
- B: cancel exit

Build with `build.ps1`. The result is copied to
`sd_card/roms/nes/OpenRhynn.nes`.

NES cartridges cannot access the SD card filesystem directly. Retro-Go stores
the cartridge SRAM/save state using its own save files; the SRAM begins with
readable `NAME`, `LEVEL`, `XP`, `KILLS`, `QUEST`, `GOLD`, and `ITEM` lines.

