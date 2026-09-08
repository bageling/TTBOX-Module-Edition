// test_util.hpp — 极简测试框架（零第三方依赖，C++17）
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace ttbox_test {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline int& failure_counter() {
    static int failures = 0;
    return failures;
}

inline void report_failure(const char* file, int line, const std::string& expr) {
    ++failure_counter();
    std::printf("  [FAIL] %s:%d  %s\n", file, line, expr.c_str());
}

inline int failure_count() {
    return failure_counter();
}

inline int run_all() {
    int passed = 0;
    int failed = 0;
    for (const auto& tc : registry()) {
        const int before = failure_count();
        std::printf("== RUN: %s\n", tc.name);
        std::fflush(stdout);
        tc.fn();
        const int after = failure_count();
        if (after > before) {
            ++failed;
            std::printf("== FAIL: %s (%d assertion(s) failed)\n", tc.name, after - before);
        } else {
            ++passed;
            std::printf("== OK: %s\n", tc.name);
        }
    }
    std::printf("\n%d tests passed, %d tests failed (%d total assertions failed)\n",
                passed, failed, failure_count());
    return failed;
}

}  // namespace ttbox_test

// 断言宏（计数失败，不中断）
#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            ::ttbox_test::report_failure(__FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define CHECK_EQ(a, b) \
    do { \
        auto va = (a); \
        auto vb = (b); \
        if (!(va == vb)) { \
            ::ttbox_test::report_failure(__FILE__, __LINE__, \
                std::string(#a " == " #b " (") + std::to_string(va) + " vs " + std::to_string(vb) + ")"); \
        } \
    } while (0)

#define CHECK_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            ::ttbox_test::report_failure(__FILE__, __LINE__, #a " != " #b); \
        } \
    } while (0)

#define TEST(name) \
    static void test_##name(); \
    static ::ttbox_test::Registrar reg_##name(#name, &test_##name); \
    static void test_##name()
