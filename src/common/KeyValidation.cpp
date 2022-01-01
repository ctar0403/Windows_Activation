#include "common/KeyValidation.h"

#include <cwctype>

namespace winactivate {
namespace {

bool IsAsciiAlphaNumeric(wchar_t character) noexcept {
    return (character >= L'A' && character <= L'Z') ||
           (character >= L'a' && character <= L'z') ||
           (character >= L'0' && character <= L'9');
}

}  // namespace

KeyValidationResult ValidateAndNormalizeProductKey(std::wstring_view input) {
    std::wstring compact;
    compact.reserve(25);

    for (const wchar_t character : input) {
        if (character == L'-') {
            continue;
        }
        if (!IsAsciiAlphaNumeric(character)) {
            return {false, {},
                    L"A product key may contain only ASCII letters, digits, and dashes."};
        }
        compact.push_back(static_cast<wchar_t>(std::towupper(character)));
    }

    if (compact.size() != 25) {
        return {false, {}, L"A product key must contain exactly 25 letters and digits."};
    }

    std::wstring normalized;
    normalized.reserve(29);
    for (std::size_t index = 0; index < compact.size(); ++index) {
        if (index != 0 && index % 5 == 0) {
            normalized.push_back(L'-');
        }
        normalized.push_back(compact[index]);
    }

    return {true, std::move(normalized), {}};
}

}  // namespace winactivate

