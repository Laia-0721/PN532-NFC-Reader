#pragma once
#include <windows.h>
#include <string>
#include <vector>

class SerialPort {
private:
    HANDLE hSerial;
    bool connected;
    COMSTAT status;
    DWORD errors;

public:
    SerialPort();
    ~SerialPort();

    // 静态方法：检测所有可用串口
    static std::vector<std::string> GetAvailablePorts();

    // 检查串口是否存在
    static bool PortExists(const std::string& portName);

    // 打开串口
    bool Open(const char* portName, DWORD baudRate = CBR_115200);

    // 关闭串口
    void Close();

    // 读取数据
    int ReadData(char* buffer, unsigned int buf_size);

    // 写入数据
    bool WriteData(const char* buffer, unsigned int buf_size);

    // 检查连接状态
    bool IsConnected();
};