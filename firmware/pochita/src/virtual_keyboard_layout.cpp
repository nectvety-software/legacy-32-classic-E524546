#include "virtual_keyboard.h"

// Keyboard layout definition
const char VirtualKeyboard::keyLayout[KEY_ROWS][KEY_COLS] = {
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'},
    {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P'},
    {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '\b'},
    {'Z', 'X', 'C', 'V', 'B', 'N', 'M', '.', '-', '\177'},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', '\n', '\r', ' '}
};