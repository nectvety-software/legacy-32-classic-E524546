#pragma once
#include <stdint.h>

// Platform-independent *loop latency* accumulator. This is NOT FPS. It
// measures time spent in Arduino loop() including I/O and cooperative sleeps.
// Instrumentation is opt-in because Serial logging itself can add jitter.
class UiPerfCounter {
public:
  struct Report {
    uint32_t samples;
    uint32_t meanUs;
    uint32_t maxUs;
    uint32_t over16ms;
    uint32_t over33ms;
    uint32_t over100ms;
  };

  static uint32_t elapsed(uint32_t now, uint32_t previous) {
    return now - previous; // uint32_t modulo subtraction tolerates micros() wrap
  }
  void record(uint32_t us) {
    ++samples_;
    totalUs_ += us;
    if (us > maxUs_) maxUs_ = us;
    if (us > 16000) ++over16ms_;
    if (us > 33000) ++over33ms_;
    if (us > 100000) ++over100ms_;
  }
  Report snapshot() const {
    return {samples_, samples_ ? uint32_t(totalUs_/samples_) : 0,
            maxUs_, over16ms_, over33ms_, over100ms_};
  }
  Report take() { Report result=snapshot(); reset(); return result; }
  void reset() {
    samples_=0; totalUs_=0; maxUs_=0;
    over16ms_=over33ms_=over100ms_=0;
  }
private:
  uint32_t samples_=0, maxUs_=0;
  uint64_t totalUs_=0;
  uint32_t over16ms_=0, over33ms_=0, over100ms_=0;
};
