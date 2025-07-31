// Complete Remote Desktop Server - Fixed Version
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#define WINVER 0x0601

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")

#define PORT 9000
#define FRAME_RATE 10  // Reduced FPS for better performance
#define FRAME_INTERVAL (1000 / FRAME_RATE)

std::atomic<bool> running(true);
std::atomic<SOCKET> clientSocket(INVALID_SOCKET);

// Protocol structures
struct PasswordAuth {
    char password[32];
};

struct MouseEvent {
    uint8_t type;      // 1=move, 2=left_down, 3=left_up, 4=right_down, 5=right_up
    int16_t x;
    int16_t y;
};

struct KeyboardEvent {
    uint8_t type;      // 1=key_down, 2=key_up
    uint16_t keyCode;
    uint32_t flags;
};

struct ScreenFrame {
    uint32_t dataSize;
    uint32_t width;
    uint32_t height;
};

std::string g_serverPassword;

// Generate random password
std::string GeneratePassword() {
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
    
    std::string password;
    for (int i = 0; i < 8; ++i) {
        password += chars[dis(gen)];
    }
    return password;
}

// Helper functions
bool SendData(SOCKET socket, const void* data, int size) {
    if (socket == INVALID_SOCKET) return false;
    
    const char* ptr = static_cast<const char*>(data);
    int totalSent = 0;
    
    while (totalSent < size) {
        int sent = send(socket, ptr + totalSent, size - totalSent, 0);
        if (sent <= 0) {
            return false;
        }
        totalSent += sent;
    }
    return true;
}

bool ReceiveData(SOCKET socket, void* data, int size) {
    if (socket == INVALID_SOCKET) return false;
    
    char* ptr = static_cast<char*>(data);
    int totalReceived = 0;
    
    while (totalReceived < size) {
        int received = recv(socket, ptr + totalReceived, size - totalReceived, 0);
        if (received <= 0) {
            return false;
        }
        totalReceived += received;
    }
    return true;
}

// Screen capture
std::vector<unsigned char> CaptureScreenAsBMP() {
    int screenX = GetSystemMetrics(SM_CXSCREEN);
    int screenY = GetSystemMetrics(SM_CYSCREEN);

    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, screenX, screenY);
    
    if (!hBitmap) {
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        return {};
    }

    HGDIOBJ hOldBitmap = SelectObject(hDC, hBitmap);
    BitBlt(hDC, 0, 0, screenX, screenY, hScreen, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = screenX;
    bi.biHeight = screenY;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    int rowSize = ((screenX * 3 + 3) / 4) * 4;
    int imageSize = rowSize * screenY;
    int fileSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize;

    std::vector<unsigned char> bmpData(fileSize);
    
    BITMAPFILEHEADER* fileHeader = reinterpret_cast<BITMAPFILEHEADER*>(bmpData.data());
    fileHeader->bfType = 0x4D42;
    fileHeader->bfSize = fileSize;
    fileHeader->bfReserved1 = 0;
    fileHeader->bfReserved2 = 0;
    fileHeader->bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    BITMAPINFOHEADER* infoHeader = reinterpret_cast<BITMAPINFOHEADER*>(bmpData.data() + sizeof(BITMAPFILEHEADER));
    *infoHeader = bi;
    infoHeader->biSizeImage = imageSize;

    unsigned char* pixelData = bmpData.data() + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    
    if (!GetDIBits(hDC, hBitmap, 0, screenY, pixelData, (BITMAPINFO*)infoHeader, DIB_RGB_COLORS)) {
        SelectObject(hDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        return {};
    }

    SelectObject(hDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

    return bmpData;
}

// Input simulation
void MoveMouse(int x, int y) {
    SetCursorPos(x, y);
}

void SimulateMouseDown(uint32_t mouseFlag) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = mouseFlag;
    SendInput(1, &input, sizeof(INPUT));
}

void SimulateMouseUp(uint32_t mouseFlag) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = mouseFlag;
    SendInput(1, &input, sizeof(INPUT));
}

void SimulateKeyDown(uint16_t keyCode) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = 0;
    SendInput(1, &input, sizeof(INPUT));
}

void SimulateKeyUp(uint16_t keyCode) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

// Screen streaming thread
void ScreenStreamingThread() {
    std::cout << "Screen streaming thread started" << std::endl;
    
    auto lastFrameTime = std::chrono::steady_clock::now();
    
    while (running) {
        SOCKET currentClient = clientSocket.load();
        if (currentClient == INVALID_SOCKET) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastFrameTime).count();
            
        if (elapsed >= FRAME_INTERVAL) {
            auto bmpData = CaptureScreenAsBMP();
            if (!bmpData.empty()) {
                ScreenFrame frameHeader;
                frameHeader.dataSize = static_cast<uint32_t>(bmpData.size());
                frameHeader.width = GetSystemMetrics(SM_CXSCREEN);
                frameHeader.height = GetSystemMetrics(SM_CYSCREEN);
                
                if (!SendData(currentClient, &frameHeader, sizeof(frameHeader)) ||
                    !SendData(currentClient, bmpData.data(), bmpData.size())) {
                    std::cout << "Failed to send frame, client disconnected" << std::endl;
                    clientSocket.store(INVALID_SOCKET);
                }
            }
            
            lastFrameTime = currentTime;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "Screen streaming thread ended" << std::endl;
}

// Input handling thread
void InputHandlingThread() {
    std::cout << "Input handling thread started" << std::endl;
    
    while (running) {
        SOCKET currentClient = clientSocket.load();
        if (currentClient == INVALID_SOCKET) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(currentClient, &readSet);
        
        timeval timeout = {0, 10000}; // 10ms timeout
        int result = select(0, &readSet, nullptr, nullptr, &timeout);
        
        if (result > 0 && FD_ISSET(currentClient, &readSet)) {
            uint8_t eventType;
            if (!ReceiveData(currentClient, &eventType, sizeof(eventType))) {
                std::cout << "Client disconnected" << std::endl;
                clientSocket.store(INVALID_SOCKET);
                continue;
            }
            
            if (eventType == 1) { // Mouse event
                MouseEvent mouseEvent;
                if (ReceiveData(currentClient, &mouseEvent, sizeof(mouseEvent))) {
                    switch (mouseEvent.type) {
                        case 1: // Mouse move
                            MoveMouse(mouseEvent.x, mouseEvent.y);
                            break;
                        case 2: // Left button down
                            SimulateMouseDown(MOUSEEVENTF_LEFTDOWN);
                            break;
                        case 3: // Left button up
                            SimulateMouseUp(MOUSEEVENTF_LEFTUP);
                            break;
                        case 4: // Right button down
                            SimulateMouseDown(MOUSEEVENTF_RIGHTDOWN);
                            break;
                        case 5: // Right button up
                            SimulateMouseUp(MOUSEEVENTF_RIGHTUP);
                            break;
                    }
                }
            }
            else if (eventType == 2) { // Keyboard event
                KeyboardEvent keyEvent;
                if (ReceiveData(currentClient, &keyEvent, sizeof(keyEvent))) {
                    if (keyEvent.type == 1) {
                        SimulateKeyDown(keyEvent.keyCode);
                    } else if (keyEvent.type == 2) {
                        SimulateKeyUp(keyEvent.keyCode);
                    }
                }
            }
        }
        else if (result == SOCKET_ERROR) {
            std::cout << "Select error, client disconnected" << std::endl;
            clientSocket.store(INVALID_SOCKET);
        }
    }
    
    std::cout << "Input handling thread ended" << std::endl;
}

void HandleClient(SOCKET socket) {
    std::cout << "Client attempting connection..." << std::endl;
    
    // Receive authentication
    PasswordAuth auth;
    if (!ReceiveData(socket, &auth, sizeof(auth))) {
        std::cout << "Failed to receive authentication" << std::endl;
        closesocket(socket);
        return;
    }
    
    // Check password
    if (g_serverPassword != std::string(auth.password)) {
        std::cout << "Authentication failed - wrong password: " << auth.password << std::endl;
        closesocket(socket);
        return;
    }
    
    std::cout << "Client authenticated successfully!" << std::endl;
    
    // Set the global client socket
    clientSocket.store(socket);
    
    // Send initial screen dimensions
    uint32_t width = GetSystemMetrics(SM_CXSCREEN);
    uint32_t height = GetSystemMetrics(SM_CYSCREEN);
    
    if (!SendData(socket, &width, sizeof(width)) ||
        !SendData(socket, &height, sizeof(height))) {
        std::cout << "Failed to send initial screen info" << std::endl;
        clientSocket.store(INVALID_SOCKET);
        closesocket(socket);
        return;
    }
    
    std::cout << "Sent screen dimensions: " << width << "x" << height << std::endl;
    std::cout << "Remote control session active! Press Ctrl+C to stop." << std::endl;
    
    // Wait for client to disconnect
    char keepAlive;
    while (running && clientSocket.load() != INVALID_SOCKET) {
        int result = recv(socket, &keepAlive, 1, MSG_PEEK);
        if (result <= 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Client disconnected" << std::endl;
    clientSocket.store(INVALID_SOCKET);
    closesocket(socket);
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "    Remote Desktop Server v1.0" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Generate and display password
    g_serverPassword = GeneratePassword();
    std::cout << std::endl;
    std::cout << "SERVER PASSWORD: " << g_serverPassword << std::endl;
    std::cout << "Share this password with the remote user" << std::endl;
    std::cout << std::endl;
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port " << PORT << "..." << std::endl;
    
    // Start background threads
    std::thread screenThread(ScreenStreamingThread);
    std::thread inputThread(InputHandlingThread);
    
    std::cout << "Waiting for remote connection..." << std::endl;

    while (running) {
        SOCKET newClientSocket = accept(serverSocket, NULL, NULL);
        if (newClientSocket == INVALID_SOCKET) {
            if (running) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }
        
        HandleClient(newClientSocket);
    }

    // Cleanup
    running = false;
    if (screenThread.joinable()) screenThread.join();
    if (inputThread.joinable()) inputThread.join();
    
    closesocket(serverSocket);
    WSACleanup();
    std::cout << "Server stopped" << std::endl;
    return 0;
}
