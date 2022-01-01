#include "core/ProductSelection.h"

#include <algorithm>
#include <cwctype>
#include <string_view>

namespace winactivate {
namespace {

std::wstring Lower(std::wstring_view value) {
    std::wstring lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](wchar_t character) {
                       return static_cast<wchar_t>(std::towlower(character));
                   });
    return lowered;
}

bool EqualsInsensitive(std::wstring_view left, std::wstring_view right) {
    return Lower(left) == Lower(right);
}

bool Contains(const std::wstring& text, std::wstring_view term) {
    return text.find(term) != std::wstring::npos;
}

bool MatchesInstalledEdition(std::uint32_t sku, const LicenseRecord& record) {
    const std::wstring text = Lower(record.name + L" " + record.licenseFamily +
                                    L" " + record.description);
    switch (sku) {
        case 2:
            return Contains(text, L"home basic");
        case 3:
            return Contains(text, L"home premium");
        case 98:
            return (Contains(text, L"core n") || Contains(text, L"home n")) &&
                   !Contains(text, L"single language");
        case 100:
            return Contains(text, L"single language");
        case 101:
            return (Contains(text, L"core") || Contains(text, L"home")) &&
                   !Contains(text, L"core n") && !Contains(text, L"home n") &&
                   !Contains(text, L"single language");
        case 48:
            return (Contains(text, L"professional") || Contains(text, L" pro")) &&
                   !Contains(text, L"workstation") && !Contains(text, L"education") &&
                   !Contains(text, L"enterprise") && !Contains(text, L"professionaln");
        case 49:
            return (Contains(text, L"professionaln") ||
                    Contains(text, L"professional n") || Contains(text, L"pro n")) &&
                   !Contains(text, L"workstation");
        case 4:
            return Contains(text, L"enterprise") && !Contains(text, L"enterprise n");
        case 27:
            return Contains(text, L"enterprise n");
        case 121:
            return Contains(text, L"education") && !Contains(text, L"education n");
        case 122:
            return Contains(text, L"education n");
        case 161:
            return Contains(text, L"workstation") && !Contains(text, L"workstation n");
        case 162:
            return Contains(text, L"workstation n");
        default:
            return false;
    }
}

bool IsSameRecord(const LicenseRecord* left, const LicenseRecord* right) {
    return !left->activationId.empty() &&
           EqualsInsensitive(left->activationId, right->activationId);
}

}  // namespace

LicenseSummaryStatus MapLicenseStatus(std::uint32_t status) {
    switch (status) {
        case 0:
            return LicenseSummaryStatus::NotActivated;
        case 1:
            return LicenseSummaryStatus::Activated;
        case 2:
        case 3:
        case 4:
        case 6:
            return LicenseSummaryStatus::Grace;
        case 5:
            return LicenseSummaryStatus::Notification;
        default:
            return LicenseSummaryStatus::Unknown;
    }
}

LicenseSnapshot SelectApplicableProduct(const OsInfo& os,
                                        const std::vector<LicenseRecord>& records) {
    LicenseSnapshot snapshot;
    snapshot.os = os;
    snapshot.records = records;

    if (!os.isSupported) {
        snapshot.reason = L"This operating system is not a supported Windows 10 or Windows 11 client.";
        return snapshot;
    }

    std::vector<const LicenseRecord*> candidates;
    for (const LicenseRecord& record : records) {
        if (!record.isAddon &&
            EqualsInsensitive(record.applicationId, kWindowsApplicationId) &&
            MatchesInstalledEdition(os.sku, record)) {
            candidates.push_back(&record);
        }
    }

    if (candidates.empty()) {
        snapshot.selectionIsAmbiguous = !records.empty();
        snapshot.reason = records.empty()
                              ? L"No Windows licensing records were returned."
                              : L"No licensing record could be matched safely to the installed Windows edition.";
        return snapshot;
    }

    std::vector<const LicenseRecord*> keyed;
    std::copy_if(candidates.begin(), candidates.end(), std::back_inserter(keyed),
                 [](const LicenseRecord* record) {
                     return record->partialKey.has_value() &&
                            !record->partialKey->empty();
                 });
    std::vector<const LicenseRecord*>& preferred = keyed.empty() ? candidates : keyed;
    const LicenseRecord* selected = preferred.front();
    const bool allSame = std::all_of(preferred.begin() + 1, preferred.end(),
                                     [selected](const LicenseRecord* record) {
                                         return IsSameRecord(selected, record);
                                     });
    if (preferred.size() > 1 && !allSame) {
        snapshot.selectionIsAmbiguous = true;
        snapshot.reason = L"Multiple licensing records match the installed Windows edition; no record was selected.";
        return snapshot;
    }

    snapshot.chosen = *selected;
    snapshot.status = MapLicenseStatus(selected->licenseStatus);
    snapshot.reason = snapshot.status == LicenseSummaryStatus::Activated
                          ? L"The applicable Windows licensing record is licensed."
                          : L"The applicable Windows licensing record is not currently licensed.";
    return snapshot;
}

std::wstring ReportedChannel(const LicenseRecord& record) {
    const std::wstring description = Lower(record.description);
    if (Contains(description, L"retail")) {
        return L"Retail";
    }
    if (Contains(description, L"oem")) {
        return L"OEM";
    }
    if (Contains(description, L"volume")) {
        return L"Volume";
    }
    return L"Unknown";
}

}  // namespace winactivate

