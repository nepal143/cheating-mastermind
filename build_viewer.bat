@echo off
echo ========================================
echo    BUILDING GUI VIEWER
echo ========================================

REM Try to find and setup Visual Studio environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

echo Building GUI viewer application...
cl viewer.cpp /EHsc /Fe:viewer.exe /link Ws2_32.lib User32.lib Gdi32.lib
if errorlevel 1 goto error

echo.
echo ========================================
echo     GUI VIEWER BUILD SUCCESSFUL!
echo ========================================
echo.
echo Created application:
echo - viewer.exe (real-time GUI remote control)
echo.
goto end

:error
echo.
echo BUILD FAILED!
echo.

:end
pause
