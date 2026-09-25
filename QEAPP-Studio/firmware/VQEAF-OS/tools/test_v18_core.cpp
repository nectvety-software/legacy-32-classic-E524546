#include "../src/services/CalculatorEngine.h"
#include "../src/services/StopwatchEngine.h"
#include "../src/services/PlaylistNavigator.h"
#include <cassert>
#include <cstring>
#include <cmath>
#include <iostream>
static void key(CalculatorEngine &calc,const char *seq){for(const char*s=seq;*s;++s)calc.press(*s);}
int main(){
  CalculatorEngine c;
  assert(!std::strcmp(c.display(),"0"));
  key(c,"12+3=");assert(!std::strcmp(c.display(),"15"));
  key(c,"C8/0=");assert(c.error());
  key(c,"99=");assert(c.error());
  key(c,"C12.5*2=");assert(!std::strcmp(c.display(),"25"));
  key(c,"C45<6=");assert(!std::strcmp(c.display(),"46"));
  key(c,"C20-5=");assert(!std::strcmp(c.display(),"15"));
  key(c,"C7/2=");assert(!std::strcmp(c.display(),"3.5"));
  key(c,"C999999999999999999999999999999");assert(std::strlen(c.display())<22);
  StopwatchEngine t;
  assert(t.elapsed(100)==0);
  t.toggle(100);assert(t.running()&&t.elapsed(625)==525);
  assert(t.lap(725)&&t.lapCount()==1&&t.lapAt(0)==625);
  t.toggle(1125);assert(!t.running()&&t.elapsed(9900)==1025);
  t.toggle(2000);assert(t.elapsed(2200)==1225);
  assert(t.lap(2300)&&t.lapCount()==2&&t.lapAt(1)==1325);
  t.reset();assert(t.running()&&t.lapCount()==2); // cannot reset running
  t.toggle(2500);t.reset();assert(t.elapsed(9900)==0&&t.lapCount()==0);
  // millis wraparound while recording
  t.toggle(0xFFFFFFF0UL);assert(t.elapsed(0x10UL)==32);
  t.toggle(0x10UL);assert(t.elapsed(0x40UL)==32);
  t.reset();t.toggle(1);
  for(unsigned i=0;i<4;++i)assert(t.lap(100+i));
  assert(!t.lap(200)&&t.lapCount()==4);
  PlaylistNavigator nav;
  assert(nav.next(0,0,true,true,123)==-1);
  assert(nav.next(3,1,true,true,123)==2);
  assert(nav.next(3,2,true,true,123)==-1);
  assert(nav.next(3,0,false,false,1)==2);
  nav.cycleRepeat(); // All
  assert(nav.next(3,2,true,true,123)==0);
  nav.cycleRepeat(); // One
  assert(nav.next(3,2,true,true,123)==2);
  nav.cycleRepeat(); // Off
  nav.toggleShuffle(0);
  int current=0;
  for(int i=0;i<3;++i){
    int next=nav.next(4,current,true,true,i);
    assert(next>=0&&next<4&&next!=current);
    nav.markPlayed(next);current=next;
  }
  assert(nav.next(4,current,true,true,100)==-1); // all visited, Repeat Off
  nav.cycleRepeat(); // All
  assert(nav.next(4,current,true,true,123)>=0); // new shuffle cycle
  std::cout<<"PASS: calculator + stopwatch + bounded playlist repeat/shuffle\n";
}
