#pragma once

#include "common/Types.h"

#include <cstdint>
#include <string_view>

namespace winactivate {

[[nodiscard]] OsInfo ClassifyWindows(std::wstring_view version,
                                     std::wstring_view buildNumber,
                                     std::wstring_view caption,
                                     std::uint32_t sku,
                                     std::uint32_t productType);
[[nodiscard]] std::wstring EditionName(std::uint32_t sku,
                                       std::wstring_view caption);

}  // namespace winactivate

