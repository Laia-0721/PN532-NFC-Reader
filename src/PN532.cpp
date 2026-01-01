#include "PN532.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <sstream>
#include <fstream>
#include <conio.h>
#include <string>


PN532::PN532() : baudRate(CBR_115200), useDefaultKeysOnly(true) {
    // 默认启用日志
    logger.Initialize("", true);
}

PN532::~PN532() {
    logger.Close();
}

// 检测可用串口
std::vector<std::string> PN532::DetectAvailablePorts() {
    std::vector<std::string> availablePorts;

    std::cout << "扫描串口..." << std::endl;

    // 使用 SerialPort 类的静态方法获取可用串口
    availablePorts = SerialPort::GetAvailablePorts();

    std::cout << "找到 " << availablePorts.size() << " 个可用串口" << std::endl;

    // 进一步测试哪些串口可能是PN532设备
    std::vector<std::string> pn532Ports;

    for (const auto& port : availablePorts) {
        std::cout << "测试串口 " << port << "... ";

        SerialPort testPort;
        if (testPort.Open(port.c_str(), CBR_115200)) {
            // 尝试发送PN532命令来确认是否是PN532设备
            // 这里可以添加更复杂的设备识别逻辑
            std::cout << "可用";

            // 可以添加测试命令，例如获取固件版本
            // 但为了简单起见，我们假设所有可用串口都是可能的PN532设备
            pn532Ports.push_back(port);

            testPort.Close();
        }
        else {
            std::cout << "不可用";
        }
        std::cout << std::endl;
    }

    return pn532Ports.empty() ? availablePorts : pn532Ports;
}

// 检查指定串口是否可用
bool PN532::TestSerialPort(const char* portName) {
    SerialPort testPort;
    bool result = testPort.Open(portName, CBR_115200);
    if (result) {
        testPort.Close();
    }
    return result;
}

// 日志控制函数
void PN532::EnableLogging(bool enable) {
    logger.SetLogging(enable);
}

bool PN532::IsLoggingEnabled() const {
    return logger.IsLoggingEnabled();
}

std::string PN532::GetLogFileName() const {
    return logger.GetLogFileName();
}

unsigned char PN532::CalculateChecksum(const std::vector<unsigned char>& data) {
    unsigned char sum = 0;
    for (auto byte : data) {
        sum += byte;
    }
    return (0x100 - sum) & 0xFF;  // 低8位补数
}

std::vector<unsigned char> PN532::BuildFrame(const std::vector<unsigned char>& data) {
    std::vector<unsigned char> frame;

    // 添加前导码和起始码
    frame.push_back(PREAMBLE);
    frame.push_back(STARTCODE1);
    frame.push_back(STARTCODE2);

    // 数据长度和长度校验
    unsigned char len = data.size();
    unsigned char len_checksum = (0x100 - len) & 0xFF;

    frame.push_back(len);
    frame.push_back(len_checksum);

    // 添加数据
    for (auto byte : data) {
        frame.push_back(byte);
    }

    // 计算数据校验和
    unsigned char data_checksum = CalculateChecksum(data);
    frame.push_back(data_checksum);

    // 添加后导码
    frame.push_back(POSTAMBLE);

    return frame;
}

bool PN532::ParseFrame(const std::vector<unsigned char>& response,
    std::vector<unsigned char>& data) {
    data.clear();

    // 去掉调试输出
    // std::cout << "解析帧，数据长度: " << response.size() << std::endl;

    // 寻找有效的PN532帧
    for (int i = 0; i < (int)response.size() - 5; i++) {
        if (response[i] == 0x00 && response[i + 1] == 0xFF &&
            (response[i + 4] == 0xD4 || response[i + 4] == 0xD5)) {

            unsigned char len = response[i + 2];
            unsigned char lcs = response[i + 3];

            // 检查长度校验
            if (((len + lcs) & 0xFF) != 0) continue;

            // 检查数据长度
            if (i + 4 + len + 1 >= response.size()) continue;

            // 提取数据
            for (int j = 0; j < len; j++) {
                data.push_back(response[i + 4 + j]);
            }

            // 验证校验和
            unsigned char received_checksum = response[i + 4 + len];
            unsigned char calculated_checksum = CalculateChecksum(data);

            if (received_checksum == calculated_checksum) {
                // 去掉成功解析的输出
                // std::cout << "成功解析帧，位置: " << i << std::endl;
                return true;
            }

            data.clear();
        }
    }

    return false;
}

bool PN532::Initialize(const char* port, DWORD baud) {
    comPort = port;
    baudRate = baud;

    // 如果未指定端口，尝试自动检测
    if (std::string(port).empty()) {
        std::cout << "正在自动检测串口..." << std::endl;

        std::vector<std::string> availablePorts = DetectAvailablePorts();

        if (availablePorts.empty()) {
            logger.Log("未找到可用串口", 2);
            std::cout << "❌ 未找到可用串口!" << std::endl;
            return false;
        }

        // 如果有多个串口，让用户选择
        if (availablePorts.size() > 1) {
            std::cout << "\n检测到多个串口，请选择:" << std::endl;
            for (size_t i = 0; i < availablePorts.size(); i++) {
                std::cout << "  [" << i + 1 << "] " << availablePorts[i] << std::endl;
            }

            std::cout << "\n请选择串口号 (1-" << availablePorts.size() << "): ";
            int choice;
            std::cin >> choice;
            std::cin.ignore();

            if (choice < 1 || choice > static_cast<int>(availablePorts.size())) {
                std::cout << "❌ 无效的选择!" << std::endl;
                return false;
            }

            comPort = availablePorts[choice - 1];
        }
        else {
            // 只有一个串口，自动选择
            comPort = availablePorts[0];
        }

        std::cout << "选择串口: " << comPort << std::endl;
    }
    else {
        comPort = port;
    }

    std::string msg = "尝试打开串口: " + comPort;
    logger.Log(msg, 0);

    if (!serial.Open(comPort.c_str(), baud)) {
        logger.Log("串口打开失败", 2);
        return false;
    }

    logger.Log("串口打开成功", 0);
    logger.Log("等待模块初始化...", 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    return true;
}

bool PN532::GetFirmwareVersion(std::vector<unsigned char>& version) {
    logger.Log("获取PN532固件版本...", 0);
    std::vector<unsigned char> command = { HOSTTOPN532, CMD_GETFIRMWAREVERSION };
    std::vector<unsigned char> frame = BuildFrame(command);

    // 发送命令
    std::cout << "发送固件版本请求..." << std::endl;
    if (!serial.WriteData((char*)frame.data(), frame.size())) {
        std::cerr << "发送命令失败!" << std::endl;
        return false;
    }

    // 等待响应
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 读取响应
    char buffer[256];
    int bytesRead = serial.ReadData(buffer, sizeof(buffer));

    std::cout << "读取到 " << bytesRead << " 字节" << std::endl;

    if (bytesRead <= 0) {
        std::cerr << "没有收到响应!" << std::endl;
        return false;
    }

    // 解析响应
    std::vector<unsigned char> response(buffer, buffer + bytesRead);
    std::vector<unsigned char> data;

    if (!ParseFrame(response, data)) {
        std::cerr << "解析响应失败!" << std::endl;
        return false;
    }

    // 检查响应数据
    if (data.size() < 6) { // D5 + 命令响应 + 4字节固件信息
        std::cerr << "响应数据太短: " << data.size() << " 字节" << std::endl;
        return false;
    }

    // 检查TFI和命令响应
    if (data[0] != PN532TOHOST) {
        std::cerr << "错误的TFI: " << std::hex << (int)data[0] << std::dec << std::endl;
        return false;
    }

    if (data[1] != CMD_GETFIRMWAREVERSION + 1) {
        std::cerr << "错误的命令响应: 期望 " << std::hex
            << (int)(CMD_GETFIRMWAREVERSION + 1)
            << ", 收到 " << (int)data[1] << std::dec << std::endl;
        return false;
    }

    // 提取固件版本
    version.assign(data.begin() + 2, data.end());

    std::cout << "固件版本: ";
    for (auto byte : version) {
        printf("%02X ", byte);
    }
    std::cout << std::endl;

    // 解释固件版本
    if (version.size() >= 4) {
        std::cout << "IC版本: " << std::hex << (int)version[0] << std::dec << std::endl;
        std::cout << "版本: " << (int)version[1] << "." << (int)version[2] << std::endl;
        std::cout << "支持的功能: " << std::hex << (int)version[3] << std::dec << std::endl;
    }
    if (!ParseFrame(response, data)) {
        logger.Log("解析固件版本响应失败", 2);
        return false;
    }

    // 记录固件版本
    if (data.size() >= 6) {
        std::stringstream ss;
        ss << "PN532固件版本: ";
        for (size_t i = 2; i < data.size(); i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
        }
        logger.Log(ss.str(), 0);

        // 记录到文件
        logger.LogToFile("设备固件: " + ss.str(), 0);
    }

    return true;
}

bool PN532::SAMConfiguration() {
    std::vector<unsigned char> command = {
        HOSTTOPN532,
        CMD_SAMCONFIGURATION,
        0x01,  // 正常模式
        0x14,  // 超时50ms * 20 = 1000ms
        0x01   // 使用外部IRQ
    };

    std::vector<unsigned char> frame = BuildFrame(command);

    if (!serial.WriteData((char*)frame.data(), frame.size())) {
        std::cerr << "发送SAM配置命令失败!" << std::endl;
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    char buffer[256];
    int bytesRead = serial.ReadData(buffer, sizeof(buffer));

    if (bytesRead <= 0) {
        std::cerr << "读取SAM配置响应失败!" << std::endl;
        return false;
    }

    std::vector<unsigned char> response(buffer, buffer + bytesRead);
    std::vector<unsigned char> data;

    if (!ParseFrame(response, data)) {
        std::cerr << "解析SAM配置响应失败!" << std::endl;
        return false;
    }

    if (data.size() < 2 || data[0] != PN532TOHOST || data[1] != CMD_SAMCONFIGURATION + 1) {
        std::cerr << "SAM配置失败!" << std::endl;
        return false;
    }

    std::cout << "SAM配置成功!" << std::endl;
    return true;
}

bool PN532::DetectNFC(std::vector<unsigned char>& uid) {
    // 清空UID
    uid.clear();

    // 尝试多次检测
    const int MAX_RETRIES = 3;

    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        // 发送检测命令
        std::vector<unsigned char> command = {
            HOSTTOPN532,
            CMD_INLISTPASSIVETARGET,
            0x01,  // 最大目标数
            0x00   // 106kbps Type A
        };

        std::vector<unsigned char> frame = BuildFrame(command);

        // 发送命令
        if (!serial.WriteData((char*)frame.data(), frame.size())) {
            if (retry == MAX_RETRIES - 1) {
                logger.Log("发送检测命令失败", 2);
            }
            continue;
        }

        // 根据重试次数调整等待时间
        int waitTime = 100 + (retry * 50);  // 第一次100ms，第二次150ms，第三次200ms
        std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));

        // 读取响应
        char buffer[256];
        int bytesRead = serial.ReadData(buffer, sizeof(buffer));

        if (bytesRead <= 0) {
            if (retry == MAX_RETRIES - 1) {
                // 最后一次尝试也失败了
                return false;
            }
            continue;  // 继续重试
        }

        std::vector<unsigned char> response(buffer, buffer + bytesRead);
        std::vector<unsigned char> data;

        if (!ParseFrame(response, data)) {
            continue;
        }

        // 检查响应数据
        if (data.size() < 12) {
            continue;
        }

        // 检查是否是有效响应
        if (data[0] != 0xD5 || data[1] != 0x4B) {
            continue;
        }

        // 检查目标数
        unsigned char targetCount = data[2];
        if (targetCount == 0) {
            return false;  // 没有检测到卡片
        }

        // 获取NFCID长度
        unsigned char nfcidLength = data[7];

        if (nfcidLength > 0 && data.size() >= 8 + nfcidLength) {
            for (int i = 0; i < nfcidLength; i++) {
                uid.push_back(data[8 + i]);
            }

            // 验证UID有效性（确保不是全0或全F）
            bool isValidUID = false;
            for (auto byte : uid) {
                if (byte != 0x00 && byte != 0xFF) {
                    isValidUID = true;
                    break;
                }
            }

            if (!isValidUID) {
                uid.clear();
                if (retry < MAX_RETRIES - 1) {
                    continue;  // 继续重试
                }
                return false;
            }

            // 记录检测成功
            if (retry > 0) {
                std::stringstream retryMsg;
                retryMsg << "卡片检测成功，重试次数: " << retry + 1;
                logger.Log(retryMsg.str(), 3);  // DEBUG级别
            }

            return true;
        }
    }

    return false;
}

bool PN532::MifareAuthenticate(const std::vector<unsigned char>& uid,
    uint8_t blockNumber,
    uint8_t keyType,
    const unsigned char* key) {
    // 如果没有提供密钥，使用默认密钥A
    if (key == nullptr) {
        key = DEFAULT_KEY_A;
    }

    // 构建认证命令
    std::vector<unsigned char> command = {
        HOSTTOPN532,
        CMD_INDATAEXCHANGE,
        0x01,  // 目标编号（通常是1）
        keyType,  // 认证类型：0x60 = Key A, 0x61 = Key B
        blockNumber
    };

    // 添加密钥
    for (int i = 0; i < 6; i++) {
        command.push_back(key[i]);
    }

    // 添加UID（用于认证）
    for (auto byte : uid) {
        command.push_back(byte);
    }

    std::vector<unsigned char> frame = BuildFrame(command);

    // 发送认证命令
    if (!serial.WriteData((char*)frame.data(), frame.size())) {
        std::cerr << "发送认证命令失败!" << std::endl;
        return false;
    }

    // 等待响应
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    char buffer[256];
    int bytesRead = serial.ReadData(buffer, sizeof(buffer));

    if (bytesRead <= 0) {
        std::cerr << "读取认证响应失败!" << std::endl;
        return false;
    }

    std::vector<unsigned char> response(buffer, buffer + bytesRead);
    std::vector<unsigned char> data;

    if (!ParseFrame(response, data)) {
        std::cerr << "解析认证响应失败!" << std::endl;
        return false;
    }

    // 检查响应
    if (data.size() < 3) {
        std::cerr << "认证响应数据太短!" << std::endl;
        return false;
    }

    // 检查是否是正确响应
    if (data[0] != PN532TOHOST || data[1] != CMD_INDATAEXCHANGE + 1) {
        std::cerr << "认证响应命令错误!" << std::endl;
        return false;
    }

    // 检查状态（第三个字节应为0x00表示成功）
    if (data[2] != 0x00) {
        std::cerr << "认证失败! 错误代码: " << std::hex << (int)data[2] << std::dec << std::endl;
        return false;
    }

    std::cout << "扇区 " << (blockNumber / 4) << " 认证成功!" << std::endl;
    return true;
}

bool PN532::MifareReadBlock(uint8_t blockNumber, std::vector<unsigned char>& data) {
    data.clear();

    // 构建读取命令
    std::vector<unsigned char> command = {
        HOSTTOPN532,
        CMD_INDATAEXCHANGE,
        0x01,  // 目标编号
        CMD_MIFARE_READ,
        blockNumber
    };

    std::vector<unsigned char> frame = BuildFrame(command);

    // 发送读取命令
    if (!serial.WriteData((char*)frame.data(), frame.size())) {
        std::cerr << "发送读取命令失败!" << std::endl;
        return false;
    }

    // 等待响应
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    char buffer[256];
    int bytesRead = serial.ReadData(buffer, sizeof(buffer));

    if (bytesRead <= 0) {
        std::cerr << "读取响应失败!" << std::endl;
        return false;
    }

    std::vector<unsigned char> response(buffer, buffer + bytesRead);
    std::vector<unsigned char> responseData;

    if (!ParseFrame(response, responseData)) {
        std::cerr << "解析读取响应失败!" << std::endl;
        return false;
    }

    // 检查响应
    if (responseData.size() < 3) {
        std::cerr << "读取响应数据太短!" << std::endl;
        return false;
    }

    // 检查是否是正确响应
    if (responseData[0] != PN532TOHOST || responseData[1] != CMD_INDATAEXCHANGE + 1) {
        std::cerr << "读取响应命令错误!" << std::endl;
        return false;
    }

    // 检查状态（第三个字节应为0x00表示成功）
    if (responseData[2] != 0x00) {
        std::cerr << "读取失败! 错误代码: " << std::hex << (int)responseData[2] << std::dec << std::endl;
        return false;
    }

    // 提取数据（16字节）
    if (responseData.size() >= 19) {  // 3字节头 + 16字节数据
        for (int i = 3; i < 19; i++) {
            data.push_back(responseData[i]);
        }
        return true;
    }

    return false;
}

bool PN532::MifareReadSector(uint8_t sector, std::vector<std::vector<unsigned char>>& blocks) {
    blocks.clear();

    // Mifare 1K有16个扇区，每个扇区4个块
    if (sector >= 16) {
        std::cerr << "无效的扇区号!" << std::endl;
        return false;
    }

    // 扇区的第一个块号
    uint8_t startBlock = sector * 4;

    std::cout << "读取扇区 " << (int)sector << " (块 " << (int)startBlock << " 到 " << (int)(startBlock + 3) << ")" << std::endl;

    // 读取扇区的所有4个块
    for (int i = 0; i < 4; i++) {
        uint8_t blockNumber = startBlock + i;
        std::vector<unsigned char> blockData;

        if (MifareReadBlock(blockNumber, blockData)) {
            blocks.push_back(blockData);
            std::cout << "块 " << (int)blockNumber << ": ";
            for (auto byte : blockData) {
                printf("%02X ", byte);
            }
            std::cout << std::endl;
        }
        else {
            std::cerr << "读取块 " << (int)blockNumber << " 失败!" << std::endl;
            return false;
        }

        // 块之间稍作延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return true;
}

bool PN532::TryAuthenticateSector(const std::vector<unsigned char>& uid,
    uint8_t sector,
    uint8_t& successfulKeyType,
    std::vector<unsigned char>& successfulKey) {
    uint8_t sectorFirstBlock = sector * 4;

    // 如果没有为该扇区配置密钥，使用默认密钥
    if (sectorKeys.find(sector) == sectorKeys.end()) {
        // 添加默认密钥
        AddDefaultKeyA(sector);
        AddDefaultKeyB(sector);
    }

    // 获取该扇区的所有密钥
    std::vector<unsigned char>& keys = sectorKeys[sector];

    // 检查是否有密钥配置
    if (keys.empty()) {
        std::cout << "扇区 " << (int)sector << " 没有配置密钥" << std::endl;
        return false;
    }

    // 每个密钥条目7字节：类型(1) + 密钥(6)
    int keyCount = keys.size() / 7;

    std::cout << "扇区 " << (int)sector << " 有 " << keyCount << " 个密钥待尝试" << std::endl;

    for (int keyIndex = 0; keyIndex < keyCount; keyIndex++) {
        int baseIndex = keyIndex * 7;
        uint8_t keyType = keys[baseIndex];
        std::vector<unsigned char> key(keys.begin() + baseIndex + 1, keys.begin() + baseIndex + 7);

        std::cout << "尝试密钥 " << (keyIndex + 1) << ": ";
        std::cout << (keyType == 0x60 ? "Key A" : "Key B") << " ";
        for (auto k : key) {
            printf("%02X ", k);
        }
        std::cout << std::endl;

        // 构建认证命令
        std::vector<unsigned char> command = {
            HOSTTOPN532,
            CMD_INDATAEXCHANGE,
            0x01,  // 目标编号
            keyType,
            sectorFirstBlock
        };

        // 添加密钥
        for (auto k : key) {
            command.push_back(k);
        }

        // 添加UID
        for (auto byte : uid) {
            command.push_back(byte);
        }

        std::vector<unsigned char> frame = BuildFrame(command);

        // 发送认证命令
        if (!serial.WriteData((char*)frame.data(), frame.size())) {
            std::cout << "发送认证命令失败" << std::endl;
            continue;
        }

        // 等待响应
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        char buffer[256];
        int bytesRead = serial.ReadData(buffer, sizeof(buffer));

        if (bytesRead <= 0) {
            std::cout << "没有响应" << std::endl;
            continue;
        }

        std::vector<unsigned char> response(buffer, buffer + bytesRead);
        std::vector<unsigned char> data;

        if (!ParseFrame(response, data)) {
            std::cout << "解析响应失败" << std::endl;
            continue;
        }

        // 检查响应
        if (data.size() >= 3 &&
            data[0] == PN532TOHOST &&
            data[1] == CMD_INDATAEXCHANGE + 1) {

            // 检查认证结果
            if (data[2] == 0x00) {  // 0x00表示认证成功
                // 认证成功
                successfulKeyType = keyType;
                successfulKey = key;

                std::cout << "✅ 扇区 " << (int)sector << " 认证成功! ";
                std::cout << "使用密钥: " << (keyType == 0x60 ? "Key A" : "Key B") << " ";
                for (auto k : key) {
                    printf("%02X ", k);
                }
                std::cout << std::endl;

                // 记录成功的认证到日志
                std::stringstream authMsg;
                authMsg << "扇区 " << (int)sector << " 认证成功 - 密钥: ";
                authMsg << (keyType == 0x60 ? "Key A " : "Key B ");
                for (auto k : key) {
                    authMsg << std::hex << std::setw(2) << std::setfill('0') << (int)k << " ";
                }
                logger.Log(authMsg.str(), 0);

                return true;
            }
            else {
                std::cout << "认证失败，错误码: " << std::hex << (int)data[2] << std::dec << std::endl;
                // 记录认证失败到日志
                std::stringstream failMsg;
                failMsg << "扇区 " << (int)sector << " 认证失败 - 错误码: 0x" << std::hex << (int)data[2];
                logger.Log(failMsg.str(), 2);
            }
        }
        else {
            std::cout << "无效的响应格式" << std::endl;
            logger.Log("扇区 " + std::to_string(sector) + " 认证响应格式无效", 2);
        }
    }

    std::cout << "❌ 扇区 " << (int)sector << " 所有密钥尝试失败" << std::endl;
    logger.Log("扇区 " + std::to_string(sector) + " 所有密钥尝试失败", 2);
    return false;
}

// 写入单个数据块
bool PN532::MifareWriteBlock(uint8_t blockNumber, const std::vector<unsigned char>& data) {
    // 检查数据长度（必须是16字节）
    if (data.size() != 16) {
        std::cout << "错误: 写入数据必须是16字节!" << std::endl;
        return false;
    }

    // 检查是否是控制块（每个扇区的第4个块）
    if ((blockNumber + 1) % 4 == 0) {
        std::cout << "警告: 尝试写入控制块! 这可能会永久锁死卡片!" << std::endl;
        std::cout << "按 Y 确认写入控制块，其他键取消: ";
        char ch = _getch();
        if (ch != 'Y' && ch != 'y') {
            std::cout << "操作取消" << std::endl;
            return false;
        }
    }

    // 构建写入命令
    std::vector<unsigned char> command = {
        HOSTTOPN532,
        CMD_INDATAEXCHANGE,
        0x01,  // 目标编号
        CMD_MIFARE_WRITE,
        blockNumber
    };

    // 添加数据
    for (auto byte : data) {
        command.push_back(byte);
    }

    std::vector<unsigned char> frame = BuildFrame(command);

    // 发送写入命令
    if (!serial.WriteData((char*)frame.data(), frame.size())) {
        std::cout << "发送写入命令失败!" << std::endl;
        return false;
    }

    // 等待响应
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    char buffer[256];
    int bytesRead = serial.ReadData(buffer, sizeof(buffer));

    if (bytesRead <= 0) {
        std::cout << "读取写入响应失败!" << std::endl;
        return false;
    }

    std::vector<unsigned char> response(buffer, buffer + bytesRead);
    std::vector<unsigned char> responseData;

    if (!ParseFrame(response, responseData)) {
        std::cout << "解析写入响应失败!" << std::endl;
        return false;
    }

    // 检查响应
    if (responseData.size() < 3) {
        std::cout << "写入响应数据太短!" << std::endl;
        return false;
    }

    // 检查是否是正确响应
    if (responseData[0] != PN532TOHOST || responseData[1] != CMD_INDATAEXCHANGE + 1) {
        std::cout << "写入响应命令错误!" << std::endl;
        return false;
    }

    // 检查状态（第三个字节应为0x00表示成功）
    if (responseData[2] != 0x00) {
        std::cout << "写入失败! 错误代码: 0x" << std::hex << (int)responseData[2] << std::dec << std::endl;
        return false;
    }

    std::cout << "✅ 块 " << (int)blockNumber << " 写入成功!" << std::endl;
    logger.Log("块 " + std::to_string(blockNumber) + " 写入成功", 0);

    return true;
}

// 写入值块（用于电子钱包功能）
bool PN532::MifareWriteValueBlock(uint8_t blockNumber, int32_t value) {
    // 值块格式: 4字节值 + 取反值 + 地址 + 取反地址
    std::vector<unsigned char> data(16, 0x00);

    // 设置值（小端格式）
    data[0] = (value >> 0) & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;

    // 设置取反值
    data[4] = ~data[0];
    data[5] = ~data[1];
    data[6] = ~data[2];
    data[7] = ~data[3];

    // 设置地址（通常为0）
    data[8] = 0x00;
    data[9] = 0x00;

    // 设置取反地址
    data[10] = 0xFF;
    data[11] = 0xFF;

    // 其余字节为0
    for (int i = 12; i < 16; i++) {
        data[i] = 0x00;
    }

    return MifareWriteBlock(blockNumber, data);
}

// 写入整个扇区
bool PN532::MifareWriteSector(uint8_t sector, const std::vector<std::vector<unsigned char>>& blocks) {
    if (sector >= 16) {
        std::cout << "无效的扇区号!" << std::endl;
        return false;
    }

    if (blocks.size() != 4) {
        std::cout << "必须提供4个块的数据!" << std::endl;
        return false;
    }

    uint8_t startBlock = sector * 4;
    bool success = true;

    std::cout << "写入扇区 " << (int)sector << " (块 " << (int)startBlock << " 到 " << (int)(startBlock + 3) << ")" << std::endl;

    for (int i = 0; i < 4; i++) {
        uint8_t blockNumber = startBlock + i;

        // 跳过厂商块（扇区0块0）
        if (blockNumber == 0) {
            std::cout << "跳过厂商块（只读）" << std::endl;
            continue;
        }

        std::cout << "写入块 " << (int)blockNumber << ": ";
        for (auto b : blocks[i]) {
            printf("%02X ", b);
        }
        std::cout << std::endl;

        if (!MifareWriteBlock(blockNumber, blocks[i])) {
            std::cout << "写入块 " << (int)blockNumber << " 失败!" << std::endl;
            success = false;
            break;
        }

        // 块之间延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return success;
}

// 修改扇区密钥
bool PN532::ChangeSectorKeys(uint8_t sector,
    const std::vector<unsigned char>& keyA,
    const std::vector<unsigned char>& keyB,
    const std::vector<unsigned char>& accessBits) {
    // 检查参数
    if (keyA.size() != 6 || keyB.size() != 6 || accessBits.size() != 4) {
        std::cout << "密钥和访问位长度错误!" << std::endl;
        return false;
    }

    if (sector >= 16) {
        std::cout << "无效的扇区号!" << std::endl;
        return false;
    }

    uint8_t blockNumber = sector * 4 + 3;  // 控制块

    std::cout << "⚠️ 警告: 正在修改扇区 " << sector << " 的控制块!" << std::endl;
    std::cout << "此操作可能永久锁死卡片!" << std::endl;
    std::cout << "按 Y 确认，其他键取消: ";

    char ch = _getch();
    if (ch != 'Y' && ch != 'y') {
        std::cout << "操作取消" << std::endl;
        return false;
    }

    // 构建控制块数据
    std::vector<unsigned char> controlBlock(16, 0x00);

    // 密钥A
    for (int i = 0; i < 6; i++) {
        controlBlock[i] = keyA[i];
    }

    // 访问控制位（需要3字节，但我们提供4字节）
    controlBlock[6] = accessBits[0];
    controlBlock[7] = accessBits[1];
    controlBlock[8] = accessBits[2];
    controlBlock[9] = accessBits[3];  // 未使用，通常为0x00

    // 密钥B
    for (int i = 0; i < 6; i++) {
        controlBlock[10 + i] = keyB[i];
    }

    std::cout << "新的控制块: ";
    for (auto b : controlBlock) {
        printf("%02X ", b);
    }
    std::cout << std::endl;

    return MifareWriteBlock(blockNumber, controlBlock);
}

// 计算访问控制位
std::vector<unsigned char> PN532::CalculateAccessBits(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    std::vector<unsigned char> accessBits(4, 0x00);

    // 访问控制字节
    accessBits[0] = (~b2 & 0x0F) | ((~b1 & 0x0F) << 4);
    accessBits[1] = (~b1 & 0xF0) | ((~b0 & 0x0F) << 4) | (b2 & 0x0F);
    accessBits[2] = (b0 & 0xF0) | (b1 & 0x0F);
    accessBits[3] = 0x00;  // 未使用

    return accessBits;
}

// 显示访问控制位
void PN532::DisplayAccessBits(uint8_t sector) {
    // 读取控制块
    uint8_t blockNumber = sector * 4 + 3;
    std::vector<unsigned char> controlBlock;

    if (MifareReadBlock(blockNumber, controlBlock) && controlBlock.size() >= 16) {
        std::cout << "扇区 " << sector << " 访问控制位: ";
        for (int i = 6; i < 10; i++) {
            printf("%02X ", controlBlock[i]);
        }
        std::cout << std::endl;

        // 解析访问控制位
        uint8_t accessBits[4];
        for (int i = 0; i < 4; i++) {
            accessBits[i] = controlBlock[6 + i];
        }

        // 简化解析（实际解析较复杂）
        std::cout << "块权限:" << std::endl;
        std::cout << "  块0-2: ";
        switch (accessBits[0] & 0x0F) {
        case 0x00: std::cout << "KeyA可读写，KeyB可读写"; break;
        case 0x01: std::cout << "KeyA可读写，KeyB可读"; break;
        case 0x02: std::cout << "KeyA可读写，KeyB可写"; break;
        case 0x03: std::cout << "KeyA可读写，KeyB无访问"; break;
        default: std::cout << "未知权限";
        }
        std::cout << std::endl;
    }
    else {
        std::cout << "无法读取访问控制位" << std::endl;
    }
}

void PN532::ReadCardAllData(const std::vector<unsigned char>& uid) {
    std::cout << "\n=== 读取CUID卡数据 ===" << std::endl;
    std::cout << "卡UID: ";
    for (auto byte : uid) {
        printf("%02X ", byte);
    }
    std::cout << std::endl;

    // 尝试读取所有扇区（0-15）
    for (int sector = 0; sector < 16; sector++) {
        // 每个扇区需要单独认证
        uint8_t sectorFirstBlock = sector * 4;

        // 尝试认证扇区（使用默认Key A）
        if (MifareAuthenticate(uid, sectorFirstBlock, 0x60, DEFAULT_KEY_A)) {
            std::vector<std::vector<unsigned char>> blocks;

            if (MifareReadSector(sector, blocks)) {
                // 显示扇区数据
                std::cout << "\n扇区 " << sector << " 数据:" << std::endl;
                for (size_t i = 0; i < blocks.size(); i++) {
                    std::cout << "  块 " << (sector * 4 + i) << ": ";

                    // 如果是数据块，尝试解析为文本
                    if (i < 3) {  // 前3个块是数据块
                        // 检查是否有可打印文本
                        bool hasPrintable = false;
                        for (auto byte : blocks[i]) {
                            if (byte >= 32 && byte <= 126) {
                                hasPrintable = true;
                                break;
                            }
                        }

                        // 显示十六进制
                        for (auto byte : blocks[i]) {
                            printf("%02X ", byte);
                        }

                        // 如果包含可打印字符，显示ASCII
                        if (hasPrintable) {
                            std::cout << "  ASCII: ";
                            for (auto byte : blocks[i]) {
                                if (byte >= 32 && byte <= 126) {
                                    std::cout << (char)byte;
                                }
                                else {
                                    std::cout << ".";
                                }
                            }
                        }
                    }
                    else {  // 第4个块是控制块
                        std::cout << "[控制块] ";
                        for (auto byte : blocks[i]) {
                            printf("%02X ", byte);
                        }

                        // 解析控制块
                        if (blocks[i].size() >= 16) {
                            std::cout << "\n        Key A: ";
                            for (int j = 0; j < 6; j++) printf("%02X ", blocks[i][j]);

                            std::cout << "  Access Bits: ";
                            for (int j = 6; j < 9; j++) printf("%02X ", blocks[i][j]);

                            std::cout << "  Key B: ";
                            for (int j = 10; j < 16; j++) printf("%02X ", blocks[i][j]);
                        }
                    }
                    std::cout << std::endl;
                }
            }
        }
        else {
            std::cout << "扇区 " << sector << " 认证失败（可能密钥不同）" << std::endl;
        }

        // 扇区间延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void PN532::SetupKeysFromUserInput() {
    ClearAllKeys();

    std::cout << "\n=== 密钥设置 ===" << std::endl;
    logger.Log("开始密钥设置", 0);

    std::cout << "请选择密钥模式:" << std::endl;
    std::cout << "1. 仅使用默认密钥 (FFFFFFFFFFFF)" << std::endl;
    std::cout << "2. 使用默认密钥 + 自定义密钥" << std::endl;
    std::cout << "3. 仅使用自定义密钥" << std::endl;
    std::cout << "请选择 (1-3): ";

    int choice;
    std::cin >> choice;
    std::cin.ignore();  // 清除换行符

    bool addDefaultKeys = (choice == 1 || choice == 2);
    bool addCustomKeys = (choice == 2 || choice == 3);

    // 记录用户选择
    std::string modeStr;
    switch (choice) {
    case 1: modeStr = "仅默认密钥"; break;
    case 2: modeStr = "默认+自定义密钥"; break;
    case 3: modeStr = "仅自定义密钥"; break;
    default: modeStr = "未知模式";
    }
    logger.Log("用户选择密钥模式: " + modeStr, 0);

    if (addDefaultKeys) {
        std::cout << "添加默认密钥到所有扇区..." << std::endl;
        logger.Log("添加默认密钥到所有扇区", 0);

        for (int sector = 0; sector < 16; sector++) {
            AddDefaultKeyA(sector);
            AddDefaultKeyB(sector);
        }

        logger.LogToFile("已添加默认密钥FFFFFFFFFFFF到所有扇区", 0);
    }

    if (addCustomKeys) {
        std::cout << "\n=== 添加自定义密钥 ===" << std::endl;
        std::cout << "密钥格式: 12位十六进制数字 (例如: FFFFFFFFFFFF)" << std::endl;
        std::cout << "输入 'q' 结束输入" << std::endl;

        logger.Log("开始添加自定义密钥", 0);

        while (true) {
            std::cout << "\n输入密钥: ";
            std::string input;
            std::getline(std::cin, input);

            if (input == "q" || input == "Q") {
                std::cout << "结束密钥输入" << std::endl;
                logger.Log("结束自定义密钥输入", 0);
                break;
            }

            // 验证输入格式
            if (input.length() != 12) {
                std::cout << "错误: 密钥必须是12位十六进制数字!" << std::endl;
                logger.Log("密钥输入错误: 长度必须是12位", 2);
                continue;
            }

            bool valid = true;
            for (char c : input) {
                if (!((c >= '0' && c <= '9') ||
                    (c >= 'A' && c <= 'F') ||
                    (c >= 'a' && c <= 'f'))) {
                    valid = false;
                    break;
                }
            }

            if (!valid) {
                std::cout << "错误: 包含无效字符!" << std::endl;
                logger.Log("密钥输入错误: 包含无效字符", 2);
                continue;
            }

            // 转换为字节
            std::vector<unsigned char> key;
            for (int i = 0; i < 12; i += 2) {
                std::string byteStr = input.substr(i, 2);
                unsigned char byte = (unsigned char)std::strtoul(byteStr.c_str(), nullptr, 16);
                key.push_back(byte);
            }

            // 记录密钥
            std::stringstream keyMsg;
            keyMsg << "自定义密钥: ";
            for (auto k : key) {
                keyMsg << std::hex << std::setw(2) << std::setfill('0') << (int)k << " ";
            }
            logger.Log(keyMsg.str(), 0);

            // 选择密钥类型
            std::cout << "密钥类型: 1. Key A  2. Key B (默认: 1): ";
            std::string typeInput;
            std::getline(std::cin, typeInput);

            uint8_t keyType = 0x60;  // 默认Key A
            if (typeInput == "2") {
                keyType = 0x61;  // Key B
            }

            std::string typeStr = (keyType == 0x60) ? "Key A" : "Key B";
            logger.Log("密钥类型: " + typeStr, 0);

            // 选择扇区
            std::cout << "应用于哪些扇区?" << std::endl;
            std::cout << "1. 所有扇区 (0-15)" << std::endl;
            std::cout << "2. 单个扇区" << std::endl;
            std::cout << "3. 扇区范围" << std::endl;
            std::cout << "请选择 (1-3): ";

            std::string sectorChoice;
            std::getline(std::cin, sectorChoice);

            if (sectorChoice == "1") {
                // 所有扇区
                for (int sector = 0; sector < 16; sector++) {
                    AddCustomKey(sector, key, keyType);
                }
                std::cout << "✅ 已为所有扇区添加密钥" << std::endl;
                logger.Log("已将自定义密钥应用到所有扇区", 0);
            }
            else if (sectorChoice == "2") {
                // 单个扇区
                std::cout << "输入扇区号 (0-15): ";
                int sector;
                std::cin >> sector;
                std::cin.ignore();

                if (sector >= 0 && sector < 16) {
                    AddCustomKey(sector, key, keyType);
                    std::cout << "✅ 已为扇区 " << sector << " 添加密钥" << std::endl;
                    logger.Log("已将自定义密钥应用到扇区 " + std::to_string(sector), 0);
                }
                else {
                    std::cout << "❌ 无效的扇区号!" << std::endl;
                    logger.Log("无效的扇区号: " + std::to_string(sector), 2);
                }
            }
            else if (sectorChoice == "3") {
                // 扇区范围
                std::cout << "输入起始扇区 (0-15): ";
                int startSector;
                std::cin >> startSector;
                std::cin.ignore();

                std::cout << "输入结束扇区 (" << startSector << "-15): ";
                int endSector;
                std::cin >> endSector;
                std::cin.ignore();

                if (startSector >= 0 && startSector < 16 &&
                    endSector >= startSector && endSector < 16) {
                    for (int sector = startSector; sector <= endSector; sector++) {
                        AddCustomKey(sector, key, keyType);
                    }
                    std::cout << "✅ 已为扇区 " << startSector << " 到 " << endSector << " 添加密钥" << std::endl;

                    std::stringstream rangeMsg;
                    rangeMsg << "已将自定义密钥应用到扇区 " << startSector << " 到 " << endSector;
                    logger.Log(rangeMsg.str(), 0);
                }
                else {
                    std::cout << "❌ 无效的扇区范围!" << std::endl;
                    logger.Log("无效的扇区范围: " + std::to_string(startSector) + " 到 " + std::to_string(endSector), 2);
                }
            }
            else {
                std::cout << "❌ 无效的选择，返回主菜单" << std::endl;
                logger.Log("无效的扇区选择选项", 2);
            }
        }
    }

    std::cout << "\n✅ 密钥设置完成!" << std::endl;
    logger.Log("密钥设置完成", 0);
}

void PN532::ReadCardAllDataWithMultipleKeys(const std::vector<unsigned char>& uid) {
    std::cout << "\n=== 读取CUID卡数据 (多密钥尝试) ===" << std::endl;
    std::cout << "卡UID: ";
    for (auto byte : uid) {
        printf("%02X ", byte);
    }
    std::cout << std::endl;

    // 记录开始读取
    std::stringstream startMsg;
    startMsg << "开始读取卡片数据 - UID: ";
    for (auto b : uid) {
        startMsg << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
    }
    logger.Log(startMsg.str(), 0);
    logger.LogToFile("读取操作开始", 0);

    int successfulSectors = 0;

    // 尝试读取所有扇区 (0-15)
    for (int sector = 0; sector < 16; sector++) {
        uint8_t keyType;
        std::vector<unsigned char> successfulKey;

        std::cout << "\n尝试扇区 " << sector << "..." << std::endl;

        if (TryAuthenticateSector(uid, sector, keyType, successfulKey)) {
            successfulSectors++;

            // 认证成功，读取扇区数据
            uint8_t sectorFirstBlock = sector * 4;
            std::vector<std::vector<unsigned char>> blocks;  // 这里声明 blocks 变量

            // 读取扇区的所有4个块
            for (int block = 0; block < 4; block++) {
                uint8_t blockNumber = sectorFirstBlock + block;
                std::vector<unsigned char> blockData;

                if (MifareReadBlock(blockNumber, blockData)) {
                    blocks.push_back(blockData);
                    std::cout << "块 " << (int)blockNumber << ": ";
                    for (auto byte : blockData) {
                        printf("%02X ", byte);
                    }
                    std::cout << std::endl;
                }
                else {
                    std::cout << "读取块 " << (int)blockNumber << " 失败!" << std::endl;
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // 这里可以安全使用 blocks，因为它在上面已经声明了
            if (blocks.size() == 4) {
                // 记录扇区数据到日志文件
                std::stringstream keyInfo;
                keyInfo << (keyType == 0x60 ? "Key A " : "Key B ");
                for (auto k : successfulKey) {
                    keyInfo << std::hex << std::setw(2) << std::setfill('0') << (int)k << " ";
                }

                logger.LogSectorData(sector, blocks,
                    (keyType == 0x60 ? "Key A" : "Key B"),
                    keyInfo.str());

                // 显示扇区数据
                std::cout << "扇区 " << sector << " 数据:" << std::endl;
                for (size_t i = 0; i < blocks.size(); i++) {
                    std::cout << "  块 " << (sector * 4 + i) << ": ";

                    // 如果是数据块，尝试解析为文本
                    if (i < 3) {  // 前3个块是数据块
                        // 显示十六进制
                        for (auto byte : blocks[i]) {
                            printf("%02X ", byte);
                        }

                        // 如果包含可打印字符，显示ASCII
                        bool hasPrintable = false;
                        for (auto byte : blocks[i]) {
                            if (byte >= 32 && byte <= 126) {
                                hasPrintable = true;
                                break;
                            }
                        }

                        if (hasPrintable) {
                            std::cout << "  ASCII: ";
                            for (auto byte : blocks[i]) {
                                if (byte >= 32 && byte <= 126) {
                                    std::cout << (char)byte;
                                }
                                else {
                                    std::cout << ".";
                                }
                            }
                        }
                    }
                    else {  // 第4个块是控制块
                        std::cout << "[控制块] ";
                        for (auto byte : blocks[i]) {
                            printf("%02X ", byte);
                        }

                        // 解析控制块
                        if (blocks[i].size() >= 16) {
                            std::cout << "\n        Key A: ";
                            for (int j = 0; j < 6; j++) printf("%02X ", blocks[i][j]);

                            std::cout << "  Access Bits: ";
                            for (int j = 6; j < 9; j++) printf("%02X ", blocks[i][j]);

                            std::cout << "  Key B: ";
                            for (int j = 10; j < 16; j++) printf("%02X ", blocks[i][j]);
                        }
                    }
                    std::cout << std::endl;
                }
            }
        }
        else {
            std::cout << "扇区 " << sector << " 认证失败（可能密钥不同）" << std::endl;
        }

        // 扇区间延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n读取完成! 成功读取 " << successfulSectors << "/16 个扇区" << std::endl;

    // 记录读取结果
    std::stringstream resultMsg;
    resultMsg << "读取完成 - 成功读取 " << successfulSectors << "/16 个扇区";
    logger.Log(resultMsg.str(), 0);
    logger.LogToFile("读取操作完成", 0);
}

void PN532::ReadCardDataInteractive(const std::vector<unsigned char>& uid) {
    std::cout << "\n=== 读取卡片数据 ===" << std::endl;
    std::cout << "卡UID: ";
    for (auto byte : uid) {
        printf("%02X ", byte);
    }
    std::cout << std::endl;

    // 记录开始交互式读取
    std::stringstream startMsg;
    startMsg << "开始交互式读取卡片 - UID: ";
    for (auto b : uid) {
        startMsg << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
    }
    logger.Log(startMsg.str(), 0);

    // 询问密钥选项
    std::cout << "\n请选择密钥选项:" << std::endl;
    std::cout << "1. 快速读取 (仅尝试默认密钥 FFFFFFFFFFFF)" << std::endl;
    std::cout << "2. 高级读取 (可配置多个密钥)" << std::endl;
    std::cout << "请选择 (1-2): ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1") {
        // 快速读取：使用默认密钥
        std::cout << "\n使用默认密钥进行快速读取..." << std::endl;
        logger.Log("用户选择快速读取模式", 0);

        ClearAllKeys();

        // 添加默认密钥到所有扇区
        for (int sector = 0; sector < 16; sector++) {
            AddDefaultKeyA(sector);
            AddDefaultKeyB(sector);
        }

        logger.Log("已为所有扇区设置默认密钥: FFFFFFFFFFFFF", 0);
        ReadCardAllDataWithMultipleKeys(uid);
    }
    else if (choice == "2") {
        // 高级读取：配置密钥
        std::cout << "\n使用高级读取模式..." << std::endl;
        logger.Log("用户选择高级读取模式", 0);

        SetupKeysFromUserInput();
        ReadCardAllDataWithMultipleKeys(uid);
    }
    else {
        std::cout << "无效选择，使用快速读取模式" << std::endl;
        logger.Log("用户输入无效，默认使用快速读取模式", 1);  // WARN级别

        // 默认使用快速读取
        ClearAllKeys();
        for (int sector = 0; sector < 16; sector++) {
            AddDefaultKeyA(sector);
            AddDefaultKeyB(sector);
        }

        logger.Log("已为所有扇区设置默认密钥: FFFFFFFFFFFFF", 0);
        ReadCardAllDataWithMultipleKeys(uid);
    }
}

// 在 PN532.cpp 中添加以下函数实现：

// 清空所有密钥
void PN532::ClearAllKeys() {
    sectorKeys.clear();
    useDefaultKeysOnly = true;
    std::cout << "已清空所有密钥配置" << std::endl;
}

// 添加默认Key A到指定扇区
void PN532::AddDefaultKeyA(uint8_t sector) {
    if (sector >= 16) {
        std::cerr << "无效的扇区号: " << (int)sector << std::endl;
        logger.Log("添加默认Key A失败: 无效扇区号 " + std::to_string(sector), 2);
        return;
    }

    std::vector<unsigned char> defaultKeyA = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    AddCustomKey(sector, defaultKeyA, 0x60);

    logger.Log("为扇区 " + std::to_string(sector) + " 添加默认Key A", 3);  // DEBUG级别
}

// 添加默认Key B到指定扇区
void PN532::AddDefaultKeyB(uint8_t sector) {
    if (sector >= 16) {
        std::cerr << "无效的扇区号: " << (int)sector << std::endl;
        logger.Log("添加默认Key B失败: 无效扇区号 " + std::to_string(sector), 2);
        return;
    }

    std::vector<unsigned char> defaultKeyB = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    AddCustomKey(sector, defaultKeyB, 0x61);

    logger.Log("为扇区 " + std::to_string(sector) + " 添加默认Key B", 3);  // DEBUG级别
}

// 添加自定义密钥到指定扇区
void PN532::AddCustomKey(uint8_t sector, const std::vector<unsigned char>& key, uint8_t keyType) {
    if (sector >= 16) {
        std::cerr << "无效的扇区号: " << (int)sector << std::endl;
        logger.Log("添加自定义密钥失败: 无效扇区号 " + std::to_string(sector), 2);
        return;
    }

    if (key.size() != 6) {
        std::cerr << "密钥必须是6字节! 实际长度: " << key.size() << std::endl;
        logger.Log("添加自定义密钥失败: 密钥长度错误", 2);
        return;
    }

    if (keyType != 0x60 && keyType != 0x61) {
        std::cerr << "无效的密钥类型: " << std::hex << (int)keyType << std::dec << std::endl;
        logger.Log("添加自定义密钥失败: 无效密钥类型", 2);
        return;
    }

    // 存储密钥：格式为 [类型(1字节), 密钥(6字节)]
    std::vector<unsigned char> keyEntry;
    keyEntry.push_back(keyType);
    for (int i = 0; i < 6; i++) {
        keyEntry.push_back(key[i]);
    }

    // 将密钥添加到该扇区的密钥列表中
    std::vector<unsigned char>& keys = sectorKeys[sector];
    keys.insert(keys.end(), keyEntry.begin(), keyEntry.end());

    // 记录密钥添加
    std::stringstream keyMsg;
    keyMsg << "为扇区 " << sector << " 添加密钥: ";
    keyMsg << (keyType == 0x60 ? "Key A " : "Key B ");
    for (auto k : key) {
        keyMsg << std::hex << std::setw(2) << std::setfill('0') << (int)k << " ";
    }
    logger.Log(keyMsg.str(), 3);  // DEBUG级别
}

// 设置特殊密钥配置
void PN532::SetupSpecialKeys() {
    ClearAllKeys();

    std::cout << "\n=== 配置特殊密钥 ===" << std::endl;
    logger.Log("开始配置特殊密钥", 0);

    std::cout << "扇区1和2 (索引1-2): Key A/B = 112233446655" << std::endl;
    std::cout << "其余扇区 (0,3-15): 使用默认密钥 FFFFFFFFFFFFF" << std::endl;

    // 特殊密钥：112233446655
    std::vector<unsigned char> specialKey = { 0x11, 0x22, 0x33, 0x44, 0x66, 0x55 };

    // 默认密钥：FFFFFFFFFFFF
    std::vector<unsigned char> defaultKey = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    // 设置所有扇区使用默认密钥
    for (int sector = 0; sector < 16; sector++) {
        // Key A
        AddCustomKey(sector, defaultKey, 0x60);
        // Key B
        AddCustomKey(sector, defaultKey, 0x61);
    }

    logger.Log("已为所有扇区设置默认密钥: FFFFFFFFFFFFF", 0);

    // 特殊处理扇区1和2（索引1和2）
    for (int sector = 1; sector <= 2; sector++) {
        // 移除默认密钥（如果需要的话）
        sectorKeys[sector].clear();

        // 添加特殊密钥 Key A
        AddCustomKey(sector, specialKey, 0x60);
        // 添加特殊密钥 Key B
        AddCustomKey(sector, specialKey, 0x61);

        std::cout << "扇区 " << sector << " (第" << (sector + 1) << "扇区): 特殊密钥已设置" << std::endl;

        std::stringstream sectorMsg;
        sectorMsg << "扇区 " << sector << " 使用特殊密钥: 11 22 33 44 66 55";
        logger.Log(sectorMsg.str(), 0);
    }

    std::cout << "✅ 特殊密钥配置完成!" << std::endl;
    std::cout << "  扇区1-2: 112233446655 (Key A & B)" << std::endl;
    std::cout << "  其他扇区: FFFFFFFFFFFFF (Key A & B)" << std::endl;

    logger.Log("特殊密钥配置完成: 扇区1-2使用112233446655, 其他扇区使用FFFFFFFFFFFF", 0);
}

// 使用特殊密钥读取卡片
void PN532::ReadCardWithSpecialKeys(const std::vector<unsigned char>& uid) {
    std::cout << "\n=== 使用特殊密钥读取卡片 ===" << std::endl;
    std::cout << "特殊密钥配置:" << std::endl;
    std::cout << "  扇区1和2: Key A/B = 112233446655" << std::endl;
    std::cout << "  其他扇区: 默认密钥 FFFFFFFFFFFFF" << std::endl;
    std::cout << "卡UID: ";
    for (auto byte : uid) {
        printf("%02X ", byte);
    }
    std::cout << std::endl;

    // 记录开始读取
    std::stringstream startMsg;
    startMsg << "开始特殊密钥读取卡片 - UID: ";
    for (auto b : uid) {
        startMsg << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
    }
    logger.Log(startMsg.str(), 0);
    logger.LogToFile("特殊密钥读取操作开始", 0);

    // 设置特殊密钥
    SetupSpecialKeys();

    // 记录特殊密钥配置
    logger.Log("特殊密钥配置完成: 扇区1-2使用112233446655, 其他扇区使用FFFFFFFFFFFF", 0);

    // 读取卡片数据
    int successfulSectors = 0;

    for (int sector = 0; sector < 16; sector++) {
        uint8_t keyType;
        std::vector<unsigned char> successfulKey;

        std::cout << "\n尝试读取扇区 " << sector;
        if (sector == 1 || sector == 2) {
            std::cout << " (使用特殊密钥 112233446655)" << std::endl;
            logger.Log("尝试扇区 " + std::to_string(sector) + " (使用特殊密钥 112233446655)", 3);
        }
        else {
            std::cout << " (使用默认密钥 FFFFFFFFFFFFF)" << std::endl;
            logger.Log("尝试扇区 " + std::to_string(sector) + " (使用默认密钥 FFFFFFFFFFFFF)", 3);
        }

        if (TryAuthenticateSector(uid, sector, keyType, successfulKey)) {
            successfulSectors++;

            // 认证成功，读取扇区数据
            uint8_t sectorFirstBlock = sector * 4;

            // 记录成功的认证
            std::stringstream authMsg;
            authMsg << "扇区 " << sector << " 认证成功 - 密钥: ";
            authMsg << (keyType == 0x60 ? "Key A " : "Key B ");
            for (auto k : successfulKey) {
                authMsg << std::hex << std::setw(2) << std::setfill('0') << (int)k << " ";
            }
            logger.Log(authMsg.str(), 0);

            // 读取当前扇区的4个块
            for (int block = 0; block < 4; block++) {
                uint8_t blockNumber = sectorFirstBlock + block;
                std::vector<unsigned char> blockData;

                if (MifareReadBlock(blockNumber, blockData)) {
                    // 显示块数据
                    std::cout << "  块" << block << ": ";
                    for (auto b : blockData) {
                        printf("%02X ", b);
                    }

                    // 记录块数据到日志文件
                    std::stringstream blockMsg;
                    blockMsg << "扇区 " << sector << " 块 " << block << ": ";
                    for (auto b : blockData) {
                        blockMsg << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
                    }

                    // 如果是数据块（块0-2）且包含可打印字符，显示ASCII
                    if (block < 3) {
                        bool hasPrintable = false;
                        for (auto b : blockData) {
                            if (b >= 32 && b <= 126) {
                                hasPrintable = true;
                                break;
                            }
                        }

                        if (hasPrintable) {
                            std::cout << " ASCII: \"";
                            for (auto b : blockData) {
                                if (b >= 32 && b <= 126) {
                                    std::cout << (char)b;
                                }
                                else if (b == 0) {
                                    std::cout << " ";  // 空格代替空字符
                                }
                                else {
                                    std::cout << ".";
                                }
                            }
                            std::cout << "\"";

                            // 记录ASCII到日志
                            blockMsg << " ASCII: \"";
                            for (auto b : blockData) {
                                if (b >= 32 && b <= 126) {
                                    blockMsg << (char)b;
                                }
                                else if (b == 0) {
                                    blockMsg << " ";
                                }
                                else {
                                    blockMsg << ".";
                                }
                            }
                            blockMsg << "\"";
                        }
                    }
                    else {
                        // 控制块（块3）- 显示密钥信息
                        std::cout << " [控制块]";
                        if (blockData.size() >= 16) {
                            std::cout << "\n        Key A: ";
                            for (int i = 0; i < 6; i++) printf("%02X ", blockData[i]);
                            std::cout << "\n        访问控制: ";
                            for (int i = 6; i < 9; i++) printf("%02X ", blockData[i]);
                            std::cout << "\n        Key B: ";
                            for (int i = 10; i < 16; i++) printf("%02X ", blockData[i]);

                            // 记录控制块信息到日志
                            blockMsg << " [控制块] KeyA: ";
                            for (int i = 0; i < 6; i++) {
                                blockMsg << std::hex << std::setw(2) << std::setfill('0') << (int)blockData[i] << " ";
                            }
                            blockMsg << " Access: ";
                            for (int i = 6; i < 9; i++) {
                                blockMsg << std::hex << std::setw(2) << std::setfill('0') << (int)blockData[i] << " ";
                            }
                            blockMsg << " KeyB: ";
                            for (int i = 10; i < 16; i++) {
                                blockMsg << std::hex << std::setw(2) << std::setfill('0') << (int)blockData[i] << " ";
                            }
                        }
                    }
                    std::cout << std::endl;

                    // 将块数据记录到文件
                    logger.LogToFile(blockMsg.str(), 0);
                }
                else {
                    std::cout << "  ❌ 读取块 " << block << " 失败" << std::endl;
                    logger.Log("读取扇区 " + std::to_string(sector) + " 块 " + std::to_string(block) + " 失败", 2);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        else {
            if (sector == 1 || sector == 2) {
                std::cout << "  ❌ 扇区 " << sector << " 特殊密钥认证失败" << std::endl;
                logger.Log("扇区 " + std::to_string(sector) + " 特殊密钥认证失败", 2);
            }
            else {
                std::cout << "  ❌ 扇区 " << sector << " 默认密钥认证失败" << std::endl;
                logger.Log("扇区 " + std::to_string(sector) + " 默认密钥认证失败", 2);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n=== 读取完成 ===" << std::endl;
    std::cout << "成功读取: " << successfulSectors << "/16 个扇区" << std::endl;
    std::cout << "按任意键继续..." << std::endl;

    // 记录读取结果
    std::stringstream resultMsg;
    resultMsg << "特殊密钥读取完成 - 成功读取 " << successfulSectors << "/16 个扇区";
    logger.Log(resultMsg.str(), 0);
    logger.LogToFile("特殊密钥读取操作完成", 0);
}

// =================================================================
// 卡片写入功能
// =================================================================

// 交互式写入卡片
void PN532::WriteCardInteractive(const std::vector<unsigned char>& uid) {
    std::cout << "\n=== 交互式卡片写入 ===" << std::endl;
    std::cout << "卡UID: ";
    for (auto byte : uid) {
        printf("%02X ", byte);
    }
    std::cout << std::endl;

    // 认证扇区
    int sector;
    std::cout << "\n输入要写入的扇区号 (0-15): ";
    std::cin >> sector;
    std::cin.ignore();

    if (sector < 0 || sector > 15) {
        std::cout << "无效的扇区号!" << std::endl;
        return;
    }

    uint8_t keyType;
    std::vector<unsigned char> successfulKey;

    if (!TryAuthenticateSector(uid, sector, keyType, successfulKey)) {
        std::cout << "扇区认证失败，无法写入!" << std::endl;
        return;
    }

    // 选择写入模式
    std::cout << "\n选择写入模式:" << std::endl;
    std::cout << "1. 写入文本数据" << std::endl;
    std::cout << "2. 写入十六进制数据" << std::endl;
    std::cout << "3. 修改密钥" << std::endl;
    std::cout << "4. 清空扇区" << std::endl;
    std::cout << "请选择 (1-4): ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1") {
        WriteTextToCard(uid);
    }
    else if (choice == "2") {
        int blockInput;
        uint8_t controlBlock = sector * 4 + 3;
        std::cout << "输入块号 (" << (sector * 4) << "-" << (sector * 4 + 2)
            << "，块" << (int)controlBlock << "为控制块): ";
        std::cin >> blockInput;
        std::cin.ignore();

        // 验证输入是否在范围内
        if (blockInput < static_cast<int>(sector * 4) ||
            blockInput > static_cast<int>(sector * 4 + 2)) {
            std::cout << "无效的块号!" << std::endl;
            std::cout << "扇区 " << sector << " 的有效数据块是: "
                << (sector * 4) << ", " << (sector * 4 + 1) << ", "
                << (sector * 4 + 2) << std::endl;
            return;
        }

        uint8_t block = static_cast<uint8_t>(blockInput);

        // 输入十六进制数据
        std::cout << "输入16字节十六进制数据 (空格分隔，例如: 01 02 03 ...): ";
        std::string hexInput;
        std::getline(std::cin, hexInput);

        // 解析十六进制数据
        std::vector<unsigned char> data;
        std::stringstream ss(hexInput);
        std::string byteStr;

        while (ss >> byteStr) {
            if (data.size() >= 16) break;
            unsigned char byte = (unsigned char)std::strtoul(byteStr.c_str(), nullptr, 16);
            data.push_back(byte);
        }

        // 填充到16字节
        while (data.size() < 16) {
            data.push_back(0x00);
        }

        // 写入数据
        if (MifareWriteBlock(block, data)) {
            std::cout << "✅ 数据写入成功!" << std::endl;

            // 验证写入
            std::vector<unsigned char> verifyData;
            if (MifareReadBlock(block, verifyData)) {
                std::cout << "验证读取: ";
                for (auto b : verifyData) {
                    printf("%02X ", b);
                }
                std::cout << std::endl;
            }
        }
    }
    else if (choice == "3") {
        std::cout << "\n修改扇区 " << sector << " 密钥" << std::endl;

        // 读取当前控制块
        uint8_t controlBlock = sector * 4 + 3;
        std::vector<unsigned char> currentData;
        if (!MifareReadBlock(controlBlock, currentData)) {
            std::cout << "无法读取当前控制块!" << std::endl;
            return;
        }

        std::cout << "当前密钥A: ";
        for (int i = 0; i < 6; i++) printf("%02X ", currentData[i]);
        std::cout << std::endl;

        std::cout << "当前访问位: ";
        for (int i = 6; i < 9; i++) printf("%02X ", currentData[i]);
        std::cout << std::endl;

        std::cout << "当前密钥B: ";
        for (int i = 10; i < 16; i++) printf("%02X ", currentData[i]);
        std::cout << std::endl;

        // 输入新密钥
        std::cout << "\n输入新Key A (6字节十六进制，例如: FF FF FF FF FF FF): ";
        std::string keyAStr;
        std::getline(std::cin, keyAStr);

        std::vector<unsigned char> keyA;
        std::stringstream ssa(keyAStr);
        std::string byteStr;

        while (ssa >> byteStr && keyA.size() < 6) {
            unsigned char byte = (unsigned char)std::strtoul(byteStr.c_str(), nullptr, 16);
            keyA.push_back(byte);
        }

        if (keyA.size() != 6) {
            std::cout << "Key A必须是6字节!" << std::endl;
            return;
        }

        std::cout << "输入新Key B (6字节十六进制，或输入 'same' 使用Key A): ";
        std::string keyBStr;
        std::getline(std::cin, keyBStr);

        std::vector<unsigned char> keyB;
        if (keyBStr == "same" || keyBStr == "SAME") {
            keyB = keyA;
        }
        else {
            std::stringstream ssb(keyBStr);
            while (ssb >> byteStr && keyB.size() < 6) {
                unsigned char byte = (unsigned char)std::strtoul(byteStr.c_str(), nullptr, 16);
                keyB.push_back(byte);
            }

            if (keyB.size() != 6) {
                std::cout << "Key B必须是6字节!" << std::endl;
                return;
            }
        }

        // 保持原有访问控制位
        std::vector<unsigned char> accessBits = { currentData[6], currentData[7], currentData[8], 0x00 };

        if (ChangeSectorKeys(sector, keyA, keyB, accessBits)) {
            std::cout << "✅ 密钥修改成功!" << std::endl;
            std::cout << "新密钥已生效，请记住新密钥!" << std::endl;
        }
    }
    else if (choice == "4") {
        std::cout << "清空扇区 " << sector << " (写入全0)" << std::endl;
        std::cout << "按 Y 确认，其他键取消: ";

        char ch = _getch();
        if (ch != 'Y' && ch != 'y') {
            std::cout << "操作取消" << std::endl;
            return;
        }

        uint8_t startBlock = sector * 4;
        std::vector<unsigned char> zeroData(16, 0x00);

        bool success = true;
        for (int i = 0; i < 4; i++) {
            uint8_t block = startBlock + i;

            // 跳过厂商块和控制块
            if (block == 0 || i == 3) {
                continue;
            }

            if (!MifareWriteBlock(block, zeroData)) {
                success = false;
                break;
            }
        }

        if (success) {
            std::cout << "✅ 扇区清空成功!" << std::endl;
        }
        else {
            std::cout << "❌ 扇区清空失败!" << std::endl;
        }
    }
}

// 写入文本到卡片
void PN532::WriteTextToCard(const std::vector<unsigned char>& uid) {
    std::cout << "\n=== 写入文本数据 ===" << std::endl;

    int sector;
    std::cout << "输入扇区号 (0-15): ";
    std::cin >> sector;
    std::cin.ignore();

    if (sector < 0 || sector > 15) {
        std::cout << "无效的扇区号!" << std::endl;
        return;
    }

    int blockInput;
    uint8_t controlBlock = sector * 4 + 3;  // 计算控制块号
    std::cout << "输入块号 (" << (sector * 4) << "-" << (sector * 4 + 2)
        << "，块" << (int)controlBlock << "为控制块): ";
    std::cin >> blockInput;
    std::cin.ignore();

    if (blockInput < static_cast<int>(sector * 4) ||
        blockInput > static_cast<int>(sector * 4 + 2)) {
        std::cout << "无效的块号! 只能写入数据块(0-2)!" << std::endl;
        std::cout << "扇区 " << sector << " 的有效数据块是: "
            << (sector * 4) << ", " << (sector * 4 + 1) << ", "
            << (sector * 4 + 2) << std::endl;
        return;
    }

    uint8_t block = static_cast<uint8_t>(blockInput);

    std::cout << "输入要写入的文本 (最多16个字符): ";
    std::string text;
    std::getline(std::cin, text);

    // 将文本转换为16字节数据
    std::vector<unsigned char> data(16, 0x00);
    for (size_t i = 0; i < text.length() && i < 16; i++) {
        data[i] = text[i];
    }

    // 添加结束符
    if (text.length() < 16) {
        data[text.length()] = 0x00;
    }

    std::cout << "准备写入的数据: ";
    for (auto b : data) {
        printf("%02X ", b);
    }
    std::cout << std::endl;

    std::cout << "按 Y 确认写入，其他键取消: ";
    char ch = _getch();
    if (ch != 'Y' && ch != 'y') {
        std::cout << "操作取消" << std::endl;
        return;
    }

    if (MifareWriteBlock(block, data)) {
        std::cout << "✅ 文本写入成功!" << std::endl;

        // 验证写入
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::vector<unsigned char> verifyData;
        if (MifareReadBlock(block, verifyData)) {
            std::cout << "验证读取: ";
            for (auto b : verifyData) {
                if (b >= 32 && b <= 126) {
                    std::cout << (char)b;
                }
                else if (b == 0) {
                    std::cout << " ";
                }
                else {
                    std::cout << ".";
                }
            }
            std::cout << std::endl;
        }
    }
}

// 添加备份功能到 PN532 类
void PN532::BackupCardData(const std::vector<unsigned char>& uid) {
    std::cout << "\n=== 卡片数据备份 ===" << std::endl;
    std::cout << "正在备份所有扇区数据..." << std::endl;

    // 生成备份文件名
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
    localtime_s(&now_tm, &now_time_t);

    char filename[100];
    strftime(filename, sizeof(filename), "backup_%Y%m%d_%H%M%S.txt", &now_tm);

    std::ofstream backupFile(filename);
    if (!backupFile.is_open()) {
        std::cout << "无法创建备份文件!" << std::endl;
        return;
    }

    // 写入备份头信息
    backupFile << "PN532 NFC卡片备份" << std::endl;
    backupFile << "时间: " << std::ctime(&now_time_t);
    backupFile << "UID: ";
    for (auto b : uid) {
        backupFile << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
    }
    backupFile << std::endl << std::endl;

    // 备份所有扇区
    for (int sector = 0; sector < 16; sector++) {
        uint8_t keyType;
        std::vector<unsigned char> successfulKey;

        if (TryAuthenticateSector(uid, sector, keyType, successfulKey)) {
            backupFile << "扇区 " << sector << " (认证成功)" << std::endl;

            uint8_t sectorFirstBlock = sector * 4;
            for (int block = 0; block < 4; block++) {
                uint8_t blockNumber = sectorFirstBlock + block;
                std::vector<unsigned char> blockData;

                if (MifareReadBlock(blockNumber, blockData)) {
                    backupFile << "  块 " << block << " (" << (int)blockNumber << "): ";
                    for (auto b : blockData) {
                        backupFile << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
                    }

                    // 如果是数据块且包含可打印字符，添加ASCII表示
                    if (block < 3) {
                        bool hasPrintable = false;
                        for (auto b : blockData) {
                            if (b >= 32 && b <= 126) {
                                hasPrintable = true;
                                break;
                            }
                        }

                        if (hasPrintable) {
                            backupFile << "  ASCII: ";
                            for (auto b : blockData) {
                                if (b >= 32 && b <= 126) {
                                    backupFile << (char)b;
                                }
                                else {
                                    backupFile << ".";
                                }
                            }
                        }
                    }
                    backupFile << std::endl;
                }
            }
            backupFile << std::endl;
        }
        else {
            backupFile << "扇区 " << sector << " (认证失败)" << std::endl << std::endl;
        }
    }

    backupFile.close();
    std::cout << "✅ 备份完成! 文件: " << filename << std::endl;
}

// 特殊写入模式
void PN532::SpecialWriteMode() {
    std::cout << "\n=== 特殊写入模式 ===" << std::endl;
    std::cout << "此模式使用特殊密钥配置:" << std::endl;
    std::cout << "  扇区1和2: Key A/B = 112233446655" << std::endl;
    std::cout << "  其他扇区: 默认密钥 FFFFFFFFFFFFF" << std::endl;
    std::cout << "将向扇区1的块5和块6写入特定格式的数据" << std::endl;
    std::cout << "\n请输入一个0~50000之间的整数: ";

    int number;
    std::cin >> number;
    std::cin.ignore();

    if (number < 0 || number > 50000) {
        std::cout << "❌ 输入的数字必须在0~50000之间!" << std::endl;
        return;
    }

    // 转换为8位十六进制字符串
    std::stringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0') << number;
    std::string hexStr = ss.str();  // 8位十六进制字符串，例如：00000002

    std::cout << "输入的整数: " << number << std::endl;
    std::cout << "转换为8位十六进制: " << hexStr << std::endl;

    // 分组：每2位一组，共4组
    std::vector<std::string> groups;
    for (int i = 0; i < 8; i += 2) {
        groups.push_back(hexStr.substr(i, 2));
    }

    std::cout << "分组结果: ";
    for (const auto& group : groups) {
        std::cout << group << " ";
    }
    std::cout << std::endl;

    // 倒序排列组，但组内顺序不变
    std::reverse(groups.begin(), groups.end());

    std::cout << "倒序后: ";
    for (const auto& group : groups) {
        std::cout << group << " ";
    }
    std::cout << std::endl;

    // 计算校验码：原数组 + 校验码 = FF
    std::vector<std::string> checksums;
    for (const auto& group : groups) {
        unsigned int value;
        std::stringstream groupStream;
        groupStream << std::hex << group;
        groupStream >> value;

        unsigned char checksum = 0xFF - static_cast<unsigned char>(value);
        std::stringstream checksumStream;
        checksumStream << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(checksum);
        checksums.push_back(checksumStream.str());
    }

    std::cout << "校验码: ";
    for (const auto& checksum : checksums) {
        std::cout << checksum << " ";
    }
    std::cout << std::endl;

    // 按照原数据-校验码-原数据进行编组
    std::vector<std::string> combined;

    // 添加原数据（倒序后）
    for (const auto& group : groups) {
        combined.push_back(group);
    }

    // 添加校验码
    for (const auto& checksum : checksums) {
        combined.push_back(checksum);
    }

    // 再次添加原数据（倒序后）
    for (const auto& group : groups) {
        combined.push_back(group);
    }

    std::cout << "编组结果: ";
    for (const auto& item : combined) {
        std::cout << item << " ";
    }
    std::cout << std::endl;

    // 转换为字节序列
    std::vector<unsigned char> baseData;
    for (const auto& item : combined) {
        unsigned char byte = static_cast<unsigned char>(
            std::strtoul(item.c_str(), nullptr, 16));
        baseData.push_back(byte);
    }

    // 创建第一组数据：baseData + 01 FE 01 FE
    std::vector<unsigned char> data1 = baseData;
    data1.push_back(0x01);
    data1.push_back(0xFE);
    data1.push_back(0x01);
    data1.push_back(0xFE);

    // 创建第二组数据：baseData + 02 FD 02 FD
    std::vector<unsigned char> data2 = baseData;
    data2.push_back(0x02);
    data2.push_back(0xFD);
    data2.push_back(0x02);
    data2.push_back(0xFD);

    // 检测卡片
    std::cout << "\n请放置卡片..." << std::endl;

    std::vector<unsigned char> uid;
    while (!DetectNFC(uid)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "✅ 检测到卡片!" << std::endl;
    std::cout << "卡UID: ";
    for (auto b : uid) {
        printf("%02X ", b);
    }
    std::cout << std::endl;

    // 设置特殊密钥
    SetupSpecialKeys();

    // 认证扇区1
    uint8_t keyType;
    std::vector<unsigned char> successfulKey;

    std::cout << "\n正在认证扇区1..." << std::endl;
    if (!TryAuthenticateSector(uid, 1, keyType, successfulKey)) {
        std::cout << "❌ 扇区1认证失败!" << std::endl;
        return;
    }

    std::cout << "✅ 扇区1认证成功!" << std::endl;

    // 写入块5
    std::cout << "\n正在写入扇区1块5..." << std::endl;
    std::cout << "数据: ";
    for (auto b : data1) {
        printf("%02X ", b);
    }
    std::cout << std::endl;

    if (MifareWriteBlock(5, data1)) {
        std::cout << "✅ 块5写入成功!" << std::endl;
    }
    else {
        std::cout << "❌ 块5写入失败!" << std::endl;
        return;
    }

    // 等待一下
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 写入块6
    std::cout << "\n正在写入扇区1块6..." << std::endl;
    std::cout << "数据: ";
    for (auto b : data2) {
        printf("%02X ", b);
    }
    std::cout << std::endl;

    if (MifareWriteBlock(6, data2)) {
        std::cout << "✅ 块6写入成功!" << std::endl;
    }
    else {
        std::cout << "❌ 块6写入失败!" << std::endl;
        return;
    }

    std::cout << "\n🎉 特殊写入模式完成!" << std::endl;
    std::cout << "按任意键继续..." << std::endl;
}

// 记录卡片信息
void PN532::LogCardInfo(const std::vector<unsigned char>& uid, const std::string& operation) {
    logger.LogCardInfo(uid, operation);
}

void PN532::Close() {
    serial.Close();
}