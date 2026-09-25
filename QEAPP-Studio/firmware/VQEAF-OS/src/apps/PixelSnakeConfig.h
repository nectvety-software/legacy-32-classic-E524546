#pragma once
#include <stddef.h>
#include <stdint.h>
namespace PixelSnake {
struct Config { uint16_t speedMs; bool wrap; enum class Skin:uint8_t { Forest, Night, Amber } skin; };
// Bounded declarative configuration; NOT a programmable QEAPP runtime.
bool parseConfig(const char *text,size_t n,Config &out);
}
