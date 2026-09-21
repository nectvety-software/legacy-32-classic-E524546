#ifndef NOTES_APP_H
#define NOTES_APP_H
#include "../core/AppManager.h"

class NotesApp : public App {
public:
    const char* getName() override { return "Notes"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;
private:
    void draw();
    char notes[256] = "Type here...";
    int cursorPos = 0;
};

#endif
