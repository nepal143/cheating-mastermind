// ===== input_control.cpp =====
#include "input_control.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void MoveMouse(int x, int y) {
    SetCursorPos(x, y);
}

void SimulateMouseClick() {
    INPUT input[2] = {};

    input[0].type = INPUT_MOUSE;
    input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    input[1].type = INPUT_MOUSE;
    input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    SendInput(2, input, sizeof(INPUT));
}