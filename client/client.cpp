// ===== client.cpp =====
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <fstream>
#include <vector>
#include <cstdint>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 8888

struct MouseEvent {
    uint8_t type;      // 1=move, 2=left_down, 3=left_up, 4=right_down, 5=right_up
    int16_t x;
    int16_t y;
};

struct ScreenFrame {
    uint32_t dataSize;
    uint32_t width;
    uint32_t height;
};

std::atomic<bool> running(true);
SOCKET clientSocket = INVALID_SOCKET;

// Helper function to convert int to string (C++98 compatible)
std::string IntToString(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

bool SendData(SOCKET socket, const void* data, int size) {
    const char* ptr = static_cast<const char*>(data);
    int totalSent = 0;
    
    while (totalSent < size) {
        int sent = send(socket, ptr + totalSent, size - totalSent, 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }
    return true;
}

bool ReceiveData(SOCKET socket, void* data, int size) {
    char* ptr = static_cast<char*>(data);
    int totalReceived = 0;
    
    while (totalReceived < size) {
        int received = recv(socket, ptr + totalReceived, size - totalReceived, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
}

void SendMouseEvent(uint8_t type, int16_t x = 0, int16_t y = 0) {
    if (clientSocket == INVALID_SOCKET) return;
    
    uint8_t eventType = 1; // Mouse event
    MouseEvent mouseEvent = {type, x, y};
    
    SendData(clientSocket, &eventType, sizeof(eventType));
    SendData(clientSocket, &mouseEvent, sizeof(mouseEvent));
}

void ScreenReceiveThread() {
    int frameCount = 0;
    
    while (running) {
        ScreenFrame frameHeader;
        
        if (!ReceiveData(clientSocket, &frameHeader, sizeof(frameHeader))) {
            std::cout << "Failed to receive frame header" << std::endl;
            break;
        }
        
        std::vector<unsigned char> imageData(frameHeader.dataSize);
        if (!ReceiveData(clientSocket, imageData.data(), frameHeader.dataSize)) {
            std::cout << "Failed to receive image data" << std::endl;
            break;
        }
        
        // Save frame to file (for demonstration)
        std::string filename = "frame_" + IntToString(frameCount++) + ".bmp";
        std::ofstream file(filename.c_str(), std::ios::binary);
        file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
        file.close();
        
        std::cout << "Received frame " << frameCount << " (" << frameHeader.width 
                  << "x" << frameHeader.height << ", " << frameHeader.dataSize << " bytes)" << std::endl;
        
        // In a real client, you would display this image instead of saving it
        
        if (frameCount >= 10) { // Stop after 10 frames for demo
            running = false;
            break;
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    
    // Connect to localhost (change IP for remote connection)
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to remote desktop server!" << std::endl;

    // Receive initial screen dimensions
    uint32_t screenWidth, screenHeight;
    if (!ReceiveData(clientSocket, &screenWidth, sizeof(screenWidth)) ||
        !ReceiveData(clientSocket, &screenHeight, sizeof(screenHeight))) {
        std::cerr << "Failed to receive screen dimensions" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Remote screen size: " << screenWidth << "x" << screenHeight << std::endl;

    // Start screen receiving thread
    std::thread screenThread(ScreenReceiveThread);

    // Simulate some mouse movements and clicks
    Sleep(1000); // Use Sleep instead of std::this_thread::sleep_for for compatibility
    
    std::cout << "Sending mouse movements..." << std::endl;
    SendMouseEvent(1, 100, 100); // Move to (100,100)
    Sleep(500);
    
    SendMouseEvent(1, 200, 200); // Move to (200,200)
    Sleep(500);
    
    std::cout << "Sending mouse click..." << std::endl;
    SendMouseEvent(2, 200, 200); // Left button down
    Sleep(100);
    SendMouseEvent(3, 200, 200); // Left button up
    
    // Wait for screen thread to finish
    if (screenThread.joinable()) {
        screenThread.join();
    }

    closesocket(clientSocket);
    WSACleanup();
    
    std::cout << "Client disconnected. Check the saved BMP files!" << std::endl;
    return 0;
}