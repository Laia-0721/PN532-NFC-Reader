#pragma once
#include "SerialPort.h"
#include "Log.h"
#include <vector>
#include <string>
#include <map>

class PN532 {
private:
    SerialPort serial;
    std::string comPort;
    DWORD baudRate;

    // 日志对象
    Logger logger;

    // 检测可用串口
    std::vector<std::string> DetectAvailablePorts();

    // 检查指定串口是否可用
    bool TestSerialPort(const char* portName);

    // PN532常量定义 - 使用C++17 constexpr在类内初始化
    static constexpr unsigned char PREAMBLE = 0x00;
    static constexpr unsigned char STARTCODE1 = 0x00;
    static constexpr unsigned char STARTCODE2 = 0xFF;
    static constexpr unsigned char POSTAMBLE = 0x00;

    static constexpr unsigned char HOSTTOPN532 = 0xD4;
    static constexpr unsigned char PN532TOHOST = 0xD5;

    // 命令定义
    static constexpr unsigned char CMD_GETFIRMWAREVERSION = 0x02;
    static constexpr unsigned char CMD_SAMCONFIGURATION = 0x14;
    static constexpr unsigned char CMD_INLISTPASSIVETARGET = 0x4A;
    static constexpr unsigned char CMD_INDATAEXCHANGE = 0x40;
    static constexpr unsigned char CMD_MIFARE_READ = 0x30;
    static constexpr unsigned char CMD_MIFARE_WRITE = 0xA0;
    static constexpr unsigned char CMD_MIFARE_WRITE_VALUE = 0xA0;
    static constexpr unsigned char CMD_AUTHENTICATE_A = 0x60;
    static constexpr unsigned char CMD_AUTHENTICATE_B = 0x61;

    // 默认密钥 - 使用静态constexpr数组
    static constexpr unsigned char DEFAULT_KEY_A[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    static constexpr unsigned char DEFAULT_KEY_B[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    // 密钥管理
    std::map<int, std::vector<unsigned char>> sectorKeys;  // 扇区号 -> 密钥列表
    bool useDefaultKeysOnly;

    // 密钥尝试函数
    bool TryAuthenticateSector(const std::vector<unsigned char>& uid,
        uint8_t sector,
        uint8_t& successfulKeyType,
        std::vector<unsigned char>& successfulKey);

    // 校验和计算
    unsigned char CalculateChecksum(const std::vector<unsigned char>& data);

    // 构建PN532帧
    std::vector<unsigned char> BuildFrame(const std::vector<unsigned char>& data);

    // 解析PN532帧
    bool ParseFrame(const std::vector<unsigned char>& response,
        std::vector<unsigned char>& data);

public:
    PN532();
    ~PN532();

    // 关闭连接
    void Close();

    // 特殊写入模式
    void SpecialWriteMode();

    // 基本功能
    bool Initialize(const char* port = "", DWORD baud = CBR_115200);
    bool GetFirmwareVersion(std::vector<unsigned char>& version);
    bool SAMConfiguration();
    bool DetectNFC(std::vector<unsigned char>& uid);
  
    // 读取功能
    bool MifareAuthenticate(const std::vector<unsigned char>& uid,
        uint8_t blockNumber,
        uint8_t keyType = 0x60,
        const unsigned char* key = nullptr);
    bool MifareReadBlock(uint8_t blockNumber, std::vector<unsigned char>& data);
    bool MifareReadSector(uint8_t sector, std::vector<std::vector<unsigned char>>& blocks);
    void ReadCardAllData(const std::vector<unsigned char>& uid);
    void ReadCardAllDataWithMultipleKeys(const std::vector<unsigned char>& uid);
    void ReadCardDataInteractive(const std::vector<unsigned char>& uid);
    void ReadCardWithSpecialKeys(const std::vector<unsigned char>& uid);

    // 写入功能
    bool MifareWriteBlock(uint8_t blockNumber, const std::vector<unsigned char>& data);
    bool MifareWriteValueBlock(uint8_t blockNumber, int32_t value);
    bool MifareWriteSector(uint8_t sector, const std::vector<std::vector<unsigned char>>& blocks);
    bool ChangeSectorKeys(uint8_t sector,
        const std::vector<unsigned char>& keyA,
        const std::vector<unsigned char>& keyB,
        const std::vector<unsigned char>& accessBits);
    void WriteCardInteractive(const std::vector<unsigned char>& uid);
    void WriteTextToCard(const std::vector<unsigned char>& uid);

    // 备份功能
    void BackupCardData(const std::vector<unsigned char>& uid);

    // 密钥管理
    void ClearAllKeys();
    void AddDefaultKeyA(uint8_t sector);
    void AddDefaultKeyB(uint8_t sector);
    void AddCustomKey(uint8_t sector, const std::vector<unsigned char>& key, uint8_t keyType);
    void SetupKeysFromUserInput();
    void SetupSpecialKeys();

    // 访问控制位工具
    std::vector<unsigned char> CalculateAccessBits(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);
    void DisplayAccessBits(uint8_t sector);

    // 日志管理
    void EnableLogging(bool enable = true);
    bool IsLoggingEnabled() const;
    std::string GetLogFileName() const;
    void LogCardInfo(const std::vector<unsigned char>& uid, const std::string& operation);
    void LogToFile(const std::string& message, int level = 0);
};