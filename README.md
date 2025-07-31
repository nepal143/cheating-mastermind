# Remote Desktop Control Application

A complete remote desktop solution that allows you to share your screen and control computers remotely over a network.

## Features

- **Password-based Authentication**: Secure access with randomly generated passwords
- **Real-time Screen Sharing**: 15 FPS screen streaming with BMP compression
- **Full Remote Control**: Complete mouse and keyboard control
- **Simple GUI Interface**: Easy-to-use Windows interface
- **Network Support**: Works over LAN and internet connections
- **Dual Applications**: Main control app + dedicated viewer

## Files Overview

- `RemoteDesktop.exe` - Main application with both server and client functionality
- `RemoteViewer.exe` - Dedicated viewer application for better remote control experience
- `build_all.bat` - Build script to compile both applications
- `RemoteDesktop.cpp` - Source code for main application
- `RemoteViewer.cpp` - Source code for viewer application

## Quick Start

### Building the Applications

1. Open Command Prompt in the project directory
2. Run: `build_all.bat`
3. Wait for successful compilation

### Giving Access (Host Computer)

1. Run `RemoteDesktop.exe`
2. Click **"Give Access (Start Server)"**
3. A random password will be generated (e.g., "ABC123XY")
4. Share this password with the person who needs remote access
5. Keep the application running
6. Your computer is now ready to accept remote connections

### Taking Access (Client Computer)

#### Method 1: Using Main Application
1. Run `RemoteDesktop.exe`
2. Enter the host computer's IP address in the "Remote IP" field
3. Enter the password provided by the host
4. Click **"Connect"**
5. Connection status will be displayed

#### Method 2: Using Dedicated Viewer (Recommended)
1. Run `RemoteViewer.exe`
2. Enter connection details when prompted
3. You'll see the remote desktop in a window
4. Use mouse and keyboard normally to control the remote computer

## Network Setup

### For Local Network (LAN)
- Host IP: Use the local IP address (e.g., 192.168.1.100)
- Find your IP with: `ipconfig` in Command Prompt
- Both computers must be on the same network

### For Internet Access
- Host needs to forward port 9000 in router settings
- Use the public IP address of the host
- Host IP: Use public IP (find at whatismyip.com)

## Technical Details

- **Port Used**: 9000 (TCP)
- **Frame Rate**: 15 FPS
- **Authentication**: 8-character alphanumeric passwords
- **Protocol**: Custom binary protocol over TCP
- **Image Format**: BMP (uncompressed for speed)

## Security Notes

- Passwords are randomly generated for each session
- Only one client connection allowed at a time
- Server stops when application is closed
- No permanent access - password changes each session

## Troubleshooting

### Connection Failed
- Check if host computer has "Give Access" enabled
- Verify IP address is correct
- Ensure firewall allows port 9000
- Check network connectivity between computers

### Authentication Failed
- Verify password is correct (case-sensitive)
- Make sure host has generated a new password
- Check that host application is still running

### Poor Performance
- Close unnecessary applications on both computers
- Use wired internet connection instead of WiFi
- Ensure good network connection between computers

## Advanced Usage

### Command Line for Viewer
You can run the viewer with command line arguments:
```
RemoteViewer.exe 192.168.1.100 ABC123XY
```

### Firewall Configuration
If connections fail, add firewall exceptions for:
- RemoteDesktop.exe
- RemoteViewer.exe
- Port 9000 (TCP)

## Limitations

- Single client connection only
- No file transfer capability
- No audio transmission
- Requires Windows operating system
- No encryption (use over trusted networks only)

## Building from Source

Requirements:
- Visual Studio Build Tools or Visual Studio
- Windows SDK
- C++14 compatible compiler

Compile commands:
```bash
cl RemoteDesktop.cpp /std:c++14 /EHsc /Fe:RemoteDesktop.exe /link ws2_32.lib user32.lib gdi32.lib comctl32.lib kernel32.lib
cl RemoteViewer.cpp /std:c++14 /EHsc /Fe:RemoteViewer.exe /link ws2_32.lib user32.lib gdi32.lib kernel32.lib
```

## Support

For issues or questions:
1. Check this README first
2. Verify network connectivity
3. Test with local IP (127.0.0.1) first
4. Check Windows Firewall settings

---

**Version**: 1.0  
**Compatibility**: Windows 7/8/10/11  
**License**: Educational/Personal Use
