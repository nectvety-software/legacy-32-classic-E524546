#pragma once
// Small, freestanding Snake engine: no Arduino, heap, RTTI or external runtime.
#include <stdint.h>
#include <stddef.h>

namespace PixelSnake {
constexpr uint8_t W = 16, H = 18;
constexpr uint16_t CAPACITY = W * H;
struct Point { uint8_t x, y; };
enum class Dir : uint8_t { Up, Right, Down, Left };
enum class Phase : uint8_t { Ready, Running, Paused, Lost, Won };
enum class Result : uint8_t { None, Moved, Ate, Lost, Won };

class Game {
 public:
  void reset(uint32_t seed, bool wrap=false);
  void start();
  void pause();
  bool turn(Dir direction);
  Result step();
  Phase phase() const { return phase_; }
  Dir direction() const { return direction_; }
  Point segment(uint16_t index) const { return body_[index]; }
  Point food() const { return food_; }
  uint16_t length() const { return length_; }
  uint16_t score() const { return score_; }
  bool wrap() const { return wrap_; }
  // Test hooks operate only on trusted local C++ tests, never on user packages.
#ifdef PIXEL_SNAKE_TEST
  void testState(const Point *segments,uint16_t count,Dir dir,Point food,bool wrap);
#endif
 private:
  Point body_[CAPACITY] = {};
  Point food_ = {0,0};
  uint16_t length_ = 0, score_ = 0;
  uint32_t rand_ = 1;
  Dir direction_ = Dir::Right;
  Dir pending_ = Dir::Right;
  bool turnQueued_ = false, wrap_ = false;
  Phase phase_ = Phase::Ready;
  uint32_t random();
  bool occupied(Point p, uint16_t count) const;
  bool newFood();
};
}
