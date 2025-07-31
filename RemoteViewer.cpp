// Enhanced Remote Desktop Viewer
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#define PORT_BASE 9000

// Global variables
HWND g_hViewerWnd = NULL;
HWND g_hCanvas = NULL;
std::atomic<bool> g_Connected(false);
std::atomic<SOCKET> g_Socket(INVALID_SOCKET);
HBITMAP g_hScreenBitmap = NULL;
uint32_t g_RemoteWidth = 0, g_RemoteHeight = 0;

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

void SendMouseEvent(uint8_t type, int16_t x = 0, int16_t y = 0) {
    SOCKET sock = g_Socket.load();
    if (sock == INVALID_SOCKET) return;
    
    uint8_t eventType = 1; // Mouse event
    MouseEvent mouseEvent = {type, x, y};
    
    SendData(sock, &eventType, sizeof(eventType));
    SendData(sock, &mouseEvent, sizeof(mouseEvent));
}

void SendKeyEvent(uint16_t keyCode, bool keyDown) {
    SOCKET sock = g_Socket.load();
    if (sock == INVALID_SOCKET) return;
    
    uint8_t eventType = 2; // Keyboard event
    KeyboardEvent keyEvent = {keyDown ? (uint8_t)1 : (uint8_t)2, keyCode};
    
    SendData(sock, &eventType, sizeof(eventType));
    SendData(sock, &keyEvent, sizeof(keyEvent));
}

HBITMAP CreateBitmapFromBMP(const std::vector<unsigned char>& bmpData) {
    if (bmpData.size() < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) {
        return NULL;
    }
    
    const BITMAPFILEHEADER* fileHeader = reinterpret_cast<const BITMAPFILEHEADER*>(bmpData.data());
    const BITMAPINFOHEADER* infoHeader = reinterpret_cast<const BITMAPINFOHEADER*>(bmpData.data() + sizeof(BITMAPFILEHEADER));
    const unsigned char* pixelData = bmpData.data() + fileHeader->bfOffBits;
    
    HDC hDC = GetDC(NULL);
    HBITMAP hBitmap = CreateDIBitmap(hDC, infoHeader, CBM_INIT, pixelData, (BITMAPINFO*)infoHeader, DIB_RGB_COLORS);
    ReleaseDC(NULL, hDC);
    
    return hBitmap;
}

void ClientReceiveThread() {
    while (g_Connected) {
        ScreenFrame frameHeader;
        if (!ReceiveData(g_Socket.load(), &frameHeader, sizeof(frameHeader))) {
            break;
        }
        
        std::vector<unsigned char> imageData(frameHeader.dataSize);
        if (!ReceiveData(g_Socket.load(), imageData.data(), frameHeader.dataSize)) {
            break;
        }
        
        // Create bitmap and update display
        HBITMAP hNewBitmap = CreateBitmapFromBMP(imageData);
        if (hNewBitmap) {
            if (g_hScreenBitmap) {
                DeleteObject(g_hScreenBitmap);
            }
            g_hScreenBitmap = hNewBitmap;
            g_RemoteWidth = frameHeader.width;
            g_RemoteHeight = frameHeader.height;
            
            // Trigger repaint
            if (g_hCanvas) {
                InvalidateRect(g_hCanvas, NULL, FALSE);
            }
        }
    }
    
    g_Connected = false;
    if (g_hViewerWnd) {
        PostMessage(g_hViewerWnd, WM_USER + 1, 0, 0); // Custom message for disconnect
    }
}

LRESULT CALLBACK CanvasProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            if (g_hScreenBitmap) {
                HDC hMemDC = CreateCompatibleDC(hdc);
                HGDIOBJ hOldBitmap = SelectObject(hMemDC, g_hScreenBitmap);
                
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                
                // Scale the remote screen to fit the window
                StretchBlt(hdc, 0, 0, clientRect.right, clientRect.bottom,
                          hMemDC, 0, 0, g_RemoteWidth, g_RemoteHeight, SRCCOPY);
                
                SelectObject(hMemDC, hOldBitmap);
                DeleteDC(hMemDC);
            } else {
                // Draw "Not Connected" message
                SetTextColor(hdc, RGB(255, 0, 0));
                SetBkMode(hdc, TRANSPARENT);
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                DrawTextA(hdc, "Not Connected", -1, &clientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            if (g_Connected) {
                SetCapture(hwnd);
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                int x = (LOWORD(lParam) * g_RemoteWidth) / clientRect.right;
                int y = (HIWORD(lParam) * g_RemoteHeight) / clientRect.bottom;
                SendMouseEvent(2, x, y); // Left button down
            }
            return 0;
        }
        
        case WM_LBUTTONUP: {
            if (g_Connected) {
                ReleaseCapture();
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                int x = (LOWORD(lParam) * g_RemoteWidth) / clientRect.right;
                int y = (HIWORD(lParam) * g_RemoteHeight) / clientRect.bottom;
                SendMouseEvent(3, x, y); // Left button up
            }
            return 0;
        }
        
        case WM_RBUTTONDOWN: {
            if (g_Connected) {
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                int x = (LOWORD(lParam) * g_RemoteWidth) / clientRect.right;
                int y = (HIWORD(lParam) * g_RemoteHeight) / clientRect.bottom;
                SendMouseEvent(4, x, y); // Right button down
            }
            return 0;
        }
        
        case WM_RBUTTONUP: {
            if (g_Connected) {
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                int x = (LOWORD(lParam) * g_RemoteWidth) / clientRect.right;
                int y = (HIWORD(lParam) * g_RemoteHeight) / clientRect.bottom;
                SendMouseEvent(5, x, y); // Right button up
            }
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            if (g_Connected && (wParam & MK_LBUTTON || wParam & MK_RBUTTON)) {
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                int x = (LOWORD(lParam) * g_RemoteWidth) / clientRect.right;
                int y = (HIWORD(lParam) * g_RemoteHeight) / clientRect.bottom;
                SendMouseEvent(1, x, y); // Mouse move
            }
            return 0;
        }
        
        case WM_KEYDOWN: {
            if (g_Connected) {
                SendKeyEvent(wParam, true);
            }
            return 0;
        }
        
        case WM_KEYUP: {
            if (g_Connected) {
                SendKeyEvent(wParam, false);
            }
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ViewerWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Create canvas for remote screen display
            g_hCanvas = CreateWindowA("RemoteCanvas", "",
                                    WS_CHILD | WS_VISIBLE,
                                    0, 0, 800, 600,
                                    hwnd, NULL, NULL, NULL);
            SetFocus(g_hCanvas);
            return 0;
        }
        
        case WM_SIZE: {
            // Resize canvas to fill window
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            if (g_hCanvas) {
                SetWindowPos(g_hCanvas, NULL, 0, 0, 
                           clientRect.right, clientRect.bottom,
                           SWP_NOZORDER);
            }
            return 0;
        }
        
        case WM_USER + 1: {
            // Custom disconnect message
            MessageBoxA(hwnd, "Disconnected from remote computer", "Remote Desktop Viewer", MB_OK | MB_ICONINFORMATION);
            PostQuitMessage(0);
            return 0;
        }
        
        case WM_CLOSE:
            g_Connected = false;
            if (g_Socket.load() != INVALID_SOCKET) {
                closesocket(g_Socket.load());
                g_Socket.store(INVALID_SOCKET);
            }
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            if (g_hScreenBitmap) {
                DeleteObject(g_hScreenBitmap);
            }
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool ConnectToRemote(const std::string& ip, const std::string& password) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBoxA(NULL, "WSA Startup failed", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        MessageBoxA(NULL, "Socket creation failed", "Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_BASE);
    
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        MessageBoxA(NULL, "Invalid IP address", "Error", MB_OK | MB_ICONERROR);
        closesocket(clientSocket);
        WSACleanup();
        return false;
    }

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        MessageBoxA(NULL, "Connection failed - check IP and ensure remote is giving access", "Error", MB_OK | MB_ICONERROR);
        closesocket(clientSocket);
        WSACleanup();
        return false;
    }

    // Send authentication
    PasswordAuth auth = {};
    strncpy_s(auth.password, password.c_str(), sizeof(auth.password) - 1);
    
    if (!SendData(clientSocket, &auth, sizeof(auth))) {
        MessageBoxA(NULL, "Failed to send authentication", "Error", MB_OK | MB_ICONERROR);
        closesocket(clientSocket);
        WSACleanup();
        return false;
    }

    // Receive screen dimensions
    uint32_t screenWidth, screenHeight;
    if (!ReceiveData(clientSocket, &screenWidth, sizeof(screenWidth)) ||
        !ReceiveData(clientSocket, &screenHeight, sizeof(screenHeight))) {
        MessageBoxA(NULL, "Authentication failed - wrong password", "Error", MB_OK | MB_ICONERROR);
        closesocket(clientSocket);
        WSACleanup();
        return false;
    }

    g_Socket.store(clientSocket);
    g_RemoteWidth = screenWidth;
    g_RemoteHeight = screenHeight;
    g_Connected = true;
    
    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Parse command line arguments
    std::string ip = "127.0.0.1";
    std::string password = "";
    
    if (strlen(lpCmdLine) > 0) {
        // Simple parsing: "IP PASSWORD"
        std::string cmdLine(lpCmdLine);
        size_t spacePos = cmdLine.find(' ');
        if (spacePos != std::string::npos) {
            ip = cmdLine.substr(0, spacePos);
            password = cmdLine.substr(spacePos + 1);
        }
    }
    
    // If no command line args, ask user
    if (password.empty()) {
        char inputBuffer[512];
        sprintf_s(inputBuffer, "Enter connection details:\nFormat: IP_ADDRESS PASSWORD\nExample: 192.168.1.100 ABC123XY");
        
        // For simplicity, we'll use a message box. In a real app, you'd use a dialog.
        if (MessageBoxA(NULL, inputBuffer, "Remote Desktop Viewer", MB_OKCANCEL) == IDCANCEL) {
            return 0;
        }
        
        // For this demo, we'll use hardcoded values
        // In a real application, you'd show a proper input dialog
        ip = "127.0.0.1";
        password = "TESTPASS"; // User would input this
    }
    
    // Register canvas window class
    WNDCLASSA canvasWc = {};
    canvasWc.lpfnWndProc = CanvasProc;
    canvasWc.hInstance = hInstance;
    canvasWc.lpszClassName = "RemoteCanvas";
    canvasWc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    canvasWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&canvasWc);
    
    // Register main window class
    WNDCLASSA wc = {};
    wc.lpfnWndProc = ViewerWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RemoteDesktopViewer";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);
    
    // Create main window
    g_hViewerWnd = CreateWindowA("RemoteDesktopViewer", "Remote Desktop Viewer - Connecting...",
                                WS_OVERLAPPEDWINDOW,
                                100, 100, 820, 640,
                                NULL, NULL, hInstance, NULL);
    
    if (!g_hViewerWnd) {
        return 1;
    }
    
    ShowWindow(g_hViewerWnd, nCmdShow);
    UpdateWindow(g_hViewerWnd);
    
    // Connect to remote
    if (ConnectToRemote(ip, password)) {
        SetWindowTextA(g_hViewerWnd, "Remote Desktop Viewer - Connected");
        
        // Start receiving thread
        std::thread receiveThread(ClientReceiveThread);
        receiveThread.detach();
        
        // Message loop
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    } else {
        DestroyWindow(g_hViewerWnd);
    }
    
    // Cleanup
    if (g_Socket.load() != INVALID_SOCKET) {
        closesocket(g_Socket.load());
    }
    WSACleanup();
    
    return 0;
}
