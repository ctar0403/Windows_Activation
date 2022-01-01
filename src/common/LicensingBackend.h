#pragma once

#include "common/Types.h"

#include <optional>
#include <string_view>

namespace winactivate {

struct SnapshotQueryResult {
    LicenseSnapshot snapshot;
    std::optional<ActivationError> error;

    [[nodiscard]] bool succeeded() const noexcept { return !error.has_value(); }
};

class ILicensingBackend {
public:
    virtual ~ILicensingBackend() = default;
    [[nodiscard]] virtual SnapshotQueryResult QuerySnapshot() = 0;
};

struct BackendCallResult {
    bool succeeded{};
    std::optional<std::int32_t> hresult;
    std::optional<std::uint32_t> wmiReturnValue;
};

class IActivationBackend : public ILicensingBackend {
public:
    [[nodiscard]] virtual BackendCallResult InstallProductKey(
        std::wstring_view normalizedKey) = 0;
    [[nodiscard]] virtual BackendCallResult ActivateProduct(
        std::wstring_view wmiObjectPath) = 0;
    [[nodiscard]] virtual BackendCallResult RefreshLicenseStatus() = 0;
};

}  // namespace winactivate

