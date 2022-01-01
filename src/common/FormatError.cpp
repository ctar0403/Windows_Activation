#include "common/FormatError.h"

#include <Windows.h>

#include <iomanip>
#include <sstream>

namespace winactivate {
namespace {

std::wstring StageName(ActivationStage stage) {
    switch (stage) {
        case ActivationStage::Query:
            return L"status query";
        case ActivationStage::Validate:
            return L"key validation";
        case ActivationStage::Elevate:
            return L"administrator approval";
        case ActivationStage::Install:
            return L"key installation";
        case ActivationStage::Activate:
            return L"activation";
        case ActivationStage::Refresh:
            return L"license refresh";
        case ActivationStage::Verify:
            return L"activation verification";
        case ActivationStage::Ipc:
            return L"helper communication";
    }
    return L"operation";
}

}  // namespace

std::wstring FormatErrorCode(std::uint32_t code) {
    std::wostringstream stream;
    stream << L"0x" << std::uppercase << std::hex << std::setw(8)
           << std::setfill(L'0') << code;
    return stream.str();
}

std::wstring GuidanceForCode(std::uint32_t code) {
    switch (code) {
        case 0xC004F050:
            return L"The key is invalid or does not apply. Check the key and Windows edition.";
        case 0xC004F210:
        case 0xC004F212:
            return L"The key is for a different Windows edition. Verify Home, Pro, Education, or Enterprise.";
        case 0x803F7001:
            return L"No matching valid license was found. Use a genuine key or the official Activation troubleshooter.";
        case 0xC004C003:
            return L"The key was rejected or blocked. Contact the seller or Microsoft Support.";
        case 0xC004F074:
        case 0x8007232B:
            return L"An organization-managed activation service could not be reached. Connect to the managed network or contact IT.";
        case 0x80072F8F:
            return L"Check the device date and time, TLS connectivity, and internet access.";
        case 0x80070005:
            return L"Access was denied. Check administrator approval, policy, and WMI permissions.";
        default:
            return {};
    }
}

std::wstring DescribeActivationError(const ActivationError& error) {
    std::wstring description = L"Failure during " + StageName(error.stage) + L".";
    std::optional<std::uint32_t> code;
    if (error.wmiReturnValue.has_value()) {
        code = *error.wmiReturnValue;
    } else if (error.hresult.has_value()) {
        code = static_cast<std::uint32_t>(*error.hresult);
    }

    if (code.has_value()) {
        description += L" Code " + FormatErrorCode(*code) + L".";
    }
    if (!error.message.empty()) {
        description += L" " + error.message;
    }
    if (code.has_value()) {
        const std::wstring guidance = GuidanceForCode(*code);
        if (!guidance.empty()) {
            description += L" " + guidance;
        }
    }
    return description;
}

}  // namespace winactivate

