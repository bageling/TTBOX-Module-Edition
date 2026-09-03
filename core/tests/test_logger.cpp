// test_logger.cpp — Logger 级别过滤 + sink 输出验证
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "common/Logger.hpp"
#include "test_util.hpp"

namespace {

// 捕获型 sink：记录收到的行
class CaptureSink : public ttbox::core::LogSink {
public:
    void write(ttbox::core::LogLevel level, const std::string& line) override {
        lines.push_back(line);
        last_level = level;
        count.fetch_add(1);
    }
    std::vector<std::string> lines;
    ttbox::core::LogLevel last_level = ttbox::core::LogLevel::kDebug;
    std::atomic<int> count{0};
};

}  // namespace

TEST(logger_writes_with_level_tag) {
    auto& logger = ttbox::core::Logger::instance();
    logger.clear_sinks();
    auto sink = std::make_shared<CaptureSink>();
    logger.add_sink(sink);
    logger.set_level(ttbox::core::LogLevel::kDebug);

    logger.log(ttbox::core::LogLevel::kInfo, "hello-info", __FILE__, __LINE__);
    CHECK(sink->count.load() == 1);
    CHECK(!sink->lines.empty());
    CHECK(sink->lines[0].find("[INFO ]") != std::string::npos);
    CHECK(sink->lines[0].find("hello-info") != std::string::npos);
    CHECK(sink->lines[0].find("test_logger.cpp") != std::string::npos);  // 文件名注入
}

TEST(logger_level_filter_blocks_debug) {
    auto& logger = ttbox::core::Logger::instance();
    logger.clear_sinks();
    auto sink = std::make_shared<CaptureSink>();
    logger.add_sink(sink);
    logger.set_level(ttbox::core::LogLevel::kWarn);  // 过滤 Debug/Info

    logger.log(ttbox::core::LogLevel::kDebug, "d", __FILE__, __LINE__);
    logger.log(ttbox::core::LogLevel::kInfo, "i", __FILE__, __LINE__);
    CHECK(sink->count.load() == 0);

    logger.log(ttbox::core::LogLevel::kWarn, "w", __FILE__, __LINE__);
    logger.log(ttbox::core::LogLevel::kError, "e", __FILE__, __LINE__);
    CHECK(sink->count.load() == 2);
}

TEST(logger_macro_basic) {
    auto& logger = ttbox::core::Logger::instance();
    logger.clear_sinks();
    auto sink = std::make_shared<CaptureSink>();
    logger.add_sink(sink);
    logger.set_level(ttbox::core::LogLevel::kInfo);

    TTBOX_LOG_INFO("macro-info");
    TTBOX_LOG_WARN("macro-warn");
    CHECK(sink->count.load() == 2);
}
