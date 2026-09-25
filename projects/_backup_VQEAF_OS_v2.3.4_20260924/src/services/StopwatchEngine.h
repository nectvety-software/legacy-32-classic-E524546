#pragma once
#include <stdint.h>
// uint32_t wrap-aware stopwatch and fixed four-lap recorder. No heap or RTC.
class StopwatchEngine {
public:
  bool running() const {return active;}
  uint8_t lapCount() const {return used;}
  uint32_t lapAt(uint8_t i) const {return i<used?laps[i]:0;}
  uint32_t elapsed(uint32_t now) const {return stored+(active?(uint32_t)(now-started):0);}
  void toggle(uint32_t now) {
    if(active) stored=elapsed(now);
    else started=now;
    active=!active;
  }
  bool lap(uint32_t now){
    if(!active || used>=4)return false;
    laps[used++]=elapsed(now);return true;
  }
  void reset(){
    if(active)return;
    started=stored=0;used=0;
    for(unsigned i=0;i<4;++i)laps[i]=0;
  }
private:
  uint32_t started=0,stored=0,laps[4]={0,0,0,0};
  uint8_t used=0;
  bool active=false;
};
