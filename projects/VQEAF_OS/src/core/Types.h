#pragma once
#include <Arduino.h>

enum class ScreenId : uint8_t {
  Splash,
  Idle,
  Launcher,
  Explorer, // optional 6-tab Retro-inspired browser, opened via Menu > Options
  WiFi,
  BLE,
  Music,
  Files,
  Collection,
  Gallery,
  TextViewer,
  Browser,
  Shell,
  Settings,
  Themes,
  Applications,
  AppInstaller,
  PackageApp, // launch gate; immediately routes to Browser/TextViewer
  QuickPanel,
  TaskSwitcher,
  Notifications,
  Notes,
  Recovery,
  Lock,
  Clock,
  SystemInfo,
  About,
  Calculator,
  Stopwatch,
  Snake // optional trusted built-in game, activated by signed snake_pixel text package
};

enum class Key : uint8_t {
  Menu, Up, A, Left, Start, Right, Option, Down, B, Select,
  Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, None
};

struct KeyEvent {
  Key key;
  bool pressed;
  bool longPress;
  bool repeat;

  KeyEvent()
      : key(Key::None), pressed(false), longPress(false), repeat(false) {}

  KeyEvent(Key k, bool isPressed, bool isLongPress, bool isRepeat)
      : key(k), pressed(isPressed), longPress(isLongPress), repeat(isRepeat) {}
};
