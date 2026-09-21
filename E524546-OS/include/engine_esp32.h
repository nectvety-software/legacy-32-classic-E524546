// engine_esp32.h — Port lõi LuaS30 (runtime_bridge.c + runtime_lua.c) sang ESP32-S3.
// Giữ nguyên global `engine` + alias `mre`, lifecycle load/update/draw/keypressed/keyreleased/pause/resume/quit.
#pragma once
#include <Arduino.h>

extern "C" {
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}

class LGFX_ESP32S3_ST7789;

void engine_init(LGFX_ESP32S3_ST7789* lcd);
void engine_open_vm();            // tạo lua_State, đăng ký engine.*, đọc conf.lua + main.lua, gọi engine.load()
void engine_close_vm();
lua_State* engine_vm();

// Gọi từ main.cpp
void engine_draw_frame(unsigned long now_ms);       // gọi update(dt)+draw()+flush theo fps
void engine_key_event(const char* key, bool pressed); // phát keypressed/keyreleased vào Lua
void engine_pause();
void engine_resume();
void engine_error_screen(const char* msg);          // màn hình đỏ báo lỗi Lua (port error_screen)
int  engine_fps();
bool engine_sd_ok();
