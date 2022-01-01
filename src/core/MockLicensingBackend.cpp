#include "core/MockLicensingBackend.h"

#include "common/KeyValidation.h"

namespace winactivate {

SnapshotQueryResult MockLicensingBackend::QuerySnapshot() {
    ++queryCalls;
    if (snapshots.empty()) {
        SnapshotQueryResult result;
        result.error = ActivationError{ActivationStage::Query, {}, {},
                                       L"The mock has no scripted snapshot.", {}};
        return result;
    }
    SnapshotQueryResult result = snapshots.front();
    if (snapshots.size() > 1) {
        snapshots.pop_front();
    }
    return result;
}

BackendCallResult MockLicensingBackend::InstallProductKey(
    std::wstring_view normalizedKey) {
    ++installCalls;
    const KeyValidationResult validation = ValidateAndNormalizeProductKey(normalizedKey);
    receivedNormalizedKey = validation.valid && validation.normalized == normalizedKey;
    return installResult;
}

BackendCallResult MockLicensingBackend::ActivateProduct(
    std::wstring_view wmiObjectPath) {
    ++activationCalls;
    lastActivatedPath = wmiObjectPath;
    return activationResult;
}

BackendCallResult MockLicensingBackend::RefreshLicenseStatus() {
    ++refreshCalls;
    return refreshResult;
}

}  // namespace winactivate

