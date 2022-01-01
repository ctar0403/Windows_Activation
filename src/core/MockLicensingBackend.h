#pragma once

#include "common/LicensingBackend.h"

#include <deque>
#include <string>

namespace winactivate {

class MockLicensingBackend final : public IActivationBackend {
public:
    std::deque<SnapshotQueryResult> snapshots;
    BackendCallResult installResult{true, {}, 0};
    BackendCallResult activationResult{true, {}, 0};
    BackendCallResult refreshResult{true, {}, 0};

    std::size_t queryCalls{};
    std::size_t installCalls{};
    std::size_t activationCalls{};
    std::size_t refreshCalls{};
    std::wstring lastActivatedPath;
    bool receivedNormalizedKey{};

    [[nodiscard]] SnapshotQueryResult QuerySnapshot() override;
    [[nodiscard]] BackendCallResult InstallProductKey(
        std::wstring_view normalizedKey) override;
    [[nodiscard]] BackendCallResult ActivateProduct(
        std::wstring_view wmiObjectPath) override;
    [[nodiscard]] BackendCallResult RefreshLicenseStatus() override;
};

}  // namespace winactivate

