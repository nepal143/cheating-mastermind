@echo off
echo ========================================
echo Remote Desktop Application Builder
echo ========================================
echo.

REM Clean previous builds
if exist *.exe del *.exe
if exist *.obj del *.obj
if exist *.pdb del *.pdb

echo Building Main Remote Desktop Application...
cl RemoteDesktop.cpp /std:c++14 /EHsc /Fe:RemoteDesktop.exe /link ws2_32.lib user32.lib gdi32.lib comctl32.lib kernel32.lib

if %ERRORLEVEL% NEQ 0 (
    echo Main application build failed!
    pause
    exit /b 1
)

echo Building Remote Desktop Viewer...
cl RemoteViewer.cpp /std:c++14 /EHsc /Fe:RemoteViewer.exe /link ws2_32.lib user32.lib gdi32.lib kernel32.lib

if %ERRORLEVEL% NEQ 0 (
    echo Viewer application build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo BUILD SUCCESSFUL!
echo ========================================
echo.
echo Applications created:
echo 1. RemoteDesktop.exe - Main control application
echo 2. RemoteViewer.exe  - Remote screen viewer
echo.
echo USAGE INSTRUCTIONS:
echo.
echo === TO GIVE ACCESS (Host Computer) ===
echo 1. Run RemoteDesktop.exe
echo 2. Click "Give Access (Start Server)"
echo 3. Share the generated password with the remote user
echo 4. Keep the application running
echo.
echo === TO TAKE ACCESS (Client Computer) ===
echo Option A - Using Main App:
echo 1. Run RemoteDesktop.exe
echo 2. Enter the host's IP address
echo 3. Enter the password provided by host
echo 4. Click "Connect"
echo.
echo Option B - Using Viewer:
echo 1. Run RemoteViewer.exe (will show input dialog)
echo 2. Enter connection details when prompted
echo 3. Full screen control with mouse and keyboard
echo.
echo FEATURES:
echo - Password-based authentication
echo - Real-time screen sharing (15 FPS)
echo - Full mouse and keyboard control
echo - Simple, user-friendly interface
echo - Works over local network and internet
echo.

pause
