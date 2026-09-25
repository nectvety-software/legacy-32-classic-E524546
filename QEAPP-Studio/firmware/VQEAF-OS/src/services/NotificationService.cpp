#include "NotificationService.h"

constexpr int NotificationService::MAX_ITEMS;

void NotificationService::push(const String &title, const String &body) {
  // Newest notification stays at index 0 so the UI can render it immediately.
  const int last = min(used, MAX_ITEMS - 1);
  for (int i = last; i > 0; --i) items[i] = items[i - 1];
  items[0] = SystemNotification(title, body, millis(), false);
  if (used < MAX_ITEMS) ++used;
}

int NotificationService::unreadCount() const {
  int n = 0;
  for (int i = 0; i < used; ++i) if (!items[i].read) ++n;
  return n;
}

const SystemNotification &NotificationService::at(int index) const {
  static SystemNotification empty;
  if (index < 0 || index >= used) return empty;
  return items[index];
}

void NotificationService::markRead(int index) {
  if (index >= 0 && index < used) items[index].read = true;
}

void NotificationService::markAllRead() {
  for (int i = 0; i < used; ++i) items[i].read = true;
}

void NotificationService::remove(int index) {
  if (index < 0 || index >= used) return;
  for (int i = index; i < used - 1; ++i) items[i] = items[i + 1];
  if (used > 0) --used;
}

void NotificationService::clear() {
  used = 0;
}
