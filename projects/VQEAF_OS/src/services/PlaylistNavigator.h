#pragma once
#include <stdint.h>
// Up to 32 songs. Pure fixed-storage playlist logic for the keypad music app.
class PlaylistNavigator {
public:
  enum class Repeat : uint8_t { Off, All, One };
  Repeat repeat() const { return mode; }
  bool shuffle() const { return randomized; }
  const char *repeatName() const {return mode==Repeat::Off?"OFF":(mode==Repeat::All?"ALL":"ONE");}
  void cycleRepeat() {mode=mode==Repeat::Off?Repeat::All:(mode==Repeat::All?Repeat::One:Repeat::Off);}
  void toggleShuffle(int current) { randomized=!randomized; resetCycle(current); }
  void resetCycle(int current) {played=current>=0 && current<32 ? uint32_t(1)<<current : 0;}
  void markPlayed(int idx) { if(idx>=0&&idx<32)played|=uint32_t(1)<<idx; }
  int next(int count,int current,bool forward,bool automatic,uint32_t seed) {
    if(count<=0)return -1;
    if(count>32)count=32;
    if(current<0||current>=count)current=0;
    if(automatic&&mode==Repeat::One)return current;
    if(randomized && count>1) {
      const uint32_t mask=count==32?UINT32_MAX:((uint32_t(1)<<count)-1U);
      uint32_t available=mask&~played;
      // Manual Next/Previous are allowed to start a new shuffle cycle.
      if(!available){
        if(automatic&&mode!=Repeat::All)return -1;
        played=0;
        available=mask & ~(uint32_t(1)<<current);
      }
      if(!available)available=mask;
      unsigned choices=0;
      for(int i=0;i<count;++i)if(available&(uint32_t(1)<<i))++choices;
      unsigned ordinal=seed%choices;
      for(int i=0;i<count;++i)if(available&(uint32_t(1)<<i)){
        if(ordinal--==0)return i;
      }
      return -1;
    }
    if(!forward)return current?current-1:count-1;
    if(current+1<count)return current+1;
    if(!automatic||mode==Repeat::All)return 0;
    return -1;
  }
private:
  Repeat mode=Repeat::Off;
  bool randomized=false;
  uint32_t played=0;
};
