@echo off
chcp 65001 >nul
title PN532 NFC Reader 编译工具

echo PN532 NFC Reader 编译工具
echo =========================
echo.

if "%1"=="vs" (
    echo 使用Visual Studio编译...
    if not exist "build" mkdir build
    cd build
    cmake .. -G "Visual Studio 17 2022" -A x64
    echo.
    echo 编译完成！请在Visual Studio中打开PN532_Reader.sln
    pause
    exit /b 0
)

if "%1"=="mingw" (
    echo 使用MinGW编译...
    if not exist "build" mkdir build
    cd build
    cmake .. -G "MinGW Makefiles"
    mingw32-make
    echo.
    echo 编译完成！可执行文件在 build/bin/ 目录
    pause
    exit /b 0
)

if "%1"=="release" (
    echo 编译Release版本...
    if not exist "build" mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build .
    echo.
    echo 编译完成！可执行文件在 build/bin/ 目录
    pause
    exit /b 0
)

echo 用法：
echo   build.bat vs       - 生成Visual Studio项目
echo   build.bat mingw    - 使用MinGW编译
echo   build.bat release  - 编译Release版本
echo   build.bat          - 显示此帮助
echo.
pause