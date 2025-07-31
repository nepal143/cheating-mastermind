// Debug Server with Maximum Logging
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
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")

#define PORT 9000

std::atomic<bool> running(true);
std::string g_serverPassword;

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

void ShowNetworkInfo() {
    std::cout << std::endl;
    std::cout << "=== NETWORK CONFIGURATION ===" << std::endl;
    
    // Get computer name
    char computerName[256];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        std::cout << "Computer Name: " << computerName << std::endl;
    }
    
    // Show all IP addresses
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        std::cout << "Hostname: " << hostname << std::endl;
        
        struct hostent* host = gethostbyname(hostname);
        if (host != NULL) {
            std::cout << "Available IP Addresses:" << std::endl;
            for (int i = 0; host->h_addr_list[i] != NULL; ++i) {
                struct in_addr addr;
                memcpy(&addr, host->h_addr_list[i], sizeof(struct in_addr));
                std::cout << "  IP " << (i+1) << ": " << inet_ntoa(addr) << std::endl;
            }
        }
    }
    
    std::cout << "Port: " << PORT << std::endl;
    std::cout << "Protocol: TCP" << std::endl;
    std::cout << std::endl;
    std::cout << "CLIENT CONNECTION COMMAND:" << std::endl;
    std::cout << "Use one of the IP addresses above when connecting from remote computer" << std::endl;
    std::cout << "For same computer testing, use: 127.0.0.1" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << std::endl;
}

bool SendData(SOCKET socket, const void* data, int size) {
    if (socket == INVALID_SOCKET) return false;
    
    const char* ptr = static_cast<const char*>(data);
    int totalSent = 0;
    
    while (totalSent < size) {
        int sent = send(socket, ptr + totalSent, size - totalSent, 0);
        if (sent <= 0) {
            std::cout << "ERROR: Send failed. Error: " << WSAGetLastError() << std::endl;
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
            std::cout << "ERROR: Receive failed. Error: " << WSAGetLastError() << std::endl;
            return false;
        }
        totalReceived += received;
    }
    return true;
}

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

struct PasswordAuth {
    char password[32];
};

struct MouseEvent {
    uint8_t type;
    int16_t x;
    int16_t y;
};

struct KeyboardEvent {
    uint8_t type;
    uint16_t keyCode;
    uint32_t flags;
};

struct ScreenFrame {
    uint32_t dataSize;
    uint32_t width;
    uint32_t height;
};

void HandleClient(SOCKET socket) {
    std::cout << std::endl;
    std::cout << "🎉 CLIENT CONNECTED! 🎉" << std::endl;
    std::cout << "Socket: " << socket << std::endl;
    
    // Get client info
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    if (getpeername(socket, (sockaddr*)&clientAddr, &clientAddrLen) == 0) {
        std::cout << "Client IP: " << inet_ntoa(clientAddr.sin_addr) << std::endl;
        std::cout << "Client Port: " << ntohs(clientAddr.sin_port) << std::endl;
    }
    
    std::cout << "Waiting for authentication..." << std::endl;
    
    // Receive authentication
    PasswordAuth auth;
    std::cout << "Reading " << sizeof(auth) << " bytes for authentication..." << std::endl;
    
    if (!ReceiveData(socket, &auth, sizeof(auth))) {
        std::cout << "❌ FAILED to receive authentication data" << std::endl;
        std::cout << "Error code: " << WSAGetLastError() << std::endl;
        closesocket(socket);
        return;
    }
    
    std::cout << "✅ Received authentication data" << std::endl;
    std::cout << "Client password: '" << auth.password << "'" << std::endl;
    std::cout << "Expected password: '" << g_serverPassword << "'" << std::endl;
    
    // Check password
    if (g_serverPassword != std::string(auth.password)) {
        std::cout << "❌ AUTHENTICATION FAILED - Wrong password!" << std::endl;
        closesocket(socket);
        return;
    }
    
    std::cout << "✅ AUTHENTICATION SUCCESSFUL!" << std::endl;
    
    // Send screen dimensions
    uint32_t width = GetSystemMetrics(SM_CXSCREEN);
    uint32_t height = GetSystemMetrics(SM_CYSCREEN);
    
    std::cout << "Sending screen dimensions: " << width << "x" << height << std::endl;
    
    if (!SendData(socket, &width, sizeof(width)) ||
        !SendData(socket, &height, sizeof(height))) {
        std::cout << "❌ Failed to send screen dimensions" << std::endl;
        closesocket(socket);
        return;
    }
    
    std::cout << "✅ Screen dimensions sent successfully" << std::endl;
    std::cout << std::endl;
    std::cout << "🚀 REMOTE CONTROL SESSION ACTIVE!" << std::endl;
    std::cout << "The client can now see and control this computer." << std::endl;
    std::cout << "Press Ctrl+C to stop the server." << std::endl;
    std::cout << "=====================================\n" << std::endl;
    
    int frameCount = 0;
    auto lastFrameTime = std::chrono::steady_clock::now();
    const int FRAME_INTERVAL = 1000; // 1 second between frames
    
    while (running) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFrameTime).count();
        
        if (elapsed >= FRAME_INTERVAL) {
            // Capture and send screen
            auto bmpData = CaptureScreenAsBMP();
            if (!bmpData.empty()) {
                ScreenFrame frameHeader;
                frameHeader.dataSize = static_cast<uint32_t>(bmpData.size());
                frameHeader.width = width;
                frameHeader.height = height;
                
                if (!SendData(socket, &frameHeader, sizeof(frameHeader)) ||
                    !SendData(socket, bmpData.data(), bmpData.size())) {
                    std::cout << "❌ Failed to send frame " << frameCount << ". Client disconnected." << std::endl;
                    break;
                }
                
                frameCount++;
                std::cout << "📺 Sent frame " << frameCount << " (" << bmpData.size() << " bytes)" << std::endl;
            }
            
            lastFrameTime = currentTime;
        }
        
        // Check for input events
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket, &readSet);
        
        timeval timeout = {0, 10000}; // 10ms
        int result = select(0, &readSet, nullptr, nullptr, &timeout);
        
        if (result > 0 && FD_ISSET(socket, &readSet)) {
            uint8_t eventType;
            if (ReceiveData(socket, &eventType, sizeof(eventType))) {
                if (eventType == 1) { // Mouse event
                    MouseEvent mouseEvent;
                    if (ReceiveData(socket, &mouseEvent, sizeof(mouseEvent))) {
                        std::cout << "🖱️ Mouse event: type=" << (int)mouseEvent.type << " x=" << mouseEvent.x << " y=" << mouseEvent.y << std::endl;
                        switch (mouseEvent.type) {
                            case 1: MoveMouse(mouseEvent.x, mouseEvent.y); break;
                            case 2: SimulateMouseDown(MOUSEEVENTF_LEFTDOWN); break;
                            case 3: SimulateMouseUp(MOUSEEVENTF_LEFTUP); break;
                            case 4: SimulateMouseDown(MOUSEEVENTF_RIGHTDOWN); break;
                            case 5: SimulateMouseUp(MOUSEEVENTF_RIGHTUP); break;
                        }
                    }
                } else if (eventType == 2) { // Keyboard event
                    KeyboardEvent keyEvent;
                    if (ReceiveData(socket, &keyEvent, sizeof(keyEvent))) {
                        std::cout << "⌨️ Keyboard event: type=" << (int)keyEvent.type << " key=" << keyEvent.keyCode << std::endl;
                        if (keyEvent.type == 1) {
                            SimulateKeyDown(keyEvent.keyCode);
                        } else if (keyEvent.type == 2) {
                            SimulateKeyUp(keyEvent.keyCode);
                        }
                    }
                }
            }
        } else if (result == SOCKET_ERROR) {
            std::cout << "❌ Socket error. Client disconnected." << std::endl;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << std::endl;
    std::cout << "=== SESSION ENDED ===" << std::endl;
    std::cout << "Total frames sent: " << frameCount << std::endl;
    std::cout << "Client disconnected" << std::endl;
    closesocket(socket);
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "    DEBUG REMOTE DESKTOP SERVER" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "Initializing Winsock..." << std::endl;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "❌ WSAStartup failed. Error: " << WSAGetLastError() << std::endl;
        return 1;
    }
    std::cout << "✅ Winsock initialized" << std::endl;

    std::cout << "Creating server socket..." << std::endl;
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "❌ Socket creation failed. Error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }
    std::cout << "✅ Server socket created: " << serverSocket << std::endl;

    // Enable socket reuse
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        std::cout << "⚠️ Warning: Could not set SO_REUSEADDR. Error: " << WSAGetLastError() << std::endl;
    } else {
        std::cout << "✅ Socket reuse enabled" << std::endl;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    std::cout << "Binding to port " << PORT << "..." << std::endl;
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        std::cerr << "❌ Bind failed. Error: " << error << std::endl;
        if (error == WSAEADDRINUSE) {
            std::cerr << "Port " << PORT << " is already in use!" << std::endl;
            std::cerr << "Try closing any other instances of this server." << std::endl;
        }
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "✅ Socket bound to port " << PORT << std::endl;

    std::cout << "Starting to listen..." << std::endl;
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "❌ Listen failed. Error: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "✅ Server listening" << std::endl;

    // Generate and display password
    g_serverPassword = GeneratePassword();
    
    ShowNetworkInfo();
    
    std::cout << "🔑 SERVER PASSWORD: " << g_serverPassword << std::endl;
    std::cout << "📤 Share this password with the remote user" << std::endl;
    std::cout << std::endl;
    std::cout << "⏳ Waiting for client connections..." << std::endl;
    std::cout << "   (Server will show detailed logs when client connects)" << std::endl;
    std::cout << std::endl;

    int connectionCount = 0;
    while (running) {
        std::cout << "👂 Listening for connections... (attempt " << (connectionCount + 1) << ")" << std::endl;
        
        SOCKET newClientSocket = accept(serverSocket, NULL, NULL);
        if (newClientSocket == INVALID_SOCKET) {
            int error = WSAGetLastError();
            if (running) {
                std::cerr << "❌ Accept failed. Error: " << error << std::endl;
            }
            continue;
        }
        
        connectionCount++;
        std::cout << "📞 New connection received! (Connection #" << connectionCount << ")" << std::endl;
        
        HandleClient(newClientSocket);
        
        std::cout << std::endl;
        std::cout << "⏳ Ready for next connection..." << std::endl;
    }

    // Cleanup
    std::cout << "Shutting down server..." << std::endl;
    closesocket(serverSocket);
    WSACleanup();
    std::cout << "Server stopped" << std::endl;
    return 0;
}
