#include "Common/logger.hh"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <mutex>

namespace dmu {

// 静态成员定义
std::mutex Logger::globalMutex;
std::unique_ptr<Logger> Logger::defaultInstance = nullptr;
std::map<std::string, std::unique_ptr<Logger>> Logger::namedInstances;
bool Logger::globalEnabled = true; // 默认启用全局日志

Logger::Logger(const std::string& name) : instanceName_(name) {}

Logger::~Logger() {
    // 在析构时关闭所有日志文件
    std::lock_guard<std::mutex> lock(logMutex);
    for (auto& pair : loggers) {
        if (pair.second->logFile.is_open()) {
            pair.second->logFile << "[INFO] Logger closed" << std::endl;
            pair.second->logFile.close();
        }
    }
}

Logger& Logger::getInstance() {
    std::lock_guard<std::mutex> lock(globalMutex);
    if (!defaultInstance) {
        defaultInstance = std::unique_ptr<Logger>(new Logger("default"));
    }
    return *defaultInstance;
}

Logger& Logger::getInstance(const std::string& name) {
    std::lock_guard<std::mutex> lock(globalMutex);
    if (namedInstances.find(name) == namedInstances.end()) {
        namedInstances[name] = std::unique_ptr<Logger>(new Logger(name));
    }
    return *namedInstances[name];
}

bool Logger::initialize(const std::string& path, const std::string& filename) {
    return initialize(instanceName_, path, filename);
}

bool Logger::initialize(const std::string& name, const std::string& path, const std::string& filename) {
    std::lock_guard<std::mutex> lock(logMutex);

    // 确保logger数据存在
    if (loggers.find(name) == loggers.end()) {
        loggers[name] = std::unique_ptr<LoggerData>(new LoggerData());
    }

    auto& loggerData = loggers[name];

    // 如果已经初始化，先关闭旧文件
    if (loggerData->initialized && loggerData->logFile.is_open()) {
        loggerData->logFile.close();
    }

    // 构建完整路径
    std::filesystem::path fullPath = std::filesystem::path(path) / (filename + ".log");
    loggerData->fullLogPath = fullPath.string();

    // 创建目录（如果不存在）
    std::filesystem::create_directories(path);

    // 打开日志文件（覆盖模式，清除之前的内容）
    loggerData->logFile.open(loggerData->fullLogPath, std::ios::out | std::ios::trunc);
    if (!loggerData->logFile.is_open()) {
        std::cerr << "Failed to open log file: " << loggerData->fullLogPath << std::endl;
        loggerData->initialized = false;
        return false;
    }

    loggerData->initialized = true;

    // 写入初始化信息（不包含时间）
    loggerData->logFile << "[INFO] Logger '" << name << "' initialized"
                        << std::endl;
    loggerData->logFile.flush();

    return true;
}

void Logger::write(LogLevel level, const std::string& message) {
    write(instanceName_, level, message);
}

void Logger::write(const std::string& name, LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);

    // 检查全局开关
    if (!globalEnabled) {
        return;
    }

    // 检查logger是否存在且已初始化
    if (loggers.find(name) == loggers.end() || !loggers[name]->initialized) {
        std::cerr << "Logger '" << name << "' not initialized!" << std::endl;
        std::abort();
        return;
    }

    auto& loggerData = loggers[name];

    // 检查logger是否启用
    if (!loggerData->enabled) {
        return;
    }

    // 检查日志级别
    if (level < loggerData->minLevel) {
        return; // 低于最小级别的日志不输出
    }

    if (!loggerData->logFile.is_open()) {
        std::cerr << "Log file for logger '" << name << "' is not open!" << std::endl;
        return;
    }

    loggerData->logFile << "[" << std::left << std::setw(5) << levelToString(level) << "] "
                        << "[" << std::left << std::setw(16) << sc_core::sc_time_stamp().value() << " ps] "
                        << message << std::endl;
    loggerData->logFile.flush();
}

void Logger::write(const std::string& level, const std::string& message) {
    LogLevel logLevel = LogLevel::INFO; // 默认级别

    if (level == "TRACE") logLevel = LogLevel::TRACE;
    else if (level == "DEBUG") logLevel = LogLevel::DBG;
    else if (level == "INFO") logLevel = LogLevel::INFO;
    else if (level == "WARN") logLevel = LogLevel::WARN;
    else if (level == "ERROR") logLevel = LogLevel::ERROR;
    else if (level == "FATAL") logLevel = LogLevel::FATAL;

    write(instanceName_, logLevel, message);
}

void Logger::write(const std::string& name, const std::string& level, const std::string& message) {
    LogLevel logLevel = LogLevel::INFO; // 默认级别

    if (level == "TRACE") logLevel = LogLevel::TRACE;
    else if (level == "DEBUG") logLevel = LogLevel::DBG;
    else if (level == "INFO") logLevel = LogLevel::INFO;
    else if (level == "WARN") logLevel = LogLevel::WARN;
    else if (level == "ERROR") logLevel = LogLevel::ERROR;
    else if (level == "FATAL") logLevel = LogLevel::FATAL;

    write(name, logLevel, message);
}

void Logger::close() {
    close(instanceName_);
}

void Logger::close(const std::string& name) {
    std::lock_guard<std::mutex> lock(logMutex);

    if (loggers.find(name) != loggers.end() && loggers[name]->initialized) {
        if (loggers[name]->logFile.is_open()) {
            loggers[name]->logFile << "[INFO] Logger '" << name << "' closed"
                                   << std::endl;
            loggers[name]->logFile.close();
        }
        loggers[name]->initialized = false;
    }
}

bool Logger::isInitialized() const {
    return isInitialized(instanceName_);
}

bool Logger::isInitialized(const std::string& name) const {
    std::lock_guard<std::mutex> lock(logMutex);
    auto it = loggers.find(name);
    return (it != loggers.end()) && it->second->initialized;
}

void Logger::setLogLevel(LogLevel level) {
    setLogLevel(instanceName_, level);
}

void Logger::setLogLevel(const std::string& name, LogLevel level) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (loggers.find(name) == loggers.end()) {
        loggers[name] = std::unique_ptr<LoggerData>(new LoggerData());
    }
    loggers[name]->minLevel = level;
}

LogLevel Logger::getLogLevel() const {
    return getLogLevel(instanceName_);
}

LogLevel Logger::getLogLevel(const std::string& name) const {
    std::lock_guard<std::mutex> lock(logMutex);
    auto it = loggers.find(name);
    if (it != loggers.end()) {
        return it->second->minLevel;
    }
    return LogLevel::TRACE; // 默认返回最低级别
}

void Logger::setGlobalEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(globalMutex);
    globalEnabled = enabled;
}

bool Logger::isGlobalEnabled() {
    std::lock_guard<std::mutex> lock(globalMutex);
    return globalEnabled;
}

void Logger::setEnabled(bool enabled) {
    setEnabled(instanceName_, enabled);
}

void Logger::setEnabled(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (loggers.find(name) == loggers.end()) {
        loggers[name] = std::unique_ptr<LoggerData>(new LoggerData());
    }
    loggers[name]->enabled = enabled;
}

bool Logger::isEnabled() const {
    return isEnabled(instanceName_);
}

bool Logger::isEnabled(const std::string& name) const {
    std::lock_guard<std::mutex> lock(logMutex);
    auto it = loggers.find(name);
    return (it != loggers.end()) && it->second->enabled;
}

void Logger::closeAll() {
    std::lock_guard<std::mutex> lock(globalMutex);

    // 关闭默认实例中的所有Logger
    if (defaultInstance) {
        std::lock_guard<std::mutex> logLock(defaultInstance->logMutex);
        for (auto& pair : defaultInstance->loggers) {
            if (pair.second->logFile.is_open()) {
                pair.second->logFile << "[INFO] Logger '" << pair.first << "' closed"
                                     << std::endl;
                pair.second->logFile.close();
            }
            pair.second->initialized = false;
        }
    }

    // 关闭所有命名实例中的logger
    for (auto& instancePair : namedInstances) {
        if (instancePair.second) {
            std::lock_guard<std::mutex> logLock(instancePair.second->logMutex);
            for (auto& pair : instancePair.second->loggers) {
                if (pair.second->logFile.is_open()) {
                    pair.second->logFile << "[INFO] Logger '" << pair.first << "' closed"
                                         << std::endl;
                    pair.second->logFile.close();
                }
                pair.second->initialized = false;
            }
        }
    }
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DBG:   return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimeString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;

    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));

    char result[128];
    std::snprintf(result, sizeof(result), "%s.%03d", buffer, static_cast<int>(ms.count()));

    return std::string(result);
}

} // namespace dmu