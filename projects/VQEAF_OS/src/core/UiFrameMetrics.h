#pragma once
#include <stdint.h>
// No heap allocations. Measures software LCD draw completion and event-to-
// dispatch completion; physical photon latency needs external instrumentation.
class UiFrameMetrics {
public:
  struct Stats { uint32_t count=0, maxUs=0; uint64_t totalUs=0;
    uint32_t bucket[8]={};
    uint32_t meanUs() const {return count?uint32_t(totalUs/count):0;}
    uint32_t p95UpperBoundUs() const {
      if(!count)return 0;
      static const uint32_t upper[]={2000,4000,8000,16000,33000,50000,100000,UINT32_MAX};
      const uint32_t target=count-(count/20);uint32_t seen=0;
      for(unsigned i=0;i<8;i++){seen+=bucket[i];if(seen>=target)return upper[i];}
      return UINT32_MAX;
    }
  };
  struct Window {Stats game, navigation, input;uint32_t elapsedMs=0;
    uint32_t gameFpsX10() const {return elapsedMs?uint32_t((uint64_t(game.count)*10000ULL)/elapsedMs):0;}
  };
  static uint32_t elapsed(uint32_t now, uint32_t before){return now-before;}
  void gameFrame(uint32_t microsDraw){add(game_,microsDraw);}
  void navigation(uint32_t microsDraw){add(nav_,microsDraw);}
  void inputDispatch(uint32_t us){add(input_,us);}
  Window take(uint32_t elapsedMs){Window w;w.game=game_;w.navigation=nav_;w.input=input_;w.elapsedMs=elapsedMs;
    game_=Stats();nav_=Stats();input_=Stats();return w;}
private:
  static void add(Stats &a,uint32_t us){
    ++a.count;a.totalUs+=us;if(us>a.maxUs)a.maxUs=us;
    const uint32_t upper[]={2000,4000,8000,16000,33000,50000,100000};
    unsigned i=0;while(i<7&&us>upper[i])++i;++a.bucket[i];
  }
  Stats game_,nav_,input_;
};
#if defined(VQEAF_PERF_DIAG)
extern UiFrameMetrics vqeafFrameMetrics;
#endif
