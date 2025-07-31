# PowerShell Build Script for Debug Tools
Write-Host "Building Debug Client and Server..." -ForegroundColor Green
Write-Host ""

# Check for compilers
$vsPath = ""
$gccPath = ""

# Try to find Visual Studio
try {
    $vsInstallPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2>$null
    if ($vsInstallPath) {
        $vcvarsPath = "$vsInstallPath\VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcvarsPath) {
            $vsPath = $vcvarsPath
            Write-Host "Found Visual Studio at: $vsInstallPath" -ForegroundColor Cyan
        }
    }
} catch {
    # vswhere not found, continue checking other options
}

# Check for g++
if (Get-Command "g++" -ErrorAction SilentlyContinue) {
    $gccPath = "g++"
    Write-Host "Found g++ compiler" -ForegroundColor Cyan
}

# Build with available compiler
if ($vsPath) {
    Write-Host "Using Visual Studio compiler..." -ForegroundColor Yellow
    
    # Create a temporary batch file to set VS environment and compile
    $tempBatch = @"
@echo off
call "$vsPath"
cl /EHsc /D_WIN32_WINNT=0x0601 debug_server.cpp User32.lib Gdi32.lib Ole32.lib
if %errorlevel% neq 0 exit /b 1
cl /EHsc /D_WIN32_WINNT=0x0601 debug_client.cpp Ws2_32.lib
if %errorlevel% neq 0 exit /b 1
"@
    
    $tempBatch | Out-File -FilePath "temp_build.bat" -Encoding ASCII
    & ".\temp_build.bat"
    $buildResult = $LASTEXITCODE
    Remove-Item "temp_build.bat" -ErrorAction SilentlyContinue
    
} elseif ($gccPath) {
    Write-Host "Using g++ compiler..." -ForegroundColor Yellow
    
    Write-Host "Compiling debug_server.cpp..."
    & g++ -std=c++11 -D_WIN32_WINNT=0x0601 -o debug_server.exe debug_server.cpp -luser32 -lgdi32 -lole32 -static-libgcc -static-libstdc++
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Failed to compile debug_server.cpp" -ForegroundColor Red
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    Write-Host "Compiling debug_client.cpp..."
    & g++ -std=c++11 -D_WIN32_WINNT=0x0601 -o debug_client.exe debug_client.cpp -lws2_32 -static-libgcc -static-libstdc++
    $buildResult = $LASTEXITCODE
    
} else {
    Write-Host "ERROR: No suitable compiler found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "SOLUTIONS:" -ForegroundColor Yellow
    Write-Host "1. Install Visual Studio Community (free)" -ForegroundColor White
    Write-Host "   - Download from: https://visualstudio.microsoft.com/vs/community/" -ForegroundColor Gray
    Write-Host "2. Install MinGW-w64" -ForegroundColor White  
    Write-Host "   - Download from: https://www.msys2.org/" -ForegroundColor Gray
    Write-Host "   - Add to PATH: C:\msys64\mingw64\bin" -ForegroundColor Gray
    Write-Host "3. Use Windows Subsystem for Linux (WSL)" -ForegroundColor White
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

if ($buildResult -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "    BUILD COMPLETED SUCCESSFULLY!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Executables created:" -ForegroundColor Cyan
    Write-Host "- debug_server.exe (run on host computer)" -ForegroundColor White
    Write-Host "- debug_client.exe (run on client computer)" -ForegroundColor White
    Write-Host ""
    Write-Host "NEXT STEPS:" -ForegroundColor Yellow
    Write-Host "1. Run debug_server.exe on the host computer" -ForegroundColor White
    Write-Host "2. Note the password displayed by the server" -ForegroundColor White
    Write-Host "3. Run debug_client.exe on the client computer" -ForegroundColor White
    Write-Host "4. Enter the server IP and password when prompted" -ForegroundColor White
    Write-Host ""
} else {
    Write-Host "ERROR: Build failed!" -ForegroundColor Red
}

Read-Host "Press Enter to continue"
