#ifndef POCHITA_SYSTEM_LAUNCH_H
#define POCHITA_SYSTEM_LAUNCH_H

#include <Arduino.h>

// Shared app-launch bridge used by the terminal `ai run` command and the AI
// chat `/run` slash command. Implemented in main.cpp where the launcher app
// registry (appList) and every app init function live.
//
// Launching an app switches `currentMode` and draws the app immediately, so
// after a successful launch the caller must stop drawing its own screen and
// let the main loop dispatch to the new mode.

// Launches a launcher app by its systemId (0..14). Returns true when handled.
bool launchSystemApp(int id);

// Resolves an app name (case-insensitive, also accepts a numeric systemId)
// to its systemId; -1 when not found.
int findSystemAppByName(const String &name);

// App registry access for the AI status / apps listing.
int systemAppCount();
const char *systemAppName(int i);

#endif