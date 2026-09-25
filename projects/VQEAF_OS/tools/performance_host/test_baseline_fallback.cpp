#include "VqeafIconRenderer.h"
#include <TFT_eSPI.h>
#include <cassert>
int main(){TFT_eSPI t; assert(VqeafIcons::drawOpaque(t,VqeafIcons::Id::WiFi,0,0,36,0xFFFF));}
