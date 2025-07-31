@echo off
echo Building Remote Desktop Application...

cl RemoteDesktop.cpp /std:c++14 /EHsc /link ws2_32.lib user32.lib gdi32.lib comctl32.lib kernel32.lib /out:RemoteDesktop.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful! Run RemoteDesktop.exe to start the application.
    echo.
    echo Usage:
    echo 1. To GIVE ACCESS: Click "Give Access" button, share the generated password
    echo 2. To TAKE ACCESS: Enter remote IP and password, then click "Connect"
    echo.
) else (
    echo Build failed!
)

pause
