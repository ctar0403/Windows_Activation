#pragma once

#include <Windows.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace winactivate {

class SecureWideString final {
public:
    SecureWideString() = default;
    explicit SecureWideString(std::wstring value) : value_(std::move(value)) {}
    explicit SecureWideString(std::wstring_view value) : value_(value) {}

    SecureWideString(const SecureWideString&) = delete;
    SecureWideString& operator=(const SecureWideString&) = delete;

    SecureWideString(SecureWideString&& other) noexcept
        : value_(std::move(other.value_)) {
        other.clear();
    }

    SecureWideString& operator=(SecureWideString&& other) noexcept {
        if (this != &other) {
            clear();
            value_ = std::move(other.value_);
            other.clear();
        }
        return *this;
    }

    ~SecureWideString() { clear(); }

    [[nodiscard]] const std::wstring& get() const noexcept { return value_; }
    [[nodiscard]] bool empty() const noexcept { return value_.empty(); }

    void clear() noexcept {
        if (!value_.empty()) {
            SecureZeroMemory(value_.data(), value_.size() * sizeof(wchar_t));
            value_.clear();
            value_.shrink_to_fit();
        }
    }

private:
    std::wstring value_;
};

}  // namespace winactivate

