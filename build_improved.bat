@echo off
echo ========================================
echo   REBUILDING REMOTE DESKTOP SOLUTION
echo ========================================
echo.

REM Clean previous builds
if exist *.exe del *.exe
if exist *.obj del *.obj
if exist *.pdb del *.pdb
if exist *.bmp del *.bmp

echo Building IMPROVED server application...
cl server.cpp /std:c++14 /EHsc /Fe:server.exe /link Ws2_32.lib Gdi32.lib User32.lib

if %ERRORLEVEL% NEQ 0 (
    echo Server build failed!
    pause
    exit /b 1
)

echo Building IMPROVED client application...
cl client.cpp /std:c++14 /EHsc /Fe:client.exe /link Ws2_32.lib User32.lib

if %ERRORLEVEL% NEQ 0 (
    echo Client build failed!
    pause
    exit /b 1
)

echo Building GUI viewer application...
cl viewer.cpp /std:c++14 /EHsc /Fe:viewer.exe /link Ws2_32.lib User32.lib Gdi32.lib

if %ERRORLEVEL% NEQ 0 (
    echo Viewer build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo    BUILD SUCCESSFUL - IMPROVED VERSION!
echo ========================================
echo.
echo APPLICATIONS CREATED:
echo 1. server.exe  - IMPROVED server (better debugging)
echo 2. client.exe  - IMPROVED client (better error handling)
echo 3. viewer.exe  - NEW real-time GUI viewer
echo.
echo DEBUGGING INSTRUCTIONS:
echo.
echo === STEP 1: TEST LOCALLY FIRST ===
echo 1. Run server.exe on Computer A
echo 2. Note the password (e.g., "K8N2M7Q9")
echo 3. Run client.exe on SAME computer (Computer A)
echo 4. Enter IP: 127.0.0.1
echo 5. Enter the password
echo 6. Should receive 100 screen frames and save as BMP files
echo.
echo === STEP 2: TEST BETWEEN COMPUTERS ===
echo 1. Run server.exe on Computer A
echo 2. Note the password
echo 3. Find Computer A's IP: run "ipconfig" 
echo 4. Run client.exe on Computer B
echo 5. Enter Computer A's IP (e.g., 192.168.1.100)
echo 6. Enter the password
echo.
echo === STEP 3: USE GUI VIEWER ===
echo 1. Run viewer.exe for real-time control
echo 2. Enter IP and password when prompted
echo 3. See and control remote desktop in real-time!
echo.
echo IMPROVEMENTS MADE:
echo - Better error messages and debugging
echo - Longer session (100 frames instead of 50)
echo - More detailed connection status
echo - Heartbeat monitoring on server
echo - Real-time GUI viewer with mouse/keyboard control
echo - Better authentication feedback
echo.
pause
