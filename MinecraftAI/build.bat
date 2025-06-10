@echo off
echo ===============================================
echo    Minecraft AI Build Script for Windows
echo ===============================================

REM Check if vcpkg is installed
if not exist "C:\vcpkg\vcpkg.exe" (
    echo ERROR: vcpkg not found at C:\vcpkg\
    echo Please install vcpkg first:
    echo   git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
    echo   cd C:\vcpkg
    echo   .\bootstrap-vcpkg.bat
    echo   .\vcpkg integrate install
    pause
    exit /b 1
)

REM Check if dependencies are installed
echo Checking dependencies...
C:\vcpkg\vcpkg list | findstr opencv >nul
if errorlevel 1 (
    echo Installing OpenCV...
    C:\vcpkg\vcpkg install opencv[contrib]:x64-windows
)

C:\vcpkg\vcpkg list | findstr jsoncpp >nul
if errorlevel 1 (
    echo Installing JsonCpp...
    C:\vcpkg\vcpkg install jsoncpp:x64-windows
)

REM Create build directory
if not exist "build" mkdir build
cd build

REM Configure with CMake
echo Configuring project...
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -G "Visual Studio 17 2022"

if errorlevel 1 (
    echo ERROR: CMake configuration failed
    pause
    exit /b 1
)

REM Build the project
echo Building project...
cmake --build . --config Release

if errorlevel 1 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

REM Copy files to build directory
echo Copying files...
copy ..\web_gui.html . >nul
copy ..\skyblock_stats.json . >nul
copy ..\known_players.txt . >nul

REM Create required directories
if not exist "memory" mkdir memory
if not exist "training_videos" mkdir training_videos
if not exist "logs" mkdir logs
if not exist "config" mkdir config

echo ===============================================
echo           Build completed successfully!
echo ===============================================
echo.
echo To run the AI:
echo   minecraft_ai.exe --gui
echo.
echo Then open your browser to: http://localhost:8080
echo.
pause