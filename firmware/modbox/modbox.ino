#include "modbox_main.h"

void setup() {
    kernelInit();
    drawSplash();
    drawMainUI();
}

void loop() {
    systemLoop();
}
