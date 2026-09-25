#pragma once
#include <Arduino.h>

struct SystemNotification {
  String title;
  String body;
  uint32_t whenMs;
  bool read;

  SystemNotification() : title(), body(), whenMs(0), read(false) {}
  SystemNotification(const String &t, const String &b, uint32_t ms, bool isRead = false)
      : title(t), body(b), whenMs(ms), read(isRead) {}
};

class NotificationService {
public:
  static constexpr int MAX_ITEMS = 8;

  void push(const String &title, const String &body);
  int count() const { return used; }
  int unreadCount() const;
  const SystemNotification &at(int index) const;
  void markRead(int index);
  void markAllRead();
  void remove(int index);
  void clear();

private:
  SystemNotification items[MAX_ITEMS];
  int used = 0;
};
