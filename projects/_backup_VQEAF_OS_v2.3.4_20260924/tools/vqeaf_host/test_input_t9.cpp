#include <cassert>
#include <iostream>
#include "BoardConfig.h"
#include "core/InputManager.h"
#include "core/TextKeyboard.h"
uint32_t vqeafFakeMillis=0;
int vqeafFakePinState[64];
static void tick(unsigned dt){vqeafFakeMillis+=dt;}
static void key(int pin,bool pressed){vqeafFakePinState[pin]=pressed?LOW:HIGH;}
int main(){
  for(int &p:vqeafFakePinState)p=HIGH;
  InputManager input; input.begin();
  assert(!input.t9Mode());
  key(Board::KEY_SELECT,true);input.poll();tick(30);
  assert(input.poll().key==Key::None); // SELECT deferred on press
  tick(660);KeyEvent e=input.poll();assert(e.key==Key::Select && e.longPress);
  assert(input.t9Mode());
  key(Board::KEY_SELECT,false);input.poll();tick(30);
  assert(input.poll().key==Key::None); // no spurious short press on long-hold release
  input.setTextInputActive(true);
  TextKeyboard keyboard;keyboard.open("URL");
  key(Board::KEY_UP,true);input.poll();tick(30);
  e=input.poll();assert(e.key==Key::Num2);
  assert(keyboard.handle(e));assert(keyboard.value()=="a");
  key(Board::KEY_UP,false);input.poll();tick(30);input.poll();
  tick(40);key(Board::KEY_UP,true);input.poll();tick(30);
  e=input.poll();assert(e.key==Key::Num2);
  assert(keyboard.handle(e));assert(keyboard.value()=="b");
  key(Board::KEY_UP,false);input.poll();tick(30);input.poll();
  // Switching back without an accidental in-editor space/OK action.
  key(Board::KEY_SELECT,true);input.poll();tick(30);input.poll();tick(660);
  e=input.poll();assert(e.longPress&&e.key==Key::Select&&!input.t9Mode());
  key(Board::KEY_SELECT,false);input.poll();tick(30);assert(input.poll().key==Key::None);
  input.setTextInputActive(false);
  key(Board::KEY_MENU,true);input.poll();tick(30);
  e=input.poll();assert(e.key==Key::Menu); // normal Home still works outside editor
  std::cout<<"PASS input: SELECT hold toggle, deferred short press, T9 multitap, Home nav\n";
}
