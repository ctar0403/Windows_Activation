#include "core/OsDetector.h"

#include <limits>
#include <string>

namespace winactivate {
namespace {

bool ParseUnsigned(std::wstring_view text, std::uint32_t& value) {
    if (text.empty()) {
        return false;
    }
    std::uint64_t parsed = 0;
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') {
            return false;
        }
        parsed = parsed * 10 + static_cast<unsigned>(character - L'0');
        if (parsed > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool IsVersion10(std::wstring_view version) {
    const std::size_t dot = version.find(L'.');
    if (dot == std::wstring_view::npos) {
        return false;
    }
    const std::size_t secondDot = version.find(L'.', dot + 1);
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    return ParseUnsigned(version.substr(0, dot), major) &&
           ParseUnsigned(version.substr(
               dot + 1, secondDot == std::wstring_view::npos
                            ? std::wstring_view::npos
                            : secondDot - dot - 1),
                         minor) &&
           major == 10 && minor == 0;
}

}  // namespace

std::wstring EditionName(std::uint32_t sku, std::wstring_view caption) {
    switch (sku) {
        case 2:
            return L"Home Basic";
        case 3:
            return L"Home Premium";
        case 4:
            return L"Enterprise";
        case 27:
            return L"Enterprise N";
        case 48:
            return L"Pro";
        case 49:
            return L"Pro N";
        case 98:
            return L"Home N";
        case 100:
            return L"Home Single Language";
        case 101:
            return L"Home";
        case 121:
            return L"Education";
        case 122:
            return L"Education N";
        case 161:
            return L"Pro for Workstations";
        case 162:
            return L"Pro for Workstations N";
        default:
            return caption.empty() ? L"Unknown edition" : std::wstring(caption);
    }
}

OsInfo ClassifyWindows(std::wstring_view version, std::wstring_view buildNumber,
                       std::wstring_view caption, std::uint32_t sku,
                       std::uint32_t productType) {
    OsInfo result;
    result.version = version;
    result.caption = caption;
    result.sku = sku;
    result.edition = EditionName(sku, caption);
    result.isWorkstation = productType == 1;

    std::uint32_t build = 0;
    if (!result.isWorkstation || !IsVersion10(version) ||
        !ParseUnsigned(buildNumber, build) || build < 10240) {
        return result;
    }

    result.build = build;
    result.family = build >= 22000 ? WindowsFamily::Win11 : WindowsFamily::Win10;
    result.isSupported = true;
    return result;
}

}  // namespace winactivate

