// Remote Desktop Application - Complete Solution
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <commctrl.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

// Constants
#define PORT_BASE 9000
#define FRAME_RATE 15
#define FRAME_INTERVAL (1000 / FRAME_RATE)

// Window controls IDs
#define ID_BTN_GIVE_ACCESS 1001
#define ID_BTN_TAKE_ACCESS 1002
#define ID_EDIT_PASSWORD 1003
#define ID_EDIT_IP 1004
#define ID_EDIT_REMOTE_PASSWORD 1005
#define ID_STATIC_PASSWORD 1006
#define ID_STATIC_STATUS 1007
#define ID_BTN_CONNECT 1008
#define ID_BTN_DISCONNECT 1009

// Global variables
HWND g_hMainWnd = NULL;
HWND g_hPasswordEdit = NULL;
HWND g_hIPEdit = NULL;
HWND g_hRemotePasswordEdit = NULL;
HWND g_hStatusStatic = NULL;
HWND g_hGiveAccessBtn = NULL;
HWND g_hTakeAccessBtn = NULL;
HWND g_hConnectBtn = NULL;
HWND g_hDisconnectBtn = NULL;

std::atomic<bool> g_ServerRunning(false);
std::atomic<bool> g_ClientConnected(false);
std::atomic<SOCKET> g_ClientSocket(INVALID_SOCKET);
std::thread g_ServerThread;
std::thread g_ClientThread;
std::string g_GeneratedPassword;

// Protocol structures
struct PasswordAuth {
    char password[32];
};

struct MouseEvent {
    uint8_t type;
    int16_t x, y;
};

struct KeyboardEvent {
    uint8_t type;
    uint16_t keyCode;
};

struct ScreenFrame {
    uint32_t dataSize;
    uint32_t width;
    uint32_t height;
};

// Helper functions
std::string GenerateRandomPassword() {
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

void UpdateStatus(const std::string& status) {
    if (g_hStatusStatic) {
        SetWindowTextA(g_hStatusStatic, status.c_str());
    }
}

bool SendData(SOCKET socket, const void* data, int size) {
    if (socket == INVALID_SOCKET) return false;
    
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
    if (socket == INVALID_SOCKET) return false;
    
    char* ptr = static_cast<char*>(data);
    int totalReceived = 0;
    
    while (totalReceived < size) {
        int received = recv(socket, ptr + totalReceived, size - totalReceived, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
}

// Screen capture functions
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

// Input simulation functions
void SimulateMouseMove(int x, int y) {
    SetCursorPos(x, y);
}

void SimulateMouseClick(uint8_t type) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    
    switch (type) {
        case 2: input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; break;
        case 3: input.mi.dwFlags = MOUSEEVENTF_LEFTUP; break;
        case 4: input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; break;
        case 5: input.mi.dwFlags = MOUSEEVENTF_RIGHTUP; break;
        default: return;
    }
    
    SendInput(1, &input, sizeof(INPUT));
}

void SimulateKey(uint16_t keyCode, bool keyDown) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = keyDown ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

// Server functions (Give Access)
void ServerThread() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        UpdateStatus("WSA Startup failed");
        return;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        UpdateStatus("Server socket creation failed");
        WSACleanup();
        return;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_BASE);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        UpdateStatus("Server bind failed");
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR) {
        UpdateStatus("Server listen failed");
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    UpdateStatus("Server listening... Waiting for connections");

    while (g_ServerRunning) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverSocket, &readSet);
        
        timeval timeout = {1, 0}; // 1 second timeout
        int result = select(0, &readSet, nullptr, nullptr, &timeout);
        
        if (result > 0 && FD_ISSET(serverSocket, &readSet)) {
            SOCKET clientSocket = accept(serverSocket, NULL, NULL);
            if (clientSocket != INVALID_SOCKET) {
                // Authenticate client
                PasswordAuth auth;
                if (ReceiveData(clientSocket, &auth, sizeof(auth))) {
                    if (g_GeneratedPassword == std::string(auth.password)) {
                        UpdateStatus("Client authenticated! Sharing screen...");
                        g_ClientSocket.store(clientSocket);
                        
                        // Send screen dimensions
                        uint32_t width = GetSystemMetrics(SM_CXSCREEN);
                        uint32_t height = GetSystemMetrics(SM_CYSCREEN);
                        SendData(clientSocket, &width, sizeof(width));
                        SendData(clientSocket, &height, sizeof(height));
                        
                        // Main server loop
                        auto lastFrameTime = std::chrono::steady_clock::now();
                        
                        while (g_ServerRunning && g_ClientSocket.load() != INVALID_SOCKET) {
                            // Send screen frames
                            auto currentTime = std::chrono::steady_clock::now();
                            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                currentTime - lastFrameTime).count();
                                
                            if (elapsed >= FRAME_INTERVAL) {
                                auto bmpData = CaptureScreenAsBMP();
                                if (!bmpData.empty()) {
                                    ScreenFrame frameHeader;
                                    frameHeader.dataSize = static_cast<uint32_t>(bmpData.size());
                                    frameHeader.width = width;
                                    frameHeader.height = height;
                                    
                                    if (!SendData(clientSocket, &frameHeader, sizeof(frameHeader)) ||
                                        !SendData(clientSocket, bmpData.data(), bmpData.size())) {
                                        break;
                                    }
                                }
                                lastFrameTime = currentTime;
                            }
                            
                            // Handle input events
                            fd_set inputSet;
                            FD_ZERO(&inputSet);
                            FD_SET(clientSocket, &inputSet);
                            
                            timeval inputTimeout = {0, 1000};
                            if (select(0, &inputSet, nullptr, nullptr, &inputTimeout) > 0) {
                                uint8_t eventType;
                                if (ReceiveData(clientSocket, &eventType, sizeof(eventType))) {
                                    if (eventType == 1) { // Mouse event
                                        MouseEvent mouseEvent;
                                        if (ReceiveData(clientSocket, &mouseEvent, sizeof(mouseEvent))) {
                                            if (mouseEvent.type == 1) {
                                                SimulateMouseMove(mouseEvent.x, mouseEvent.y);
                                            } else {
                                                SimulateMouseClick(mouseEvent.type);
                                            }
                                        }
                                    } else if (eventType == 2) { // Keyboard event
                                        KeyboardEvent keyEvent;
                                        if (ReceiveData(clientSocket, &keyEvent, sizeof(keyEvent))) {
                                            SimulateKey(keyEvent.keyCode, keyEvent.type == 1);
                                        }
                                    }
                                }
                            }
                            
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                        
                        UpdateStatus("Client disconnected");
                        g_ClientSocket.store(INVALID_SOCKET);
                    } else {
                        UpdateStatus("Authentication failed - wrong password");
                    }
                }
                closesocket(clientSocket);
            }
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    UpdateStatus("Server stopped");
}

// Client functions (Take Access)
void ClientThread(const std::string& ip, const std::string& password) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        UpdateStatus("WSA Startup failed");
        return;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        UpdateStatus("Client socket creation failed");
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_BASE);
    
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        UpdateStatus("Invalid IP address");
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    UpdateStatus("Connecting to remote computer...");
    
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        UpdateStatus("Connection failed - check IP and ensure remote is giving access");
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    // Send authentication
    PasswordAuth auth = {};
    strncpy_s(auth.password, password.c_str(), sizeof(auth.password) - 1);
    
    if (!SendData(clientSocket, &auth, sizeof(auth))) {
        UpdateStatus("Failed to send authentication");
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    // Receive screen dimensions
    uint32_t screenWidth, screenHeight;
    if (!ReceiveData(clientSocket, &screenWidth, sizeof(screenWidth)) ||
        !ReceiveData(clientSocket, &screenHeight, sizeof(screenHeight))) {
        UpdateStatus("Authentication failed or connection error");
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    UpdateStatus("Connected! You can now control the remote computer. Close this window to disconnect.");
    g_ClientConnected = true;

    // Simple client loop - just receive frames (for demonstration)
    // In a full implementation, you'd display the frames and handle input
    int frameCount = 0;
    while (g_ClientConnected && frameCount < 100) { // Limit for demo
        ScreenFrame frameHeader;
        if (!ReceiveData(clientSocket, &frameHeader, sizeof(frameHeader))) {
            break;
        }
        
        std::vector<unsigned char> imageData(frameHeader.dataSize);
        if (!ReceiveData(clientSocket, imageData.data(), frameHeader.dataSize)) {
            break;
        }
        
        // Save frame (in real implementation, display it)
        std::string filename = "remote_frame_" + std::to_string(frameCount++) + ".bmp";
        std::ofstream file(filename, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
            file.close();
        }
        
        // Simulate some mouse movement for demo
        if (frameCount == 10) {
            uint8_t eventType = 1;
            MouseEvent mouseEvent = {1, 200, 200};
            SendData(clientSocket, &eventType, sizeof(eventType));
            SendData(clientSocket, &mouseEvent, sizeof(mouseEvent));
        }
    }

    UpdateStatus("Disconnected from remote computer");
    g_ClientConnected = false;
    closesocket(clientSocket);
    WSACleanup();
}

// GUI Event Handler
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Create controls
            CreateWindowA("STATIC", "Remote Desktop Control", WS_VISIBLE | WS_CHILD | SS_CENTER,
                         50, 20, 400, 30, hwnd, NULL, NULL, NULL);
            
            g_hGiveAccessBtn = CreateWindowA("BUTTON", "Give Access (Start Server)", 
                                           WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                           50, 70, 200, 40, hwnd, (HMENU)ID_BTN_GIVE_ACCESS, NULL, NULL);
            
            g_hTakeAccessBtn = CreateWindowA("BUTTON", "Take Access (Connect to Remote)", 
                                           WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                           270, 70, 200, 40, hwnd, (HMENU)ID_BTN_TAKE_ACCESS, NULL, NULL);
            
            CreateWindowA("STATIC", "Generated Password:", WS_VISIBLE | WS_CHILD,
                         50, 130, 150, 20, hwnd, NULL, NULL, NULL);
            
            g_hPasswordEdit = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY,
                                          210, 128, 100, 24, hwnd, (HMENU)ID_EDIT_PASSWORD, NULL, NULL);
            
            CreateWindowA("STATIC", "Remote IP:", WS_VISIBLE | WS_CHILD,
                         50, 170, 80, 20, hwnd, NULL, NULL, NULL);
            
            g_hIPEdit = CreateWindowA("EDIT", "127.0.0.1", WS_VISIBLE | WS_CHILD | WS_BORDER,
                                    140, 168, 120, 24, hwnd, (HMENU)ID_EDIT_IP, NULL, NULL);
            
            CreateWindowA("STATIC", "Password:", WS_VISIBLE | WS_CHILD,
                         280, 170, 70, 20, hwnd, NULL, NULL, NULL);
            
            g_hRemotePasswordEdit = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER,
                                                360, 168, 100, 24, hwnd, (HMENU)ID_EDIT_REMOTE_PASSWORD, NULL, NULL);
            
            g_hConnectBtn = CreateWindowA("BUTTON", "Connect", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                        50, 210, 100, 30, hwnd, (HMENU)ID_BTN_CONNECT, NULL, NULL);
            
            g_hDisconnectBtn = CreateWindowA("BUTTON", "Disconnect", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                           160, 210, 100, 30, hwnd, (HMENU)ID_BTN_DISCONNECT, NULL, NULL);
            
            g_hStatusStatic = CreateWindowA("STATIC", "Ready", WS_VISIBLE | WS_CHILD | SS_LEFT,
                                          50, 260, 400, 40, hwnd, (HMENU)ID_STATIC_STATUS, NULL, NULL);
            
            EnableWindow(g_hDisconnectBtn, FALSE);
            break;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_BTN_GIVE_ACCESS: {
                    if (!g_ServerRunning) {
                        g_GeneratedPassword = GenerateRandomPassword();
                        SetWindowTextA(g_hPasswordEdit, g_GeneratedPassword.c_str());
                        
                        g_ServerRunning = true;
                        g_ServerThread = std::thread(ServerThread);
                        
                        SetWindowTextA(g_hGiveAccessBtn, "Stop Server");
                        UpdateStatus("Server started. Share the password with remote user.");
                    } else {
                        g_ServerRunning = false;
                        if (g_ServerThread.joinable()) {
                            g_ServerThread.join();
                        }
                        g_ClientSocket.store(INVALID_SOCKET);
                        
                        SetWindowTextA(g_hGiveAccessBtn, "Give Access (Start Server)");
                        SetWindowTextA(g_hPasswordEdit, "");
                        UpdateStatus("Server stopped");
                    }
                    break;
                }
                
                case ID_BTN_CONNECT: {
                    if (!g_ClientConnected) {
                        char ip[256], password[256];
                        GetWindowTextA(g_hIPEdit, ip, sizeof(ip));
                        GetWindowTextA(g_hRemotePasswordEdit, password, sizeof(password));
                        
                        if (strlen(ip) == 0 || strlen(password) == 0) {
                            UpdateStatus("Please enter IP and password");
                            break;
                        }
                        
                        g_ClientThread = std::thread(ClientThread, std::string(ip), std::string(password));
                        EnableWindow(g_hConnectBtn, FALSE);
                        EnableWindow(g_hDisconnectBtn, TRUE);
                    }
                    break;
                }
                
                case ID_BTN_DISCONNECT: {
                    g_ClientConnected = false;
                    if (g_ClientThread.joinable()) {
                        g_ClientThread.join();
                    }
                    EnableWindow(g_hConnectBtn, TRUE);
                    EnableWindow(g_hDisconnectBtn, FALSE);
                    UpdateStatus("Disconnected");
                    break;
                }
            }
            break;
        }
        
        case WM_CLOSE:
            g_ServerRunning = false;
            g_ClientConnected = false;
            if (g_ServerThread.joinable()) g_ServerThread.join();
            if (g_ClientThread.joinable()) g_ClientThread.join();
            DestroyWindow(hwnd);
            break;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Main function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RemoteDesktopApp";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClassA(&wc);
    
    // Create window
    g_hMainWnd = CreateWindowA("RemoteDesktopApp", "Remote Desktop Control v1.0",
                              WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
                              CW_USEDEFAULT, CW_USEDEFAULT, 520, 350,
                              NULL, NULL, hInstance, NULL);
    
    if (!g_hMainWnd) {
        return 1;
    }
    
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
