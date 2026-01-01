内容：
```markdown
# 安装指南

## 系统要求
- Windows 10/11 64位
- 至少1G内存
- 512M可用磁盘空间
- USB 2.0或更高版本端口

## 驱动安装

### 自动安装（推荐）
运行工具目录下的安装脚本：
```bash
tools\install_ch340_driver.bat

###手动安装(稳定但麻烦)
1.访问 WCH官网
2.下载 CH341SER.EXE
3.运行安装程序
4.重启电脑

验证安装

成功安装后，设备管理器应显示：

CH340 (COMx)

或 USB-SERIAL CH340 (COMx)

如果显示"通信串口1"或其它串口设备，表示驱动未正确安装。

##编译环境配置

###选项1:Visual Studio 2022(推荐)
1.下载并安装Visual Studio 2022
2.选择"使用C++的桌面开发"工作负载
3.打开项目文件夹
4.按F5编译并运行

###选项2:MinGW
1.安装MinGW-w64
2.添加到系统PATH环境变量
3.编译:g++ -std=c++17 -o PN532_Reader src/*.cpp -lsetupapi -static

###选项3:CMake
# 配置项目
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build

# 运行
.\build\bin\PN532_Reader.exe

##首次运行
1.编译成功后,运行程序
2.连接PN532读卡器到电脑
3.程序会自动检测可用串口
4.按屏幕提示操作