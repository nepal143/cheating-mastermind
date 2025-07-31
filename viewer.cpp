// Real-time Remote Desktop Viewer with GUI
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
#define WM_UPDATE_SCREEN (WM_USER + 1)

// Global variables
HWND g_hMainWnd = NULL;
HWND g_hCanvas = NULL;
std::atomic<bool> g_Connected(false);
std::atomic<SOCKET> g_Socket(INVALID_SOCKET);
HBITMAP g_hScreenBitmap = NULL;
uint32_t g_RemoteWidth = 0, g_RemoteHeight = 0;
std::string g_ServerIP;
std::string g_Password;

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
    uint32_t flags;
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
    KeyboardEvent keyEvent = {keyDown ? (uint8_t)1 : (uint8_t)2, keyCode, 0};
    
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
    int frameCount = 0;
    
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
            if (g_hMainWnd) {
                PostMessage(g_hMainWnd, WM_UPDATE_SCREEN, 0, 0);
            }
        }
        
        frameCount++;
    }
    
    g_Connected = false;
    if (g_hMainWnd) {
        PostMessage(g_hMainWnd, WM_USER + 2, 0, 0); // Disconnect message
    }
}

LRESULT CALLBACK CanvasProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            if (g_hScreenBitmap && g_Connected) {
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
                // Draw status message
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkColor(hdc, RGB(0, 0, 0));
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                FillRect(hdc, &clientRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
                
                const char* message = g_Connected ? "Connecting..." : "Not Connected";
                DrawTextA(hdc, message, -1, &clientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
            if (g_Connected) {
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
        
        case WM_CHAR: {
            // Handle character input
            if (g_Connected && wParam >= 32 && wParam <= 126) {
                // Convert char to virtual key code (simplified)
                WORD vk = VkKeyScanA((char)wParam) & 0xFF;
                SendKeyEvent(vk, true);
                Sleep(10);
                SendKeyEvent(vk, false);
            }
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Create canvas for remote screen display
            g_hCanvas = CreateWindowA("RemoteCanvas", "",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
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
        
        case WM_UPDATE_SCREEN: {
            // Update screen display
            if (g_hCanvas) {
                InvalidateRect(g_hCanvas, NULL, FALSE);
            }
            return 0;
        }
        
        case WM_USER + 2: {
            // Disconnect message
            MessageBoxA(hwnd, "Disconnected from remote computer", "Remote Desktop Viewer", MB_OK | MB_ICONINFORMATION);
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

bool ConnectToRemote() {
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
    
    if (inet_pton(AF_INET, g_ServerIP.c_str(), &serverAddr.sin_addr) <= 0) {
        MessageBoxA(NULL, ("Invalid IP address: " + g_ServerIP).c_str(), "Error", MB_OK | MB_ICONERROR);
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
    strncpy_s(auth.password, g_Password.c_str(), sizeof(auth.password) - 1);
    
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

INT_PTR CALLBACK ConnectDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            SetDlgItemTextA(hDlg, 1001, "127.0.0.1");
            SetFocus(GetDlgItem(hDlg, 1001));
            return FALSE;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                char ip[256], password[256];
                GetDlgItemTextA(hDlg, 1001, ip, sizeof(ip));
                GetDlgItemTextA(hDlg, 1002, password, sizeof(password));
                
                if (strlen(ip) == 0 || strlen(password) == 0) {
                    MessageBoxA(hDlg, "Please enter both IP address and password", "Error", MB_OK);
                    return TRUE;
                }
                
                g_ServerIP = ip;
                g_Password = password;
                EndDialog(hDlg, IDOK);
                return TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Show connection dialog
    HWND hDialog = CreateDialogA(hInstance, MAKEINTRESOURCEA(100), NULL, ConnectDialogProc);
    if (!hDialog) {
        // Create simple input dialog using message boxes
        char buffer[512];
        strcpy_s(buffer, "Enter remote computer details:\n\nIP Address (e.g., 192.168.1.100):");
        
        // Simple way to get input - not ideal but works
        g_ServerIP = "127.0.0.1"; // Default
        g_Password = ""; // Will be set by user
        
        // For now, use console input
        AllocConsole();
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
        
        std::cout << "=== REMOTE DESKTOP VIEWER ===" << std::endl;
        std::cout << "Enter remote IP address [127.0.0.1]: ";
        std::string input;
        std::getline(std::cin, input);
        if (!input.empty()) g_ServerIP = input;
        
        std::cout << "Enter password: ";
        std::getline(std::cin, g_Password);
        
        if (g_Password.empty()) {
            std::cout << "Password cannot be empty!" << std::endl;
            return 0;
        }
        
        FreeConsole();
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
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RemoteDesktopViewer";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);
    
    // Create main window
    g_hMainWnd = CreateWindowA("RemoteDesktopViewer", "Remote Desktop Viewer - Connecting...",
                              WS_OVERLAPPEDWINDOW,
                              100, 100, 1024, 768,
                              NULL, NULL, hInstance, NULL);
    
    if (!g_hMainWnd) {
        return 1;
    }
    
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    
    // Connect to remote
    if (ConnectToRemote()) {
        SetWindowTextA(g_hMainWnd, ("Remote Desktop Viewer - Connected to " + g_ServerIP).c_str());
        
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
        DestroyWindow(g_hMainWnd);
    }
    
    // Cleanup
    if (g_Socket.load() != INVALID_SOCKET) {
        closesocket(g_Socket.load());
    }
    WSACleanup();
    
    return 0;
}
