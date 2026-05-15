#ifndef __DMU_LOGGER_HH__
#define __DMU_LOGGER_HH__

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>


// 包含SystemC头文件以支持report兼容性
#include <systemc>

namespace dmu {

// 定义日志级别枚举
enum class LogLevel {
  TRACE = 0,
  DBG = 1,
  INFO = 2,
  WARN = 3,
  ERROR = 4,
  FATAL = 5
};

class Logger {
public:
  // 获取默认实例
  static Logger &getInstance();

  // 获取指定名称的Logger实例
  static Logger &getInstance(const std::string &name);

  // 初始化日志文件
  bool initialize(const std::string &path, const std::string &filename);

  // 初始化指定名称的Logger的日志文件
  bool initialize(const std::string &name, const std::string &path,
                  const std::string &filename);

  // 写入日志消息到默认logger
  void write(LogLevel level, const std::string &message);

  // 写入日志消息到指定名称的logger
  void write(const std::string &name, LogLevel level,
             const std::string &message);

  // 写入日志消息到默认logger (字符串级别)
  void write(const std::string &level, const std::string &message);

  // 写入日志消息到指定名称的Logger (字符串级别)
  void write(const std::string &name, const std::string &level,
             const std::string &message);

  // 关闭默认logger的日志文件
  void close();

  // 关闭指定名称的logger的日志文件
  void close(const std::string &name);

  // 检查默认logger是否已初始化
  bool isInitialized() const;

  // 检查指定名称的logger是否已初始化
  bool isInitialized(const std::string &name) const;

  // 设置日志级别过滤
  void setLogLevel(LogLevel level);
  void setLogLevel(const std::string &name, LogLevel level);

  // 获取日志级别过滤设置
  LogLevel getLogLevel() const;
  LogLevel getLogLevel(const std::string &name) const;

  // 设置全局日志启用/禁用开关
  static void setGlobalEnabled(bool enabled);
  static bool isGlobalEnabled();

  // 设置指定logger的启用/禁用开关
  void setEnabled(bool enabled);
  void setEnabled(const std::string &name, bool enabled);

  // 检查指定logger是否启用
  bool isEnabled() const;
  bool isEnabled(const std::string &name) const;

  // 关闭所有logger
  static void closeAll();

private:
  struct LoggerData {
    std::ofstream logFile;
    bool initialized = false;
    bool enabled = true; // 默认启用
    std::string fullLogPath;
    LogLevel minLevel = LogLevel::TRACE; // 默认记录所有级别的日志
  };

  Logger(const std::string &name = ""); // 私有构造函数

public:
  ~Logger(); // 析构函数

private:
  // 删除拷贝构造函数和赋值操作符
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  mutable std::mutex logMutex;
  std::map<std::string, std::unique_ptr<LoggerData>>
      loggers;               // 每个logger实例管理自己的日志文件
  std::string instanceName_; // 当前实例的名称
  static std::mutex globalMutex;
  static std::unique_ptr<Logger> defaultInstance;
  static std::map<std::string, std::unique_ptr<Logger>> namedInstances;
  static bool globalEnabled; // 全局日志开关

  // 将LogLevel转换为字符串
  std::string levelToString(LogLevel level) const;

  // 获取当前时间字符串
  std::string getCurrentTimeString() const;
};

} // namespace dmu

// 日志宏定义 - 针对默认logger
#define DMU_LOG_TRACE(msg)                                                     \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance().write(dmu::LogLevel::TRACE, ss.str());        \
    }                                                                          \
  } while (0)

#define DMU_LOG_DEBUG(msg)                                                     \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance().write(dmu::LogLevel::DBG, ss.str());          \
    }                                                                          \
  } while (0)

#define DMU_LOG_INFO(msg)                                                      \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance().write(dmu::LogLevel::INFO, ss.str());         \
    }                                                                          \
  } while (0)

#define DMU_LOG_WARN(msg)                                                      \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance().write(dmu::LogLevel::WARN, ss.str());         \
    }                                                                          \
  } while (0)

#define DMU_LOG_ERROR(msg)                                                     \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance().write(dmu::LogLevel::ERROR, ss.str());        \
    }                                                                          \
  } while (0)

#define DMU_LOG_FATAL(msg)                                                     \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance().write(dmu::LogLevel::FATAL, ss.str());        \
    }                                                                          \
  } while (0)

// 带logger名称的日志宏定义
#define DMU_LOG_TRACE_N(name, msg)                                             \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance(name).write(name, dmu::LogLevel::TRACE,         \
                                           ss.str());                          \
    }                                                                          \
  } while (0)

#define DMU_LOG_DEBUG_N(name, msg)                                             \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance(name).write(name, dmu::LogLevel::DBG,           \
                                           ss.str());                          \
    }                                                                          \
  } while (0)

#define DMU_LOG_INFO_N(name, msg)                                              \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance(name).write(name, dmu::LogLevel::INFO,          \
                                           ss.str());                          \
    }                                                                          \
  } while (0)

#define DMU_LOG_WARN_N(name, msg)                                              \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance(name).write(name, dmu::LogLevel::WARN,          \
                                           ss.str());                          \
    }                                                                          \
  } while (0)

#define DMU_LOG_ERROR_N(name, msg)                                             \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance(name).write(name, dmu::LogLevel::ERROR,         \
                                           ss.str());                          \
    }                                                                          \
  } while (0)

#define DMU_LOG_FATAL_N(name, msg)                                             \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << msg;                                                               \
      dmu::Logger::getInstance(name).write(name, dmu::LogLevel::FATAL,         \
                                           ss.str());                          \
    }                                                                          \
  } while (0)

// printf-style 宏定义 - 针对默认logger
#define DMU_LOG_TRACE_F(format, ...)                                           \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance().write(dmu::LogLevel::TRACE,                   \
                                       std::string(__fmt_buf));                \
    }                                                                          \
  } while (0)

#define DMU_LOG_DEBUG_F(format, ...)                                           \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance().write(dmu::LogLevel::DBG,                     \
                                       std::string(__fmt_buf));                \
    }                                                                          \
  } while (0)

#define DMU_LOG_INFO_F(format, ...)                                            \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance().write(dmu::LogLevel::INFO,                    \
                                       std::string(__fmt_buf));                \
    }                                                                          \
  } while (0)

#define DMU_LOG_WARN_F(format, ...)                                            \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance().write(dmu::LogLevel::WARN,                    \
                                       std::string(__fmt_buf));                \
    }                                                                          \
  } while (0)

#define DMU_LOG_ERROR_F(format, ...)                                           \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance().write(dmu::LogLevel::ERROR,                   \
                                       std::string(__fmt_buf));                \
    }                                                                          \
  } while (0)

#define DMU_LOG_FATAL_F(format, ...)                                           \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance().write(dmu::LogLevel::FATAL,                   \
                                       std::string(__fmt_buf));                \
    }                                                                          \
  } while (0)

// printf-style 宏定义 - 针对指定名称的logger
#define DMU_LOG_TRACE_NF(name, format, ...)                                    \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      const std::string __logger_name = (name);                                \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance(__logger_name)                                  \
          .write(__logger_name, dmu::LogLevel::TRACE, std::string(__fmt_buf)); \
    }                                                                          \
  } while (0)

#define DMU_LOG_DEBUG_NF(name, format, ...)                                    \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      const std::string __logger_name = (name);                                \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance(__logger_name)                                  \
          .write(__logger_name, dmu::LogLevel::DBG, std::string(__fmt_buf));   \
    }                                                                          \
  } while (0)

#define DMU_LOG_INFO_NF(name, format, ...)                                     \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      const std::string __logger_name = (name);                                \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance(__logger_name)                                  \
          .write(__logger_name, dmu::LogLevel::INFO, std::string(__fmt_buf));  \
    }                                                                          \
  } while (0)

#define DMU_LOG_WARN_NF(name, format, ...)                                     \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      const std::string __logger_name = (name);                                \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance(__logger_name)                                  \
          .write(__logger_name, dmu::LogLevel::WARN, std::string(__fmt_buf));  \
    }                                                                          \
  } while (0)

#define DMU_LOG_ERROR_NF(name, format, ...)                                    \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      const std::string __logger_name = (name);                                \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance(__logger_name)                                  \
          .write(__logger_name, dmu::LogLevel::ERROR, std::string(__fmt_buf)); \
    }                                                                          \
  } while (0)

#define DMU_LOG_FATAL_NF(name, format, ...)                                    \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      const std::string __logger_name = (name);                                \
      char __fmt_buf[4096] = {0};                                              \
      snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__);           \
      dmu::Logger::getInstance(__logger_name)                                  \
          .write(__logger_name, dmu::LogLevel::FATAL, std::string(__fmt_buf)); \
    }                                                                          \
  } while (0)

// SystemC report兼容性宏 - 针对默认logger
#define DMU_SC_REPORT_INFO(name, msg)                                          \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << "[SystemC INFO] [" << name << "] " << msg;                         \
      dmu::Logger::getInstance().write(dmu::LogLevel::INFO, ss.str());         \
    }                                                                          \
  } while (0)

#define DMU_SC_REPORT_WARNING(name, msg)                                       \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << "[SystemC WARNING] [" << name << "] " << msg;                      \
      dmu::Logger::getInstance().write(dmu::LogLevel::WARN, ss.str());         \
    }                                                                          \
  } while (0)

#define DMU_SC_REPORT_ERROR(name, msg)                                         \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << "[SystemC ERROR] [" << name << "] " << msg;                        \
      dmu::Logger::getInstance().write(dmu::LogLevel::ERROR, ss.str());        \
    }                                                                          \
  } while (0)

#define DMU_SC_REPORT_FATAL(name, msg)                                         \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << "[SystemC FATAL] [" << name << "] " << msg;                        \
      dmu::Logger::getInstance().write(dmu::LogLevel::FATAL, ss.str());        \
    }                                                                          \
  } while (0)

// SystemC report兼容性宏 - 针对指定名称的logger
#define DMU_SC_REPORT_INFO_N(loggerName, scName, msg)                          \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << "[SystemC INFO] [" << scName << "] " << msg;                       \
      dmu::Logger::getInstance(loggerName)                                     \
          .write(loggerName, dmu::LogLevel::INFO, ss.str());                   \
    }                                                                          \
  } while (0)

#define DMU_SC_REPORT_WARNING_N(loggerName, scName, msg)                       \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << "[SystemC WARNING] [" << scName << "] " << msg;                    \
      dmu::Logger::getInstance(loggerName)                                     \
          .write(loggerName, dmu::LogLevel::WARN, ss.str());                   \
    }                                                                          \
  } while (0)

#define DMU_SC_REPORT_ERROR_N(loggerName, scName, msg)                         \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << "[SystemC ERROR] [" << scName << "] " << msg;                      \
      dmu::Logger::getInstance(loggerName)                                     \
          .write(loggerName, dmu::LogLevel::ERROR, ss.str());                  \
    }                                                                          \
  } while (0)

#define DMU_SC_REPORT_FATAL_N(loggerName, scName, msg)                         \
  do {                                                                         \
    if (dmu::Logger::isGlobalEnabled()) {                                      \
      std::stringstream ss;                                                    \
      ss << "[SystemC FATAL] [" << scName << "] " << msg;                      \
      dmu::Logger::getInstance(loggerName)                                     \
          .write(loggerName, dmu::LogLevel::FATAL, ss.str());                  \
    }                                                                          \
  } while (0)

#endif // __DMU_LOGGER_HH__