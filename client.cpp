// Simple Remote Desktop Client
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
#include <string>
#include <iomanip>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 9000

struct PasswordAuth {
    char password[32];
};

struct MouseEvent {
    uint8_t type;
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

void SendKeyEvent(uint16_t keyCode, bool keyDown) {
    if (clientSocket == INVALID_SOCKET) return;
    
    uint8_t eventType = 2; // Keyboard event
    struct KeyboardEvent {
        uint8_t type;
        uint16_t keyCode;
        uint32_t flags;
    } keyEvent = {keyDown ? (uint8_t)1 : (uint8_t)2, keyCode, 0};
    
    SendData(clientSocket, &eventType, sizeof(eventType));
    SendData(clientSocket, &keyEvent, sizeof(keyEvent));
}

void ScreenReceiveThread() {
    int frameCount = 0;
    
    std::cout << "Starting to receive screen frames..." << std::endl;
    std::cout << "Press Ctrl+C to stop or wait for 100 frames..." << std::endl;
    
    while (running && frameCount < 100) { // More frames for longer session
        ScreenFrame frameHeader;
        
        std::cout << "Waiting for frame " << (frameCount + 1) << "..." << std::endl;
        
        if (!ReceiveData(clientSocket, &frameHeader, sizeof(frameHeader))) {
            std::cout << "ERROR: Failed to receive frame header. Connection lost!" << std::endl;
            std::cout << "Error code: " << WSAGetLastError() << std::endl;
            break;
        }
        
        std::cout << "Receiving frame data (" << frameHeader.dataSize << " bytes)..." << std::endl;
        
        std::vector<unsigned char> imageData(frameHeader.dataSize);
        if (!ReceiveData(clientSocket, imageData.data(), frameHeader.dataSize)) {
            std::cout << "ERROR: Failed to receive image data. Connection lost!" << std::endl;
            std::cout << "Error code: " << WSAGetLastError() << std::endl;
            break;
        }
        
        // Save frame to file
        std::ostringstream filename;
        filename << "remote_screen_" << std::setfill('0') << std::setw(3) << frameCount++ << ".bmp";
        std::ofstream file(filename.str().c_str(), std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
            file.close();
            std::cout << "SUCCESS: Saved " << filename.str() << " (" << frameHeader.width 
                      << "x" << frameHeader.height << ", " << frameHeader.dataSize << " bytes)" << std::endl;
        } else {
            std::cout << "ERROR: Failed to save frame to file!" << std::endl;
        }
        
        // Send some test mouse movements
        if (frameCount == 5) {
            std::cout << "*** Sending test mouse movements ***" << std::endl;
            SendMouseEvent(1, 100, 100); // Move to 100,100
            Sleep(200);
            SendMouseEvent(1, 300, 300); // Move to 300,300
            Sleep(200);
            std::cout << "*** Mouse movement test completed ***" << std::endl;
        }
        
        if (frameCount == 10) {
            std::cout << "*** Sending test mouse click ***" << std::endl;
            SendMouseEvent(2, 300, 300); // Left down
            Sleep(100);
            SendMouseEvent(3, 300, 300); // Left up
            std::cout << "*** Mouse click test completed ***" << std::endl;
        }
        
        if (frameCount == 15) {
            std::cout << "*** Sending test key press (Windows key) ***" << std::endl;
            SendKeyEvent(VK_LWIN, true);  // Windows key down
            Sleep(50);
            SendKeyEvent(VK_LWIN, false); // Windows key up
            std::cout << "*** Key press test completed ***" << std::endl;
        }
        
        // Add small delay between frames
        Sleep(100);
    }
    
    std::cout << std::endl;
    std::cout << "=== SESSION COMPLETE ===" << std::endl;
    std::cout << "Total frames received: " << frameCount << std::endl;
    std::cout << "Check the saved BMP files to see the remote screen!" << std::endl;
    std::cout << "Files saved as: remote_screen_000.bmp, remote_screen_001.bmp, etc." << std::endl;
    
    running = false;
}

int main(int argc, char* argv[]) {
    std::string serverIP = "127.0.0.1";
    std::string password;
    
    std::cout << "========================================" << std::endl;
    std::cout << "    Remote Desktop Client v1.0" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Get connection details
    if (argc >= 3) {
        serverIP = argv[1];
        password = argv[2];
    } else {
        std::cout << "Enter server IP address [127.0.0.1]: ";
        std::getline(std::cin, serverIP);
        if (serverIP.empty()) serverIP = "127.0.0.1";
        
        std::cout << "Enter password: ";
        std::getline(std::cin, password);
        
        if (password.empty()) {
            std::cout << "Password cannot be empty!" << std::endl;
            return 1;
        }
    }
    
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
    
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << serverIP << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connecting to " << serverIP << ":" << SERVER_PORT << "..." << std::endl;
    
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
        std::cerr << "Make sure the server is running and accessible" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected! Authenticating..." << std::endl;

    // Send authentication
    PasswordAuth auth = {};
    strncpy_s(auth.password, sizeof(auth.password), password.c_str(), _TRUNCATE);
    
    if (!SendData(clientSocket, &auth, sizeof(auth))) {
        std::cerr << "Failed to send authentication" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Receive screen dimensions
    uint32_t screenWidth, screenHeight;
    if (!ReceiveData(clientSocket, &screenWidth, sizeof(screenWidth)) ||
        !ReceiveData(clientSocket, &screenHeight, sizeof(screenHeight))) {
        std::cerr << "Authentication failed or connection error" << std::endl;
        std::cerr << "Check your password and try again" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Authentication successful!" << std::endl;
    std::cout << "Remote screen size: " << screenWidth << "x" << screenHeight << std::endl;
    std::cout << "Starting remote control session..." << std::endl;

    // Start screen receiving thread
    std::thread screenThread(ScreenReceiveThread);

    // Wait for completion
    if (screenThread.joinable()) {
        screenThread.join();
    }

    closesocket(clientSocket);
    WSACleanup();
    
    std::cout << "Disconnected. Check the saved BMP files to see the remote screen!" << std::endl;
    std::cout << "Files will be named: remote_screen_000.bmp, remote_screen_001.bmp, etc." << std::endl;
    std::cout << "You can open these with any image viewer to see what was on the remote screen." << std::endl;
    std::cout << std::endl;
    std::cout << "=== REMOTE CONTROL TEST RESULTS ===" << std::endl;
    std::cout << "- Mouse movements were sent at frame 5" << std::endl;
    std::cout << "- Mouse click was sent at frame 10" << std::endl;
    std::cout << "- Windows key press was sent at frame 15" << std::endl;
    std::cout << "Check if these actions occurred on the remote computer!" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.clear();
    std::cin.ignore(10000, '\n');
    std::cin.get();
    
    return 0;
}
