@echo off
chcp 65001 >nul
title CH340/CH341 Driver Installer

echo ========================================
echo    CH340/CH341 USB转串口驱动安装工具
echo ========================================
echo.

:: 检查管理员权限
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo 需要管理员权限运行！
    echo 请右键选择"以管理员身份运行"
    pause
    exit /b 1
)

echo 正在检查CH340驱动状态...
echo.

:: 检查是否已安装
reg query "HKLM\SYSTEM\CurrentControlSet\Services\usbser" /v "ImagePath" >nul 2>&1
if %errorLevel% equ 0 (
    echo [✓] USB串口服务已安装
) else (
    echo [✗] USB串口服务未安装
)

echo.
echo 请按以下步骤操作：

echo 1. 访问 http://www.wch.cn/downloads/CH341SER_EXE.html
echo 2. 下载 CH341SER.EXE
echo 3. 运行安装程序
echo 4. 重启电脑
echo.
echo 安装完成后，设备管理器应显示"CH340"或"USB-SERIAL CH340"
echo.

pause