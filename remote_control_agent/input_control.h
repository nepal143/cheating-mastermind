// ===== input_control.h =====
#ifndef INPUT_CONTROL_H
#define INPUT_CONTROL_H

#include <cstdint>

void MoveMouse(int x, int y);
void SimulateMouseClick(); // Legacy function
void SimulateMouseDown(uint32_t mouseFlag);
void SimulateMouseUp(uint32_t mouseFlag);
void SimulateKeyDown(uint16_t keyCode);
void SimulateKeyUp(uint16_t keyCode);
void SimulateScroll(int delta);

#endif // INPUT_CONTROL_H