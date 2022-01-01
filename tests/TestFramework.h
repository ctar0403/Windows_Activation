#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace test {

struct Case {
    std::string name;
    std::function<void()> body;
};

inline std::vector<Case>& Cases() {
    static std::vector<Case> cases;
    return cases;
}

class Registrar {
public:
    Registrar(std::string name, std::function<void()> body) {
        Cases().push_back({std::move(name), std::move(body)});
    }
};

inline void Require(bool condition, std::string_view expression,
                    std::string_view file, int line) {
    if (!condition) {
        throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                                 ": requirement failed: " + std::string(expression));
    }
}

inline int RunAll() {
    int failures = 0;
    for (const auto& testCase : Cases()) {
        try {
            testCase.body();
            std::cout << "[PASS] " << testCase.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << testCase.name << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << testCase.name << ": unknown exception\n";
        }
    }
    std::cout << (Cases().size() - static_cast<std::size_t>(failures)) << "/"
              << Cases().size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}

}  // namespace test

#define TEST_CONCAT_IMPL(left, right) left##right
#define TEST_CONCAT(left, right) TEST_CONCAT_IMPL(left, right)
#define TEST_CASE(name)                                                        \
    static void TEST_CONCAT(TestBody_, __LINE__)();                            \
    static ::test::Registrar TEST_CONCAT(TestRegistrar_, __LINE__)(            \
        name, TEST_CONCAT(TestBody_, __LINE__));                               \
    static void TEST_CONCAT(TestBody_, __LINE__)()
#define REQUIRE(expression)                                                    \
    ::test::Require(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

