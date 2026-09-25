#include "../../src/core/UiFrameMetrics.h"
#include <cassert>
#include <cstdio>
int main(){
  UiFrameMetrics m;
  assert(UiFrameMetrics::elapsed(9u,0xFFFFFFFEu)==11u); // 32-bit micros wrap
  for(int i=0;i<24;i++)m.gameFrame(i==23?86000:5000);
  for(int i=0;i<7;i++)m.navigation(3700);
  for(int i=0;i<28;i++)m.inputDispatch(400+i*300);
  auto v=m.take(5000);
  assert(v.game.count==24 && v.gameFpsX10()==48); // 4.8 content updates/s
  assert(v.game.meanUs()>5000 && v.game.maxUs==86000);
  assert(v.game.p95UpperBoundUs()==8000); // p95 excludes 1 worst event
  assert(v.navigation.count==7 && v.navigation.p95UpperBoundUs()==4000);
  assert(v.input.count==28 && v.input.maxUs==8500);
  auto empty=m.take(5000);assert(!empty.game.count&&!empty.input.count&&!empty.navigation.count);
  assert(empty.gameFpsX10()==0);
  puts("PASS: fixed-memory FPS/input/nav stats, p95 buckets, reset and micros wrap");
}
