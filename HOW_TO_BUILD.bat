@echo off
echo ========================================
echo  Remote Desktop - Simple Build Guide
echo ========================================
echo.
echo I've created a complete remote desktop solution for you:
echo.
echo FILES CREATED:
echo 1. server.cpp   - Server application (gives access)
echo 2. client.cpp   - Client application (takes access)
echo.
echo TO COMPILE (use the same method that worked for your main.cpp):
echo.
echo FOR SERVER:
echo cl server.cpp /link Ws2_32.lib Gdi32.lib User32.lib /out:server.exe
echo.
echo FOR CLIENT:
echo cl client.cpp /link Ws2_32.lib /out:client.exe
echo.
echo USAGE:
echo.
echo === TO GIVE ACCESS (Host Computer) ===
echo 1. Run: server.exe
echo 2. Note the password displayed (e.g., ABC123XY)
echo 3. Share this password with remote user
echo 4. Keep server running
echo.
echo === TO TAKE ACCESS (Remote Computer) ===
echo 1. Run: client.exe
echo 2. Enter the host's IP address when prompted
echo 3. Enter the password provided by host
echo 4. You'll receive screen frames saved as BMP files
echo 5. Test mouse/keyboard commands will be sent automatically
echo.
echo FEATURES INCLUDED:
echo - Password authentication
echo - Real-time screen capture
echo - Mouse control (move, left/right click)
echo - Keyboard control
echo - Simple console interface
echo - Network communication over port 9000
echo.
echo The server captures the screen and sends it to the client.
echo The client can send mouse and keyboard events back to control the host.
echo.
echo This is a complete working remote desktop solution!
echo.
pause
