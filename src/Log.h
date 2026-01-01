#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <mutex>
#include <sstream>

class Logger {
private:
    std::ofstream logFile;
    std::string logFileName;
    bool enableLogging;
    std::mutex logMutex;

    // 获取当前时间字符串
    std::string GetCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm now_tm;
        localtime_s(&now_tm, &now_time_t);

        std::stringstream ss;
        ss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
        return ss.str();
    }

    // 格式化日志级别
    std::string FormatLevel(int level) {
        switch (level) {
        case 0: return "[INFO] ";
        case 1: return "[WARN] ";
        case 2: return "[ERROR] ";
        case 3: return "[DEBUG] ";
        default: return "[LOG] ";
        }
    }

public:
    Logger() : enableLogging(false) {}

    ~Logger() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }

    // 初始化日志系统
    bool Initialize(const std::string& fileName = "", bool enable = true) {
        enableLogging = enable;

        if (!enableLogging) {
            return true;
        }

        if (fileName.empty()) {
            // 使用默认文件名：年-月-日_时-分-秒.log
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm now_tm;
            localtime_s(&now_tm, &now_time_t);

            char buffer[100];
            strftime(buffer, sizeof(buffer), "nfc_%Y%m%d_%H%M%S.log", &now_tm);
            logFileName = buffer;
        }
        else {
            logFileName = fileName;
        }

        logFile.open(logFileName, std::ios::out | std::ios::app);
        if (!logFile.is_open()) {
            std::cerr << "无法创建日志文件: " << logFileName << std::endl;
            enableLogging = false;
            return false;
        }

        // 写入日志头
        std::lock_guard<std::mutex> lock(logMutex);
        logFile << "==========================================" << std::endl;
        logFile << "NFC读写器日志 - 开始时间: " << GetCurrentTime() << std::endl;
        logFile << "==========================================" << std::endl;
        logFile.flush();

        std::cout << "✅ 日志系统已启动，文件: " << logFileName << std::endl;
        return true;
    }

    // 写入日志
    void Log(const std::string& message, int level = 0, bool toConsole = true) {
        if (!enableLogging && !toConsole) {
            return;
        }

        std::string formattedMessage = FormatLevel(level) + GetCurrentTime() + " - " + message;

        std::lock_guard<std::mutex> lock(logMutex);

        // 输出到控制台
        if (toConsole) {
            std::cout << formattedMessage << std::endl;
        }

        // 输出到文件
        if (enableLogging && logFile.is_open()) {
            logFile << formattedMessage << std::endl;
            logFile.flush();
        }
    }

    // 仅写入文件（不显示到控制台）
    void LogToFile(const std::string& message, int level = 0) {
        Log(message, level, false);
    }

    // 记录卡片信息
    void LogCardInfo(const std::vector<unsigned char>& uid, const std::string& operation) {
        if (!enableLogging) return;

        std::stringstream ss;
        ss << "卡片操作: " << operation << " - UID: ";
        for (auto b : uid) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
        }

        std::lock_guard<std::mutex> lock(logMutex);
        logFile << GetCurrentTime() << " - " << ss.str() << std::endl;
        logFile.flush();
    }

    // 记录扇区数据
    void LogSectorData(int sector, const std::vector<std::vector<unsigned char>>& blocks,
        const std::string& keyType, const std::string& key) {
        if (!enableLogging) return;

        std::lock_guard<std::mutex> lock(logMutex);

        logFile << "扇区 " << sector << " 数据 (密钥: " << keyType << " " << key << ")" << std::endl;

        for (size_t i = 0; i < blocks.size(); i++) {
            logFile << "  块 " << i << ": ";
            for (auto byte : blocks[i]) {
                logFile << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
            }

            // 如果是数据块，添加ASCII表示
            if (i < 3 && blocks[i].size() == 16) {
                logFile << "  ASCII: ";
                for (auto byte : blocks[i]) {
                    if (byte >= 32 && byte <= 126) {
                        logFile << (char)byte;
                    }
                    else {
                        logFile << ".";
                    }
                }
            }

            logFile << std::endl;
        }
        logFile.flush();
    }

    // 设置日志状态
    void SetLogging(bool enable) {
        enableLogging = enable;
        if (enable && !logFile.is_open()) {
            Initialize("", true);
        }
    }

    // 获取日志状态
    bool IsLoggingEnabled() const {
        return enableLogging;
    }

    // 获取日志文件名
    std::string GetLogFileName() const {
        return logFileName;
    }

    // 关闭日志
    void Close() {
        if (logFile.is_open()) {
            std::lock_guard<std::mutex> lock(logMutex);
            logFile << "==========================================" << std::endl;
            logFile << "NFC读写器日志 - 结束时间: " << GetCurrentTime() << std::endl;
            logFile << "==========================================" << std::endl;
            logFile.close();
        }
        enableLogging = false;
    }
};