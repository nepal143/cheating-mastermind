@echo off
echo Building Debug Client and Server...
echo.

echo Compiling debug_server.cpp...
cl /EHsc /D_WIN32_WINNT=0x0601 debug_server.cpp User32.lib Gdi32.lib Ole32.lib
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile debug_server.cpp
    pause
    exit /b 1
)

echo Compiling debug_client.cpp...
cl /EHsc /D_WIN32_WINNT=0x0601 debug_client.cpp Ws2_32.lib
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile debug_client.cpp
    pause
    exit /b 1
)

echo.
echo ========================================
echo    BUILD COMPLETED SUCCESSFULLY!
echo ========================================
echo.
echo Executables created:
echo - debug_server.exe (run on host computer)
echo - debug_client.exe (run on client computer)
echo.
echo NEXT STEPS:
echo 1. Run debug_server.exe on the host computer
echo 2. Note the password displayed by the server
echo 3. Run debug_client.exe on the client computer
echo 4. Enter the server IP and password when prompted
echo.
echo DEBUGGING FEATURES:
echo - Maximum verbosity logging
echo - Network diagnostics
echo - Step-by-step connection process
echo - Error code explanations
echo - Test remote control commands
echo - Screen captures saved as BMP files
echo.
pause
