#pragma once

#include "common/Types.h"

#include <cstdint>
#include <string>

namespace winactivate {

[[nodiscard]] std::wstring FormatErrorCode(std::uint32_t code);
[[nodiscard]] std::wstring GuidanceForCode(std::uint32_t code);
[[nodiscard]] std::wstring DescribeActivationError(const ActivationError& error);

}  // namespace winactivate

