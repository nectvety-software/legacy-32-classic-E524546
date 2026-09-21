#ifndef POCHITA_AI_CHAT_H
#define POCHITA_AI_CHAT_H

#include <Arduino.h>

// Magic values of the two on-device model formats supported by the engine:
// PLE quantized TinyLM (0x504C4531) and the byte-level Vietnamese FP32
// transformer (0x4C494C4B). Shared with FileManager for .bin detection.
#define AI_MAGIC_PLE 0x504C4531u
#define AI_MAGIC_VN  0x4C494C4Bu

// Runs the on-device LLM chat app for a model file on the SD card. Blocks
// until the user exits (MENU). Returns true when a session ran (including a
// failed load that showed an error screen), false if the path was unusable.
// The chat prompt also understands slash commands (/help /apps /status /sync
// /load /temp /run /quit) to control and sync the OS apps.
bool runAiChat(const String &modelPath);

// ---- model access API (shared with the terminal `ai` command) ---------------
// Loads a model file into PSRAM. Returns 0 on success; on failure writes a
// human-readable reason into `err` and returns nonzero. A previously loaded
// model is freed first.
int aiModelLoad(const String &path, char *err, int errCap);
void aiModelFree();
bool aiModelLoaded();
// Writes "PLE V=.. D=.. L=.. S=.." / "VN ..." into buf; false when unloaded.
bool aiModelInfo(char *buf, int cap);

// Generates an answer for `prompt`. Every emitted chunk of text is forwarded
// to `sink` (may be called with multi-line text). Returns true when a model
// was loaded and generation started.
typedef void (*ai_sink_t)(const char *text, void *ctx);
bool aiGenerate(const char *prompt, int maxTokens, ai_sink_t sink, void *ctx);

// True when the chat executed a `/run` command (i.e. another app was launched
// and the chat exited). Cleared on read. Callers of runAiChat() must check
// this and skip their own redraw so the launched app keeps the screen.
bool aiTookLaunch();

// Recursively scans the SD card for AI model files (PLE/VN magic). Returns
// the total found and copies the first `maxOut` SD-relative paths into out.
int aiScanModels(String *outPaths, int maxOut);

#endif