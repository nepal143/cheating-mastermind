// ===== screen_capture.h =====
#ifndef SCREEN_CAPTURE_H
#define SCREEN_CAPTURE_H

#include <vector>

std::vector<unsigned char> CaptureScreen();
std::vector<unsigned char> CaptureScreenAsBMP();

#endif // SCREEN_CAPTURE_H