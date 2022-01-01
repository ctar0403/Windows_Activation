#pragma once

#include "common/Types.h"

#include <vector>

namespace winactivate {

[[nodiscard]] LicenseSummaryStatus MapLicenseStatus(std::uint32_t status);
[[nodiscard]] LicenseSnapshot SelectApplicableProduct(
    const OsInfo& os, const std::vector<LicenseRecord>& records);
[[nodiscard]] std::wstring ReportedChannel(const LicenseRecord& record);

}  // namespace winactivate

