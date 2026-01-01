# PN532 NFC读写器 - Windows版

![GitHub License](https://img.shields.io/github/license/Laia-0721/PN532-NFC-Reader)
![GitHub Release](https://img.shields.io/github/v/release/Laia-0721/PN532-NFC-Reader)
![Platform](https://img.shields.io/badge/platform-Windows-blue)
![C++](https://img.shields.io/badge/language-C++-green)
![GitHub Stars](https://img.shields.io/github/stars/Laia-0721/PN532-NFC-Reader)

一个功能强大的PN532 NFC读卡器Windows控制台应用程序，支持多种卡片操作。

##  功能特性

-  **智能串口检测** - 自动识别CH340、CP2102等USB转串口设备
-  **完整卡片读写** - 支持读取、写入、备份卡片数据
-  **多密钥支持** - 支持默认密钥、自定义密钥和特殊密钥
-  **详细日志记录** - 自动记录所有操作到日志文件
-  **用户友好界面** - 交互式菜单，操作简单直观
-  **特殊写入模式** - 一键式快速写入特定格式数据(水卡金额覆写)
-  **安全防护** - 写入前警告，支持数据备份

##  界面预览

================================================
PN532 NFC读写器 - 稳定版
================================================

 操作说明:
[R] 读取卡片数据
[W] 写入卡片数据 (谨慎!)
[B] 备份卡片数据 (建议写入前备份)
[S] 特殊密钥读取
[K] 配置密钥
[L] 开启/关闭日志记录
[Enter] 四中水卡金额充值
[C] 清屏
[Q] 退出程序
================================================

状态: 等待检测...


##  快速开始

### 前置要求
- Windows 10/11 64位操作系统
- Visual Studio 2022 或 MinGW
- PN532 NFC读卡器（CH340芯片）

### 安装步骤
1. 克隆项目：
   ```bash
   git clone https://github.com/laia-0721/PN532-NFC-Reader.git
   cd PN532-NFC-Reader

2.编译项目:
# 方法1：使用CMake
mkdir build
cd build
cmake ..
cmake --build .

# 方法2：使用Visual Studio
# 打开项目文件夹，使用Visual Studio编译

3.安装驱动(如果需要):
# 运行驱动安装脚本
tools\install_ch340_driver.bat

4.运行程序:
# 编译后会在build/bin/目录生成可执行文件
PN532_Reader.exe

##详细文档

- 安装指南 - 详细安装说明
- 故障排除 - 常见问题解决方案
- 使用教程 - 完整使用教程

##开发指南

#项目结构
PN532-NFC-Reader/
├── src/           # 源代码
│   ├── Log.h      # 日志系统
│   ├── PN532.h    # PN532主类
│   ├── SerialPort.h # 串口通信
│   └── main.cpp   # 主程序
├── docs/          # 文档
├── tools/         # 工具脚本
└── examples/      # 示例代码

#编译选项
# Debug版本（包含调试信息）
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Release版本（优化性能）
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 64位版本
cmake -B build -A x64

##贡献指南

欢迎贡献代码！请阅读 CONTRIBUTING.md 了解如何参与。

1.Fork 项目

2.创建功能分支 (git checkout -b feature/新功能)

3.提交更改 (git commit -m '添加新功能')

4.推送分支 (git push origin feature/新功能)

5.创建 Pull Request

##许可证

本项目采用 MIT 许可证 - 查看 LICENSE 文件了解详情。

##联系方式

项目主页：https://github.com/laia-0721/PN532-NFC-Reader

问题反馈：https://github.com/laia-0721/PN532-NFC-Reader/issues

邮箱：2062751658@qq.com

##致谢

感谢所有测试和使用本项目的用户！

如果这个项目对您有帮助，请给我们一个 Star！