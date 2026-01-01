#include "PN532.h"
#include <iostream>
#include <thread>
#include <conio.h>
#include <vector>
#include <string>
#include <deque>

// 清屏
void ClearScreen() {
    system("cls");
}

// 显示程序标题
void ShowTitle() {
    std::cout << "================================================" << std::endl;
    std::cout << "         PN532 NFC读写器 - 稳定版" << std::endl;
    std::cout << "================================================" << std::endl;
}

// 显示操作说明
void ShowInstructions(PN532& nfc) {
    std::cout << "\n 操作说明:" << std::endl;
    std::cout << "  [R] 读取卡片数据" << std::endl;
    std::cout << "  [W] 写入卡片数据 (谨慎!)" << std::endl;
    std::cout << "  [B] 备份卡片数据 (建议写入前备份)" << std::endl;
    std::cout << "  [S] 特殊密钥读取" << std::endl;
    std::cout << "  [K] 配置密钥" << std::endl;
    std::cout << "  [L] " << (nfc.IsLoggingEnabled() ? "关闭" : "开启") << "日志记录" << std::endl;
    std::cout << "  [Enter] 四中水卡金额充值" << std::endl;  // 添加这一行
    std::cout << "  [C] 清屏" << std::endl;
    std::cout << "  [Q] 退出程序" << std::endl;
    std::cout << "================================================" << std::endl;
}

int main() {
    // 显示标题
    ClearScreen();
    ShowTitle();

    // 初始化PN532
    PN532 nfc;
    std::cout << "\nPN532设备初始化..." << std::endl;

    // 询问用户如何选择串口
    std::cout << "\n请选择串口配置方式:" << std::endl;
    std::cout << "1. 自动检测并选择串口" << std::endl;
    std::cout << "2. 手动指定串口 (如 COM5)" << std::endl;
    std::cout << "请选择 (1-2): ";

    std::string choice;
    std::getline(std::cin, choice);

    bool initSuccess = false;

    if (choice == "1") {
        // 自动检测模式
        std::cout << "\n使用自动检测模式..." << std::endl;
        initSuccess = nfc.Initialize("", CBR_115200);
    }
    else if (choice == "2") {
        // 手动指定模式
        std::cout << "\n请输入串口号 (如 COM5): ";
        std::string port;
        std::getline(std::cin, port);

        if (port.empty()) {
            std::cout << "未输入串口号!" << std::endl;
            system("pause");
            return -1;
        }

        // 转换为大写
        for (auto& c : port) {
            c = toupper(c);
        }

        // 检查格式，如果没有COM前缀，加上
        if (port.find("COM") == std::string::npos) {
            port = "COM" + port;
        }

        std::cout << "尝试连接串口: " << port << std::endl;
        initSuccess = nfc.Initialize(port.c_str(), CBR_115200);
    }
    else {
        std::cout << "无效的选择，使用自动检测模式..." << std::endl;
        initSuccess = nfc.Initialize("", CBR_115200);
    }

    if (!initSuccess) {
        std::cerr << "初始化失败!" << std::endl;
        system("pause");
        return -1;
    }

    std::cout << "设备就绪!" << std::endl;

    // 防抖机制相关变量
    const int DEBOUNCE_COUNT = 2;  // 从3减少到2，响应更快
    int detectCounter = 0;
    bool cardPresent = false;
    bool stableCardPresent = false;
    std::vector<unsigned char> cardUID;
    std::vector<unsigned char> lastStableUID;
    int consecutiveFailures = 0;  // 添加连续失败计数器
    const int MAX_FAILURES = 5;   // 最大连续失败次数

    // 显示初始菜单
    ShowInstructions(nfc);
    std::cout << "\n状态: 等待检测..." << std::endl;

    while (true) {
        // 检测卡片
        std::vector<unsigned char> uid;
        bool currentDetect = nfc.DetectNFC(uid);

        // 如果连续失败次数过多，尝试重新初始化
        if (!currentDetect && cardPresent) {
            consecutiveFailures++;
            if (consecutiveFailures >= MAX_FAILURES) {
                std::cout << "\n⚠️ 检测异常，尝试重新初始化..." << std::endl;
                // 这里可以添加重新初始化逻辑
                consecutiveFailures = 0;
            }
        }
        else {
            consecutiveFailures = 0;
        }

        // 防抖逻辑
        if (currentDetect == cardPresent) {
            detectCounter = 0;
        }
        else {
            detectCounter++;

            if (detectCounter >= DEBOUNCE_COUNT) {
                cardPresent = currentDetect;

                if (cardPresent != stableCardPresent) {
                    stableCardPresent = cardPresent;

                    if (stableCardPresent) {
                        // 卡片从无到有
                        cardUID = uid;
                        lastStableUID = uid;

                        std::cout << "\n✅ 检测到卡片!" << std::endl;
                        std::cout << "UID: ";
                        for (auto b : uid) {
                            printf("%02X ", b);
                        }
                        std::cout << std::endl;
                        std::cout << "按 R 读取数据，按 S 特殊密钥读取" << std::endl;

                        // 检测到卡片后立即记录
                        nfc.LogCardInfo(uid, "卡片放置");
                    }
                    else {
                        // 卡片从有到无
                        cardUID.clear();
                        std::cout << "\n📭 卡片已移开" << std::endl;

                        // 记录卡片移开
                        nfc.LogCardInfo(lastStableUID, "卡片移开");
                    }
                }
                detectCounter = 0;
            }
        }

        // 检查按键
        if (_kbhit()) {
            char ch = _getch();
            ch = toupper(ch);

            switch (ch) {
            case 13:  // Enter 键 (ASCII 13)
                if (stableCardPresent && !cardUID.empty()) {
                    std::cout << "\n警告: 特殊写入模式将修改扇区1的块5和块6!" << std::endl;
                    std::cout << "密钥配置将自动设置为:" << std::endl;
                    std::cout << "  扇区1和2: Key A/B = 112233446655" << std::endl;
                    std::cout << "  其他扇区: 默认密钥 FFFFFFFFFFFFF" << std::endl;
                    std::cout << "按 Y 确认继续，其他键取消: ";

                    char confirm = _getch();
                    if (confirm == 'Y' || confirm == 'y') {
                        nfc.SpecialWriteMode();
                    }
                    else {
                        std::cout << "\n操作取消" << std::endl;
                    }

                    std::cout << "\n按任意键继续..." << std::endl;
                    _getch();
                    ClearScreen();
                    ShowTitle();
                    ShowInstructions(nfc);
                    std::cout << "\n状态: 卡片就绪" << std::endl;
                }
                else {
                    std::cout << "\n请先放置卡片!" << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    ClearScreen();
                    ShowTitle();
                    ShowInstructions(nfc);
                    if (stableCardPresent) {
                        std::cout << "\n状态: 卡片就绪" << std::endl;
                    }
                    else {
                        std::cout << "\n状态: 等待检测..." << std::endl;
                    }
                }
                break;

            case 'B':  // 备份卡片数据
                if (cardPresent && !cardUID.empty()) {
                    std::cout << "\n\n开始备份卡片数据..." << std::endl;
                    nfc.BackupCardData(cardUID);
                    std::cout << "\n按任意键继续..." << std::endl;
                    _getch();
                    ClearScreen();
                    ShowTitle();
                    ShowInstructions(nfc);
                }
                else {
                    std::cout << "\n请先放置卡片!" << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
                break;

            case 'W':  // 写入功能
                if (cardPresent && !cardUID.empty()) {
                    std::cout << "\n\n警告: 写入操作可能损坏卡片数据!" << std::endl;
                    std::cout << "请在继续前确认已备份原始数据!" << std::endl;
                    std::cout << "按 Y 继续，其他键取消: ";

                    char confirm = _getch();
                    if (confirm == 'Y' || confirm == 'y') {
                        nfc.WriteCardInteractive(cardUID);
                    }
                    else {
                        std::cout << "\n操作取消" << std::endl;
                    }

                    std::cout << "\n按任意键继续..." << std::endl;
                    _getch();
                    ClearScreen();
                    ShowTitle();
                    ShowInstructions(nfc);
                }
                else {
                    std::cout << "\n请先放置卡片!" << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
                break;
            
            case 'Q':
                std::cout << "\n程序结束!" << std::endl;
                return 0;

            case 'C':
                ClearScreen();
                ShowTitle();
                ShowInstructions(nfc);
                if (stableCardPresent) {
                    std::cout << "\n状态: 卡片就绪" << std::endl;
                }
                else {
                    std::cout << "\n状态: 等待检测..." << std::endl;
                }
                break;

            case 'K':
                std::cout << "\n配置密钥..." << std::endl;
                nfc.SetupKeysFromUserInput();
                std::cout << "\n按任意键继续..." << std::endl;
                _getch();
                ClearScreen();
                ShowTitle();
                ShowInstructions(nfc);
                if (stableCardPresent) {
                    std::cout << "\n状态: 卡片就绪" << std::endl;
                }
                else {
                    std::cout << "\n状态: 等待检测..." << std::endl;
                }
                break;

            case 'R':
                if (stableCardPresent && !cardUID.empty()) {
                    std::cout << "\n读取卡片数据..." << std::endl;
                    nfc.ReadCardDataInteractive(cardUID);
                    std::cout << "\n按任意键继续..." << std::endl;
                    _getch();
                    ClearScreen();
                    ShowTitle();
                    ShowInstructions(nfc);
                    std::cout << "\n状态: 卡片就绪" << std::endl;
                }
                else {
                    std::cout << "\n请先放置卡片!" << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    ClearScreen();
                    ShowTitle();
                    ShowInstructions(nfc);
                    if (stableCardPresent) {
                        std::cout << "\n状态: 卡片就绪" << std::endl;
                    }
                    else {
                        std::cout << "\n状态: 等待检测..." << std::endl;
                    }
                }
                break;

            case 'S':
                if (stableCardPresent && !cardUID.empty()) {
                    std::cout << "\n特殊密钥读取..." << std::endl;
                    nfc.ReadCardWithSpecialKeys(cardUID);
                    std::cout << "\n按任意键继续..." << std::endl;
                    _getch();
                    ClearScreen();
                    ShowTitle();
                    ShowInstructions(nfc);
                    std::cout << "\n状态: 卡片就绪" << std::endl;
                }
                else {
                    std::cout << "\n请先放置卡片!" << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    ClearScreen();
                    ShowTitle();
                    ShowInstructions(nfc);
                    if (stableCardPresent) {
                        std::cout << "\n状态: 卡片就绪" << std::endl;
                    }
                    else {
                        std::cout << "\n状态: 等待检测..." << std::endl;
                    }
                }
                break;

            case 'L':
                nfc.EnableLogging(!nfc.IsLoggingEnabled());
                std::cout << "\n日志记录已" << (nfc.IsLoggingEnabled() ? "启用" : "禁用") << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                ClearScreen();
                ShowTitle();
                ShowInstructions(nfc);
                if (stableCardPresent) {
                    std::cout << "\n状态: 卡片就绪" << std::endl;
                }
                else {
                    std::cout << "\n状态: 等待检测..." << std::endl;
                }
                break;
            }
        }

        // 根据卡片状态调整延迟
        if (stableCardPresent) {
            // 卡片已放置，可以增加延迟，减少系统负载
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        else {
            // 卡片未放置，减少延迟以快速检测
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    }

    return 0;
}