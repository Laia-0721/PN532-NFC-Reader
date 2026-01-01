#include "SerialPort.h"
#include <iostream>

SerialPort::SerialPort() : hSerial(NULL), connected(false) {
}

SerialPort::~SerialPort() {
    Close();
}

bool SerialPort::Open(const char* portName, DWORD baudRate) {
    // 格式：COM3, COM4等
    std::string port = "\\\\.\\" + std::string(portName);

    // 打开串口
    hSerial = CreateFileA(
        port.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hSerial == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        std::cerr << "打开串口失败! 错误代码: " << error << std::endl;
        return false;
    }

    // 配置串口参数
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "获取串口状态失败!" << std::endl;
        Close();
        return false;
    }

    // 设置串口参数
    dcbSerialParams.BaudRate = baudRate;       // 波特率
    dcbSerialParams.ByteSize = 8;              // 数据位
    dcbSerialParams.StopBits = ONESTOPBIT;     // 停止位
    dcbSerialParams.Parity = NOPARITY;         // 校验位
    dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "设置串口参数失败!" << std::endl;
        Close();
        return false;
    }

    // 设置超时
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;         // 读取间隔超时
    timeouts.ReadTotalTimeoutConstant = 50;    // 读取固定超时
    timeouts.ReadTotalTimeoutMultiplier = 10;  // 读取总超时
    timeouts.WriteTotalTimeoutConstant = 50;   // 写入固定超时
    timeouts.WriteTotalTimeoutMultiplier = 10; // 写入总超时

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        std::cerr << "设置超时失败!" << std::endl;
        Close();
        return false;
    }

    connected = true;
    std::cout << "串口 " << portName << " 打开成功!" << std::endl;
    return true;
}

void SerialPort::Close() {
    if (connected) {
        connected = false;
        CloseHandle(hSerial);
        std::cout << "串口已关闭" << std::endl;
    }
}

int SerialPort::ReadData(char* buffer, unsigned int buf_size) {
    DWORD bytesRead = 0;

    if (!ReadFile(hSerial, buffer, buf_size, &bytesRead, NULL)) {
        ClearCommError(hSerial, &errors, &status);
        return -1;
    }

    return bytesRead;
}

bool SerialPort::WriteData(const char* buffer, unsigned int buf_size) {
    DWORD bytesWritten;

    if (!WriteFile(hSerial, buffer, buf_size, &bytesWritten, NULL)) {
        ClearCommError(hSerial, &errors, &status);
        return false;
    }

    return (bytesWritten == buf_size);
}

bool SerialPort::IsConnected() {
    return connected;
}

// 获取所有可用串口
std::vector<std::string> SerialPort::GetAvailablePorts() {
    std::vector<std::string> ports;

    // 在Windows中，我们可以尝试打开COM1-COM256来检测可用串口
    for (int i = 1; i <= 256; i++) {
        std::string portName = "COM" + std::to_string(i);

        // 尝试打开串口
        HANDLE hSerial = CreateFileA(
            ("\\\\.\\" + portName).c_str(),
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
        }
    }

    return ports;
}

// 检查串口是否存在
bool SerialPort::PortExists(const std::string& portName) {
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
        CloseHandle(hSerial);
        return true;
    }

    return false;
}