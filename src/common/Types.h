#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace winactivate {

inline constexpr wchar_t kWindowsApplicationId[] =
    L"55c92734-d682-4d71-983e-d6ec3f16059f";

enum class WindowsFamily {
    Unsupported,
    Win10,
    Win11,
};

struct OsInfo {
    WindowsFamily family{WindowsFamily::Unsupported};
    std::uint32_t build{};
    std::wstring version;
    std::wstring caption;
    std::uint32_t sku{};
    std::wstring edition;
    bool isWorkstation{};
    bool isSupported{};
};

struct LicenseRecord {
    std::wstring wmiObjectPath;
    std::wstring activationId;
    std::wstring applicationId;
    std::wstring name;
    std::wstring description;
    std::wstring licenseFamily;
    std::uint32_t licenseStatus{};
    std::uint32_t licenseStatusReason{};
    std::optional<std::wstring> partialKey;
    bool isAddon{};
};

enum class LicenseSummaryStatus {
    Activated,
    NotActivated,
    Grace,
    Notification,
    Unknown,
};

struct LicenseSnapshot {
    OsInfo os;
    std::vector<LicenseRecord> records;
    std::optional<LicenseRecord> chosen;
    LicenseSummaryStatus status{LicenseSummaryStatus::Unknown};
    bool selectionIsAmbiguous{};
    std::wstring reason;
};

enum class ActivationStage {
    Query,
    Validate,
    Elevate,
    Install,
    Activate,
    Refresh,
    Verify,
    Ipc,
};

struct ActivationError {
    ActivationStage stage{ActivationStage::Query};
    std::optional<std::int32_t> hresult;
    std::optional<std::uint32_t> wmiReturnValue;
    std::wstring message;
    std::wstring remediationUrl;
};

struct ActivationResult {
    bool installAttempted{};
    bool installReportedSuccess{};
    bool activationAttempted{};
    bool activationReportedSuccess{};
    LicenseSnapshot finalSnapshot;
    bool verifiedActivated{};
    std::optional<ActivationError> error;
};

}  // namespace winactivate

