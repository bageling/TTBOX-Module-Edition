// Logger.cpp — 轻量日志实现
/*
 * TTBOX 文件说明
 *
 * 文件：Logger.cpp
 *
 * 作用：
 *   TTBOX 的日志系统。
 *   记录程序运行过程中的信息、警告和错误。
 *
 * 小白理解：
 *   就像飞机的黑匣子一样，日志记录了程序运行中的所有重要事件。
 *   出问题时，先看日志找原因。
 *
 * 注意：
 *   本注释仅用于说明代码，不改变程序逻辑。
 */

#include "Logger.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace ttbox::core {

namespace {

const char* level_name(LogLevel level) {
    switch (level) {
        case LogLevel::kDebug: return "DEBUG";
        case LogLevel::kInfo: return "INFO ";
        case LogLevel::kWarn: return "WARN ";
        case LogLevel::kError: return "ERROR";
        case LogLevel::kOff: return "OFF  ";
    }
    return "?????";
}

std::string format_timestamp() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t t = clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, static_cast<int>(ms));
    return buf;
}

}  // namespace

void ConsoleSink::write(LogLevel level, const std::string& line) {
    if (level == LogLevel::kWarn || level == LogLevel::kError) {
        std::fprintf(stderr, "%s\n", line.c_str());
    } else {
        std::fprintf(stdout, "%s\n", line.c_str());
    }
    std::fflush(nullptr);
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::add_sink(std::shared_ptr<LogSink> sink) {
    if (!sink) return;
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::clear_sinks() {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

void Logger::log(LogLevel level, const std::string& msg, const char* file, int line) {
    if (static_cast<int>(level) < static_cast<int>(level_)) return;

    // 提取文件名（去掉路径）
    const char* base = file;
    if (const char* slash = std::strrchr(file, '/'); slash != nullptr) {
        base = slash + 1;
    } else if (const char* bs = std::strrchr(file, '\\'); bs != nullptr) {
        base = bs + 1;
    }

    std::string line_text = "[" + format_timestamp() + "] [" + level_name(level) + "] " +
                            base + ":" + std::to_string(line) + ": " + msg;

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) {
        if (sink) sink->write(level, line_text);
    }
}

}  // namespace ttbox::core
