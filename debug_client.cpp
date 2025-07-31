// Debug Client with Maximum Logging
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

void ShowNetworkDiagnostics() {
    std::cout << std::endl;
    std::cout << "=== NETWORK DIAGNOSTICS ===" << std::endl;
    
    // Show local computer info
    char computerName[256];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        std::cout << "Local Computer: " << computerName << std::endl;
    }
    
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        std::cout << "Local Hostname: " << hostname << std::endl;
        
        struct hostent* host = gethostbyname(hostname);
        if (host != NULL) {
            std::cout << "Local IP Addresses:" << std::endl;
            for (int i = 0; host->h_addr_list[i] != NULL; ++i) {
                struct in_addr addr;
                memcpy(&addr, host->h_addr_list[i], sizeof(struct in_addr));
                std::cout << "  " << inet_ntoa(addr) << std::endl;
            }
        }
    }
    std::cout << "===============================" << std::endl;
    std::cout << std::endl;
}

bool SendData(SOCKET socket, const void* data, int size) {
    if (socket == INVALID_SOCKET) {
        std::cout << "❌ ERROR: Socket is invalid" << std::endl;
        return false;
    }
    
    const char* ptr = static_cast<const char*>(data);
    int totalSent = 0;
    
    std::cout << "📤 Sending " << size << " bytes..." << std::endl;
    
    while (totalSent < size) {
        int sent = send(socket, ptr + totalSent, size - totalSent, 0);
        if (sent <= 0) {
            int error = WSAGetLastError();
            std::cout << "❌ Send failed. Sent: " << sent << ", Error: " << error << std::endl;
            return false;
        }
        totalSent += sent;
        std::cout << "   Sent " << sent << " bytes (total: " << totalSent << "/" << size << ")" << std::endl;
    }
    
    std::cout << "✅ Send completed: " << totalSent << " bytes" << std::endl;
    return true;
}

bool ReceiveData(SOCKET socket, void* data, int size) {
    if (socket == INVALID_SOCKET) {
        std::cout << "❌ ERROR: Socket is invalid" << std::endl;
        return false;
    }
    
    char* ptr = static_cast<char*>(data);
    int totalReceived = 0;
    
    std::cout << "📥 Receiving " << size << " bytes..." << std::endl;
    
    while (totalReceived < size) {
        int received = recv(socket, ptr + totalReceived, size - totalReceived, 0);
        if (received <= 0) {
            int error = WSAGetLastError();
            std::cout << "❌ Receive failed. Received: " << received << ", Error: " << error << std::endl;
            return false;
        }
        totalReceived += received;
        std::cout << "   Received " << received << " bytes (total: " << totalReceived << "/" << size << ")" << std::endl;
    }
    
    std::cout << "✅ Receive completed: " << totalReceived << " bytes" << std::endl;
    return true;
}

void SendMouseEvent(uint8_t type, int16_t x = 0, int16_t y = 0) {
    if (clientSocket == INVALID_SOCKET) return;
    
    uint8_t eventType = 1; // Mouse event
    MouseEvent mouseEvent = {type, x, y};
    
    std::cout << "🖱️ Sending mouse event: type=" << (int)type << " x=" << x << " y=" << y << std::endl;
    
    if (!SendData(clientSocket, &eventType, sizeof(eventType)) ||
        !SendData(clientSocket, &mouseEvent, sizeof(mouseEvent))) {
        std::cout << "❌ Failed to send mouse event" << std::endl;
    } else {
        std::cout << "✅ Mouse event sent successfully" << std::endl;
    }
}

void SendKeyEvent(uint16_t keyCode, bool keyDown) {
    if (clientSocket == INVALID_SOCKET) return;
    
    uint8_t eventType = 2; // Keyboard event
    struct KeyboardEvent {
        uint8_t type;
        uint16_t keyCode;
        uint32_t flags;
    } keyEvent = {keyDown ? (uint8_t)1 : (uint8_t)2, keyCode, 0};
    
    std::cout << "⌨️ Sending key event: " << (keyDown ? "DOWN" : "UP") << " key=" << keyCode << std::endl;
    
    if (!SendData(clientSocket, &eventType, sizeof(eventType)) ||
        !SendData(clientSocket, &keyEvent, sizeof(keyEvent))) {
        std::cout << "❌ Failed to send key event" << std::endl;
    } else {
        std::cout << "✅ Key event sent successfully" << std::endl;
    }
}

void ScreenReceiveThread() {
    int frameCount = 0;
    
    std::cout << std::endl;
    std::cout << "🎬 STARTING SCREEN CAPTURE SESSION" << std::endl;
    std::cout << "Will receive up to 20 frames for testing..." << std::endl;
    std::cout << std::endl;
    
    while (running && frameCount < 20) { // Reduced to 20 frames for faster testing
        ScreenFrame frameHeader;
        
        std::cout << "⏳ Waiting for frame " << (frameCount + 1) << "..." << std::endl;
        
        if (!ReceiveData(clientSocket, &frameHeader, sizeof(frameHeader))) {
            std::cout << "❌ CRITICAL: Failed to receive frame header. Connection lost!" << std::endl;
            std::cout << "Error code: " << WSAGetLastError() << std::endl;
            break;
        }
        
        std::cout << "📋 Frame header received:" << std::endl;
        std::cout << "   Data size: " << frameHeader.dataSize << " bytes" << std::endl;
        std::cout << "   Dimensions: " << frameHeader.width << "x" << frameHeader.height << std::endl;
        
        if (frameHeader.dataSize > 10000000) { // 10MB safety check
            std::cout << "❌ ERROR: Frame size too large (" << frameHeader.dataSize << " bytes). Aborting!" << std::endl;
            break;
        }
        
        std::vector<unsigned char> imageData(frameHeader.dataSize);
        if (!ReceiveData(clientSocket, imageData.data(), frameHeader.dataSize)) {
            std::cout << "❌ CRITICAL: Failed to receive image data. Connection lost!" << std::endl;
            std::cout << "Error code: " << WSAGetLastError() << std::endl;
            break;
        }
        
        // Save frame to file
        std::ostringstream filename;
        filename << "debug_screen_" << std::setfill('0') << std::setw(3) << frameCount << ".bmp";
        std::ofstream file(filename.str().c_str(), std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
            file.close();
            std::cout << "✅ SUCCESS: Saved " << filename.str() << std::endl;
            std::cout << "   File size: " << imageData.size() << " bytes" << std::endl;
        } else {
            std::cout << "❌ ERROR: Failed to save frame to file!" << std::endl;
        }
        
        frameCount++;
        
        // Send test commands at specific frames
        if (frameCount == 3) {
            std::cout << std::endl;
            std::cout << "🎯 SENDING TEST MOUSE MOVEMENTS" << std::endl;
            SendMouseEvent(1, 100, 100); // Move to 100,100
            Sleep(100);
            SendMouseEvent(1, 300, 300); // Move to 300,300
            Sleep(100);
            std::cout << "✅ Mouse movement test completed" << std::endl;
            std::cout << std::endl;
        }
        
        if (frameCount == 6) {
            std::cout << std::endl;
            std::cout << "🎯 SENDING TEST MOUSE CLICK" << std::endl;
            SendMouseEvent(2, 300, 300); // Left down
            Sleep(50);
            SendMouseEvent(3, 300, 300); // Left up
            std::cout << "✅ Mouse click test completed" << std::endl;
            std::cout << std::endl;
        }
        
        if (frameCount == 9) {
            std::cout << std::endl;
            std::cout << "🎯 SENDING TEST KEY PRESS (Windows key)" << std::endl;
            SendKeyEvent(VK_LWIN, true);  // Windows key down
            Sleep(50);
            SendKeyEvent(VK_LWIN, false); // Windows key up
            std::cout << "✅ Windows key test completed" << std::endl;
            std::cout << std::endl;
        }
        
        // Small delay between frames
        Sleep(500); // 0.5 second delay
    }
    
    std::cout << std::endl;
    std::cout << "🎊 SESSION COMPLETE!" << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << "Total frames received: " << frameCount << std::endl;
    std::cout << "Files saved as: debug_screen_000.bmp, debug_screen_001.bmp, etc." << std::endl;
    std::cout << std::endl;
    std::cout << "📁 CHECK THE BMP FILES:" << std::endl;
    std::cout << "Open the saved BMP files to see the remote computer's screen!" << std::endl;
    std::cout << std::endl;
    std::cout << "🎮 REMOTE CONTROL TESTS PERFORMED:" << std::endl;
    std::cout << "- Frame 3: Mouse movements (should see cursor move on remote)" << std::endl;
    std::cout << "- Frame 6: Mouse click (should see click effect on remote)" << std::endl;
    std::cout << "- Frame 9: Windows key press (should open Start menu on remote)" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    running = false;
}

int main(int argc, char* argv[]) {
    std::string serverIP = "127.0.0.1";
    std::string password;
    
    std::cout << "========================================" << std::endl;
    std::cout << "    DEBUG REMOTE DESKTOP CLIENT" << std::endl;
    std::cout << "========================================" << std::endl;
    
    ShowNetworkDiagnostics();
    
    // Get connection details
    if (argc >= 3) {
        serverIP = argv[1];
        password = argv[2];
        std::cout << "Using command line arguments:" << std::endl;
        std::cout << "Server IP: " << serverIP << std::endl;
        std::cout << "Password: " << password << std::endl;
    } else {
        std::cout << "🔗 CONNECTION SETUP" << std::endl;
        std::cout << "Enter server IP address [127.0.0.1]: ";
        std::getline(std::cin, serverIP);
        if (serverIP.empty()) {
            serverIP = "127.0.0.1";
            std::cout << "Using default: 127.0.0.1" << std::endl;
        }
        
        std::cout << "Enter password: ";
        std::getline(std::cin, password);
        
        if (password.empty()) {
            std::cout << "❌ Password cannot be empty!" << std::endl;
            std::cout << "Press Enter to exit...";
            std::cin.get();
            return 1;
        }
    }
    
    std::cout << std::endl;
    std::cout << "📡 CONNECTION ATTEMPT" << std::endl;
    std::cout << "Target server: " << serverIP << ":" << SERVER_PORT << std::endl;
    std::cout << "Password: " << password << std::endl;
    std::cout << std::endl;
    
    std::cout << "Initializing Winsock..." << std::endl;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        int error = WSAGetLastError();
        std::cerr << "❌ WSAStartup failed. Error: " << error << std::endl;
        return 1;
    }
    std::cout << "✅ Winsock initialized" << std::endl;

    std::cout << "Creating client socket..." << std::endl;
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        int error = WSAGetLastError();
        std::cerr << "❌ Socket creation failed. Error: " << error << std::endl;
        WSACleanup();
        return 1;
    }
    std::cout << "✅ Client socket created: " << clientSocket << std::endl;

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    
    std::cout << "Converting IP address: " << serverIP << std::endl;
    int result = inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
    if (result <= 0) {
        std::cerr << "❌ Invalid IP address format: " << serverIP << std::endl;
        if (result == 0) {
            std::cerr << "The IP address format is invalid." << std::endl;
        } else {
            std::cerr << "inet_pton error: " << WSAGetLastError() << std::endl;
        }
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "✅ IP address converted successfully" << std::endl;

    std::cout << std::endl;
    std::cout << "🔌 ATTEMPTING CONNECTION..." << std::endl;
    std::cout << "This may take a few seconds..." << std::endl;
    
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        std::cerr << std::endl;
        std::cerr << "❌ CONNECTION FAILED!" << std::endl;
        std::cerr << "Error code: " << error << std::endl;
        
        switch (error) {
            case WSAECONNREFUSED:
                std::cerr << "Connection refused - Server is not running or not listening on port " << SERVER_PORT << std::endl;
                std::cerr << "SOLUTION: Make sure the server is running on " << serverIP << std::endl;
                break;
            case WSAENETUNREACH:
                std::cerr << "Network unreachable - Cannot reach " << serverIP << std::endl;
                std::cerr << "SOLUTION: Check network connection and IP address" << std::endl;
                break;
            case WSAETIMEDOUT:
                std::cerr << "Connection timeout - Server not responding" << std::endl;
                std::cerr << "SOLUTION: Check if server is running and firewall settings" << std::endl;
                break;
            case WSAEHOSTUNREACH:
                std::cerr << "Host unreachable - Cannot find " << serverIP << std::endl;
                std::cerr << "SOLUTION: Verify the IP address is correct" << std::endl;
                break;
            default:
                std::cerr << "Unknown connection error" << std::endl;
                break;
        }
        
        std::cerr << std::endl;
        std::cerr << "🔧 TROUBLESHOOTING STEPS:" << std::endl;
        std::cerr << "1. Verify server is running on " << serverIP << std::endl;
        std::cerr << "2. Check if port " << SERVER_PORT << " is open" << std::endl;
        std::cerr << "3. Try with 127.0.0.1 if testing on same computer" << std::endl;
        std::cerr << "4. Check Windows Firewall settings" << std::endl;
        std::cerr << "5. Verify network connectivity with: ping " << serverIP << std::endl;
        
        closesocket(clientSocket);
        WSACleanup();
        
        std::cout << std::endl;
        std::cout << "Press Enter to exit...";
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cin.get();
        return 1;
    }

    std::cout << "✅ CONNECTION SUCCESSFUL!" << std::endl;
    std::cout << "Connected to " << serverIP << ":" << SERVER_PORT << std::endl;
    std::cout << std::endl;

    std::cout << "🔐 SENDING AUTHENTICATION..." << std::endl;
    
    // Send authentication
    PasswordAuth auth = {};
    strncpy_s(auth.password, sizeof(auth.password), password.c_str(), _TRUNCATE);
    
    std::cout << "Sending password: '" << auth.password << "'" << std::endl;
    
    if (!SendData(clientSocket, &auth, sizeof(auth))) {
        std::cerr << "❌ Failed to send authentication" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "⏳ Waiting for authentication response..." << std::endl;

    // Receive screen dimensions
    uint32_t screenWidth, screenHeight;
    if (!ReceiveData(clientSocket, &screenWidth, sizeof(screenWidth)) ||
        !ReceiveData(clientSocket, &screenHeight, sizeof(screenHeight))) {
        std::cerr << std::endl;
        std::cerr << "❌ AUTHENTICATION FAILED!" << std::endl;
        std::cerr << "Server rejected the password or connection error occurred." << std::endl;
        std::cerr << std::endl;
        std::cerr << "Possible reasons:" << std::endl;
        std::cerr << "1. Wrong password (case-sensitive)" << std::endl;
        std::cerr << "2. Server disconnected" << std::endl;
        std::cerr << "3. Network error" << std::endl;
        std::cerr << std::endl;
        std::cerr << "SOLUTION: Check the password and try again" << std::endl;
        
        closesocket(clientSocket);
        WSACleanup();
        
        std::cout << std::endl;
        std::cout << "Press Enter to exit...";
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cin.get();
        return 1;
    }

    std::cout << "✅ AUTHENTICATION SUCCESSFUL!" << std::endl;
    std::cout << "Remote screen size: " << screenWidth << "x" << screenHeight << std::endl;
    std::cout << std::endl;
    std::cout << "🚀 STARTING REMOTE CONTROL SESSION..." << std::endl;

    // Start screen receiving thread
    std::thread screenThread(ScreenReceiveThread);

    // Wait for completion
    if (screenThread.joinable()) {
        screenThread.join();
    }

    std::cout << std::endl;
    std::cout << "🔌 Closing connection..." << std::endl;
    closesocket(clientSocket);
    WSACleanup();
    
    std::cout << "✅ Session ended successfully!" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Enter to exit...";
    std::cin.clear();
    std::cin.ignore(10000, '\n');
    std::cin.get();
    
    return 0;
}
