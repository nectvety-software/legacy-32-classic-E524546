#include "UiPerfCounter.h"
#include <cassert>
#include <cstdio>
#include <limits>
int main(){
 UiPerfCounter p;
 auto empty=p.snapshot();
 assert(empty.samples==0 && empty.meanUs==0 && empty.maxUs==0);
 assert(UiPerfCounter::elapsed(5,std::numeric_limits<uint32_t>::max()-1)==7);
 p.record(4000); p.record(17000); p.record(34000); p.record(120000);
 const auto q=p.snapshot();
 assert(q.samples==4 && q.meanUs==43750 && q.maxUs==120000);
 assert(q.over16ms==3 && q.over33ms==2 && q.over100ms==1);
 auto saved=p.take();
 assert(saved.samples==4 && p.snapshot().samples==0);
 p.record(0); p.record(120);
 assert(p.snapshot().meanUs==60 && p.snapshot().maxUs==120);
 std::puts("PASS: loop latency counters, 16/33/100ms thresholds, wraparound and window reset");
}
