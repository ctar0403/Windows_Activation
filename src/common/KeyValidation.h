#pragma once

#include <string>
#include <string_view>

namespace winactivate {

struct KeyValidationResult {
    bool valid{};
    std::wstring normalized;
    std::wstring error;
};

[[nodiscard]] KeyValidationResult ValidateAndNormalizeProductKey(
    std::wstring_view input);

}  // namespace winactivate

