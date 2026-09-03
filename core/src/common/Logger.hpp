// Logger.hpp — 轻量日志（stdout/stderr，预留 journald/file sink）
//
// 不引入大型日志框架。Sink 抽象允许未来扩展：
//   - ConsoleSink（默认，INFO 及以下 -> stdout，WARN/ERROR -> stderr）
//   - 未来 JournaldSink / FileSink 通过 add_sink() 接入
#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ttbox::core {

enum class LogLevel : int {
    kDebug = 0,
    kInfo = 1,
    kWarn = 2,
    kError = 3,
    kOff = 4,
};

// 日志落地点抽象（未来扩展点）
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(LogLevel level, const std::string& line) = 0;
};

// 默认控制台 sink：INFO/DEBUG -> stdout，WARN/ERROR -> stderr
class ConsoleSink : public LogSink {
public:
    void write(LogLevel level, const std::string& line) override;
};

class Logger {
public:
    static Logger& instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void set_level(LogLevel level);
    LogLevel level() const { return level_; }

    void add_sink(std::shared_ptr<LogSink> sink);
    void clear_sinks();  // 重置 sink 列表（测试/重配置用）

    // 线程安全写入；file/line 由日志宏填充
    void log(LogLevel level, const std::string& msg, const char* file, int line);

private:
    Logger() = default;

    mutable std::mutex mutex_;
    LogLevel level_ = LogLevel::kInfo;
    std::vector<std::shared_ptr<LogSink>> sinks_;
};

// 日志宏（自动携带源文件与行号）
#define TTBOX_LOG_DEBUG(msg) \
    ::ttbox::core::Logger::instance().log(::ttbox::core::LogLevel::kDebug, (msg), __FILE__, __LINE__)
#define TTBOX_LOG_INFO(msg) \
    ::ttbox::core::Logger::instance().log(::ttbox::core::LogLevel::kInfo, (msg), __FILE__, __LINE__)
#define TTBOX_LOG_WARN(msg) \
    ::ttbox::core::Logger::instance().log(::ttbox::core::LogLevel::kWarn, (msg), __FILE__, __LINE__)
#define TTBOX_LOG_ERROR(msg) \
    ::ttbox::core::Logger::instance().log(::ttbox::core::LogLevel::kError, (msg), __FILE__, __LINE__)

}  // namespace ttbox::core
