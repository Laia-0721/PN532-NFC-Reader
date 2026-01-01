#include <windows.h>
#include <iostream>
#include <vector>
#include <string>

std::vector<std::string> GetAvailablePorts() {
    std::vector<std::string> ports;
    
    std::cout << "正在扫描串口..." << std::endl;
    
    // 尝试检测COM1-COM20
    for (int i = 1; i <= 20; i++) {
        std::string portName = "COM" + std::to_string(i);
        std::string fullPortName = "\\\\.\\" + portName;
        
        HANDLE hSerial = CreateFileA(
            fullPortName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (hSerial != INVALID_HANDLE_VALUE) {
            ports.push_back(portName);
            CloseHandle(hSerial);
            std::cout << "  找到: " << portName << std::endl;
        }
    }
    
    return ports;
}

int main() {
    std::cout << "串口诊断工具" << std::endl;
    std::cout << "============" << std::endl;
    
    std::vector<std::string> ports = GetAvailablePorts();
    
    if (ports.empty()) {
        std::cout << "\n未找到任何串口设备！" << std::endl;
        std::cout << "\n可能原因：" << std::endl;
        std::cout << "1. 没有连接任何串口设备" << std::endl;
        std::cout << "2. 驱动未正确安装" << std::endl;
        std::cout << "3. 设备被其他程序占用" << std::endl;
    } else {
        std::cout << "\n找到 " << ports.size() << " 个串口设备：" << std::endl;
        for (const auto& port : ports) {
            std::cout << "  - " << port << std::endl;
        }
        
        std::cout << "\nCH340设备状态：" << std::endl;
        
        // 检查是否可能是CH340设备
        bool hasCH340 = false;
        for (int i = 1; i <= 20; i++) {
            std::string portName = "COM" + std::to_string(i);
            std::string fullPortName = "\\\\.\\" + portName;
            
            HANDLE hSerial = CreateFileA(
                fullPortName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
            
            if (hSerial != INVALID_HANDLE_VALUE) {
                DCB dcbSerialParams = {0};
                dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
                
                if (GetCommState(hSerial, &dcbSerialParams)) {
                    // 尝试设置115200波特率，这是PN532常用波特率
                    dcbSerialParams.BaudRate = CBR_115200;
                    dcbSerialParams.ByteSize = 8;
                    dcbSerialParams.StopBits = ONESTOPBIT;
                    dcbSerialParams.Parity = NOPARITY;
                    
                    if (SetCommState(hSerial, &dcbSerialParams)) {
                        std::cout << "  " << portName << "：支持115200波特率" << std::endl;
                        hasCH340 = true;
                    }
                }
                
                CloseHandle(hSerial);
            }
        }
        
        if (!hasCH340) {
            std::cout << "  未检测到支持115200波特率的设备" << std::endl;
            std::cout << "  请确保已安装CH340驱动" << std::endl;
        }
    }
    
    std::cout << "\n按任意键退出..." << std::endl;
    std::cin.get();
    
    return 0;
}