#pragma once
#include <stdint.h>
#include <chrono>
#include <cstdarg>
#include <cstdio>
struct HostSerial {
  void println(const char *s) { printf("%s\n",s); }
  template<typename... Args> void printf(const char *format, Args... args) {
    ::printf(format,args...);
  }
};
extern HostSerial Serial;
inline uint32_t micros() {
  auto d = std::chrono::steady_clock::now().time_since_epoch();
  return uint32_t(std::chrono::duration_cast<std::chrono::microseconds>(d).count());
}
