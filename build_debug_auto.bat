@echo off
echo Building Debug Client and Server...
echo.

REM Check if cl (Visual Studio) is available
where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo Using Visual Studio compiler (cl)...
    goto :use_cl
)

REM Check if g++ is available  
where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Using g++ compiler...
    goto :use_gcc
)

REM Check if gcc is available
where gcc >nul 2>nul
if %errorlevel% equ 0 (
    echo Using gcc compiler...
    goto :use_gcc
)

echo ERROR: No suitable compiler found!
echo.
echo SOLUTIONS:
echo 1. Install Visual Studio (recommended)
echo    - Download Visual Studio Community (free)
echo    - Run this from "Developer Command Prompt for VS"
echo.
echo 2. Install MinGW-w64 or MSYS2
echo    - Download from: https://www.msys2.org/
echo    - Add to PATH: C:\msys64\mingw64\bin
echo.
echo 3. Use the pre-compiled executables if available
echo.
pause
exit /b 1

:use_cl
echo Compiling debug_server.cpp with Visual Studio...
cl /EHsc /D_WIN32_WINNT=0x0601 debug_server.cpp User32.lib Gdi32.lib Ole32.lib
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile debug_server.cpp
    pause
    exit /b 1
)

echo Compiling debug_client.cpp with Visual Studio...
cl /EHsc /D_WIN32_WINNT=0x0601 debug_client.cpp Ws2_32.lib
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile debug_client.cpp
    pause
    exit /b 1
)
goto :success

:use_gcc
echo Compiling debug_server.cpp with g++/gcc...
g++ -std=c++11 -D_WIN32_WINNT=0x0601 -o debug_server.exe debug_server.cpp -luser32 -lgdi32 -lole32 -static-libgcc -static-libstdc++
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile debug_server.cpp
    pause
    exit /b 1
)

echo Compiling debug_client.cpp with g++/gcc...
g++ -std=c++11 -D_WIN32_WINNT=0x0601 -o debug_client.exe debug_client.cpp -lws2_32 -static-libgcc -static-libstdc++
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile debug_client.cpp
    pause
    exit /b 1
)
goto :success

:success
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
