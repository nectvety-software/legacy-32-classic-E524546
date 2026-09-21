#ifndef POCHITA_NES_EMULATOR_H
#define POCHITA_NES_EMULATOR_H

#include <Arduino.h>

// Launch the nofrendo NES emulator on a .nes ROM stored on the SD card.
// Blocks until the user exits (OPTION key). On return the caller is
// responsible for restoring its own UI (the emulator restores the display
// rotation but repaints nothing of the OS).
//
// romPath is a POSIX/SD path, e.g. "/roms/nes/game.nes" or "/sd/...".
// Returns true if the ROM was launched, false if it could not be loaded.
bool runNesEmulator(const String &romPath);

#endif // POCHITA_NES_EMULATOR_H
