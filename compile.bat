@echo off
echo ========================================
echo    BUILDING REMOTE DESKTOP SOLUTION
echo ========================================

REM Try to find and setup Visual Studio environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

echo Building server application...
cl server.cpp /EHsc /Fe:server.exe /link Ws2_32.lib Gdi32.lib User32.lib
if errorlevel 1 goto error

echo Building client application...
cl client.cpp /EHsc /Fe:client.exe /link Ws2_32.lib User32.lib
if errorlevel 1 goto error

echo.
echo ========================================
echo     BUILD SUCCESSFUL!
echo ========================================
echo.
echo Created applications:
echo - server.exe (run on computer giving access)
echo - client.exe (run on computer taking access)
echo.
echo USAGE:
echo.
echo 1. HOST COMPUTER (giving access):
echo    Run: server.exe
echo    Share the displayed password
echo.
echo 2. CLIENT COMPUTER (taking access):
echo    Run: client.exe
echo    Enter host IP and password
echo.
goto end

:error
echo.
echo BUILD FAILED!
echo Make sure Visual Studio C++ Build Tools are installed.
echo.

:end
pause
