#include "PixelSnakeLogic.h"
namespace PixelSnake {
static bool same(Point a,Point b){return a.x==b.x && a.y==b.y;}
uint32_t Game::random(){
  uint32_t s=rand_?rand_:0x6d2b79f5u;
  s ^= s << 13; s ^= s >> 17; s ^= s << 5; rand_=s;
  return s;
}
bool Game::occupied(Point p,uint16_t count) const {
  for(uint16_t i=0;i<count;i++)if(same(body_[i],p))return true;
  return false;
}
bool Game::newFood(){
  if(length_>=CAPACITY)return false;
  uint16_t offset=uint16_t(random()%CAPACITY);
  for(uint16_t i=0;i<CAPACITY;i++){
    uint16_t tile=uint16_t((offset+i)%CAPACITY);
    Point candidate={uint8_t(tile%W),uint8_t(tile/W)};
    if(!occupied(candidate,length_)){food_=candidate;return true;}
  }
  return false;
}
void Game::reset(uint32_t seed,bool wrap){
  rand_=seed?seed:0x1234567u;
  wrap_=wrap;length_=4;score_=0;direction_=pending_=Dir::Right;
  turnQueued_=false;phase_=Phase::Ready;
  for(uint8_t i=0;i<4;i++)body_[i]={uint8_t(7-i), uint8_t(9)};
  newFood();
}
void Game::start(){
  if(phase_==Phase::Ready||phase_==Phase::Paused)phase_=Phase::Running;
}
void Game::pause(){if(phase_==Phase::Running)phase_=Phase::Paused;}
bool Game::turn(Dir direction){
  if(phase_!=Phase::Ready&&phase_!=Phase::Running)return false;
  if(turnQueued_)return false; // exactly one queued direction per simulation tick
  if(uint8_t(direction)==((uint8_t(direction_)+2u)&3u))return false;
  pending_=direction;turnQueued_=true;return true;
}
Result Game::step(){
  if(phase_!=Phase::Running)return Result::None;
  direction_=pending_;turnQueued_=false;
  int x=body_[0].x,y=body_[0].y;
  switch(direction_){case Dir::Up:y--;break;case Dir::Right:x++;break;case Dir::Down:y++;break;case Dir::Left:x--;break;}
  if(wrap_){x=(x+W)%W;y=(y+H)%H;}
  else if(x<0||x>=W||y<0||y>=H){phase_=Phase::Lost;return Result::Lost;}
  Point head={uint8_t(x),uint8_t(y)};
  bool eat=same(head,food_);
  // Occupying the old tail is legal on a non-growth tick.
  const uint16_t collisionEnd=eat?length_:uint16_t(length_-1);
  if(occupied(head,collisionEnd)){phase_=Phase::Lost;return Result::Lost;}
  uint16_t nextLen=length_+(eat?1u:0u);
  if(nextLen>CAPACITY){phase_=Phase::Won;return Result::Won;}
  for(uint16_t i=nextLen-1;i>0;i--)body_[i]=body_[i-1];
  body_[0]=head;length_=nextLen;
  if(eat){score_=uint16_t(score_+10);if(!newFood()){phase_=Phase::Won;return Result::Won;}return Result::Ate;}
  return Result::Moved;
}
#ifdef PIXEL_SNAKE_TEST
void Game::testState(const Point *segments,uint16_t count,Dir dir,Point food,bool wrap){
  reset(1337,wrap);if(count<1||count>CAPACITY)return;
  length_=count;for(uint16_t i=0;i<count;i++)body_[i]=segments[i];
  direction_=pending_=dir;food_=food;phase_=Phase::Running;turnQueued_=false;score_=0;
}
#endif
}
