#pragma once
#include "Types.h"

// Short clicks of START, OPTION, A and B are dispatched immediately to apps.
// Never reinterpret a prolonged click on one of these keys as a second,
// unrelated screen transition after a slow SD read/manifest verification.
namespace GlobalShortcutPolicy {
enum class Action : uint8_t { None, TaskSwitcher, ToggleT9 };
inline Action resolve(const KeyEvent &event, bool textEditorActive) {
  if (!event.pressed || !event.longPress) return Action::None;
  if (event.key == Key::Select) return Action::ToggleT9;
  if (event.key == Key::Menu && !textEditorActive) return Action::TaskSwitcher;
  return Action::None;
}
} // namespace GlobalShortcutPolicy
