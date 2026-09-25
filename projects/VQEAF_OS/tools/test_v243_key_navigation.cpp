// Reproduce the field video: START selected an app/theme, the operation
// took >650ms, then the held START erroneously jumped to Music.
// Fake GPIO/clock tests use the REAL InputManager and new shared policy.
#include <cassert>
#include <iostream>
#include <algorithm>
#define VQEAF_INPUT_FAKE_CLOCK 1
#include "../src/core/InputManager.h"
#include "../src/core/GlobalShortcutPolicy.h"
#include "BoardConfig.h"
uint32_t vqeafFakeMillis=0;
int vqeafFakePinState[64];

using GlobalShortcutPolicy::Action;
static void advance(uint32_t ms){ vqeafFakeMillis+=ms; }
static void down(int pin){vqeafFakePinState[pin]=LOW;}
static void up(int pin){vqeafFakePinState[pin]=HIGH;}
static KeyEvent next(InputManager &in,uint32_t elapsed){advance(elapsed);return in.poll();}
int main(){
  std::fill(vqeafFakePinState,vqeafFakePinState+64,HIGH);
  InputManager in;in.begin();
  down(Board::KEY_START);
  assert(next(in,2).key==Key::None);
  KeyEvent start=next(in,30);
  assert(start.key==Key::Start && start.pressed && !start.longPress);
  // While the loader verifies the file/SD, the user still holds START.
  // No second key event is delivered after a prolonged operation.
  KeyEvent afterOpening=next(in,1200);
  assert(afterOpening.key==Key::None);
  // Even a legacy injected long START may not trigger any global shortcut.
  const KeyEvent injectedStart(Key::Start,true,true,false);
  assert(GlobalShortcutPolicy::resolve(injectedStart,false)==Action::None);
  up(Board::KEY_START);assert(next(in,30).key==Key::None);

  // Holding OPTION while waiting for the file menu must not open Settings;
  // A/B holding may not open Recovery/Lock either.
  for(Key key:{Key::Option,Key::A,Key::B}){
    assert(GlobalShortcutPolicy::resolve(KeyEvent(key,true,true,false),false)==Action::None);
  }

  // MENU remains an intentional release-vs-hold gesture.
  down(Board::KEY_MENU);assert(next(in,1).key==Key::None);
  assert(next(in,30).key==Key::None); // no launcher on initial key-down
  KeyEvent longMenu=next(in,670);
  assert(longMenu.key==Key::Menu && longMenu.longPress);
  assert(GlobalShortcutPolicy::resolve(longMenu,false)==Action::TaskSwitcher);
  assert(GlobalShortcutPolicy::resolve(longMenu,true)==Action::None);
  up(Board::KEY_MENU);assert(next(in,1).key==Key::None);
  assert(next(in,30).key==Key::None); // long hold must not also click launcher
  down(Board::KEY_MENU);assert(next(in,1).key==Key::None);
  assert(next(in,30).key==Key::None);
  up(Board::KEY_MENU);assert(next(in,1).key==Key::None);
  KeyEvent shortMenu=next(in,30);
  assert(shortMenu.key==Key::Menu && shortMenu.pressed && !shortMenu.longPress);

  // SELECT always has priority for its Game/T9 mode switch.
  down(Board::KEY_SELECT);assert(next(in,1).key==Key::None);
  assert(next(in,30).key==Key::None);
  KeyEvent mode=next(in,660);
  assert(mode.key==Key::Select && mode.longPress && in.t9Mode());
  assert(GlobalShortcutPolicy::resolve(mode,true)==Action::ToggleT9);
  up(Board::KEY_SELECT);assert(next(in,30).key==Key::None);
  std::cout << "PASS v2.4.3: START held across slow install/theme, no Music hijack; "
            << "OPTION/A/B guarded; MENU and SELECT release/long exclusivity\n";
}
