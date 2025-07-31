// ===== screen_capture.cpp =====
#include "screen_capture.h"
#include <iostream>

std::vector<unsigned char> CaptureScreen() {
    int screenX = GetSystemMetrics(SM_CXSCREEN);
    int screenY = GetSystemMetrics(SM_CYSCREEN);

    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, screenX, screenY);
    HGDIOBJ hOldBitmap = SelectObject(hDC, hBitmap);
    
    BitBlt(hDC, 0, 0, screenX, screenY, hScreen, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = screenX;
    bi.biHeight = -screenY; // top-down
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    std::vector<unsigned char> pixels(screenX * screenY * 3);
    GetDIBits(hDC, hBitmap, 0, screenY, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // Cleanup
    SelectObject(hDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

    return pixels;
}

std::vector<unsigned char> CaptureScreenAsBMP() {
    int screenX = GetSystemMetrics(SM_CXSCREEN);
    int screenY = GetSystemMetrics(SM_CYSCREEN);

    std::cout << "Screen dimensions: " << screenX << "x" << screenY << std::endl;

    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, screenX, screenY);
    
    if (!hBitmap) {
        std::cerr << "Failed to create compatible bitmap" << std::endl;
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        return {};
    }

    HGDIOBJ hOldBitmap = SelectObject(hDC, hBitmap);
    
    // Capture the screen
    if (!BitBlt(hDC, 0, 0, screenX, screenY, hScreen, 0, 0, SRCCOPY)) {
        std::cerr << "BitBlt failed" << std::endl;
        SelectObject(hDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        return {};
    }

    // Prepare bitmap info
    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = screenX;
    bi.biHeight = screenY; // positive for bottom-up bitmap
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0; // Can be 0 for BI_RGB

    // Calculate sizes
    int rowSize = ((screenX * 3 + 3) / 4) * 4; // Row size must be multiple of 4
    int imageSize = rowSize * screenY;
    int fileSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize;

    // Create the complete BMP data
    std::vector<unsigned char> bmpData(fileSize);
    
    // Fill BMP file header
    BITMAPFILEHEADER* fileHeader = reinterpret_cast<BITMAPFILEHEADER*>(bmpData.data());
    fileHeader->bfType = 0x4D42; // "BM"
    fileHeader->bfSize = fileSize;
    fileHeader->bfReserved1 = 0;
    fileHeader->bfReserved2 = 0;
    fileHeader->bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    // Fill BMP info header
    BITMAPINFOHEADER* infoHeader = reinterpret_cast<BITMAPINFOHEADER*>(bmpData.data() + sizeof(BITMAPFILEHEADER));
    *infoHeader = bi;
    infoHeader->biSizeImage = imageSize;

    // Get the pixel data
    unsigned char* pixelData = bmpData.data() + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    
    if (!GetDIBits(hDC, hBitmap, 0, screenY, pixelData, (BITMAPINFO*)infoHeader, DIB_RGB_COLORS)) {
        std::cerr << "GetDIBits failed" << std::endl;
        SelectObject(hDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        return {};
    }

    // Cleanup
    SelectObject(hDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

    std::cout << "BMP created successfully, size: " << bmpData.size() << " bytes" << std::endl;
    return bmpData;
}
