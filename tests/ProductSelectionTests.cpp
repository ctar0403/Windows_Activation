#include "TestFramework.h"

#include "core/ProductSelection.h"
#include "core/OsDetector.h"

#include <vector>

namespace {

winactivate::OsInfo ProOs() {
    return winactivate::ClassifyWindows(L"10.0", L"26100", L"Windows 11 Pro", 48, 1);
}

winactivate::LicenseRecord Record(std::wstring id, std::wstring name,
                                  std::uint32_t status,
                                  bool withPartialKey = true) {
    winactivate::LicenseRecord record;
    record.activationId = std::move(id);
    record.applicationId = winactivate::kWindowsApplicationId;
    record.name = std::move(name);
    record.description = L"Windows Operating System, RETAIL channel";
    record.licenseFamily = record.name;
    record.licenseStatus = status;
    if (withPartialKey) {
        record.partialKey = L"ABCDE";
    }
    return record;
}

}  // namespace

TEST_CASE("license status values zero through six map precisely") {
    using winactivate::LicenseSummaryStatus;
    using winactivate::MapLicenseStatus;
    REQUIRE(MapLicenseStatus(0) == LicenseSummaryStatus::NotActivated);
    REQUIRE(MapLicenseStatus(1) == LicenseSummaryStatus::Activated);
    REQUIRE(MapLicenseStatus(2) == LicenseSummaryStatus::Grace);
    REQUIRE(MapLicenseStatus(3) == LicenseSummaryStatus::Grace);
    REQUIRE(MapLicenseStatus(4) == LicenseSummaryStatus::Grace);
    REQUIRE(MapLicenseStatus(5) == LicenseSummaryStatus::Notification);
    REQUIRE(MapLicenseStatus(6) == LicenseSummaryStatus::Grace);
    REQUIRE(MapLicenseStatus(99) == LicenseSummaryStatus::Unknown);
}

TEST_CASE("activated installed edition is selected deterministically") {
    const std::vector records{
        Record(L"home-id", L"Windows 11 Home", 1),
        Record(L"pro-id", L"Windows 11 Professional", 1),
    };
    const auto snapshot = winactivate::SelectApplicableProduct(ProOs(), records);
    REQUIRE(snapshot.chosen.has_value());
    REQUIRE(snapshot.chosen->activationId == L"pro-id");
    REQUIRE(snapshot.status == winactivate::LicenseSummaryStatus::Activated);
    REQUIRE(!snapshot.selectionIsAmbiguous);
}

TEST_CASE("licensed record for another edition cannot produce activated") {
    const std::vector records{Record(L"home-id", L"Windows 11 Home", 1)};
    const auto snapshot = winactivate::SelectApplicableProduct(ProOs(), records);
    REQUIRE(!snapshot.chosen.has_value());
    REQUIRE(snapshot.status == winactivate::LicenseSummaryStatus::Unknown);
}

TEST_CASE("add-ons are excluded") {
    auto addon = Record(L"addon-id", L"Windows 11 Professional", 1);
    addon.isAddon = true;
    const auto snapshot = winactivate::SelectApplicableProduct(ProOs(), {addon});
    REQUIRE(!snapshot.chosen.has_value());
    REQUIRE(snapshot.status == winactivate::LicenseSummaryStatus::Unknown);
}

TEST_CASE("multiple matching records are ambiguous") {
    const auto snapshot = winactivate::SelectApplicableProduct(
        ProOs(), {Record(L"one", L"Windows 11 Professional", 1),
                  Record(L"two", L"Windows 11 Professional", 0)});
    REQUIRE(snapshot.selectionIsAmbiguous);
    REQUIRE(!snapshot.chosen.has_value());
    REQUIRE(snapshot.status == winactivate::LicenseSummaryStatus::Unknown);
}

TEST_CASE("a keyed matching record is preferred over an unkeyed record") {
    const auto snapshot = winactivate::SelectApplicableProduct(
        ProOs(), {Record(L"unkeyed", L"Windows 11 Professional", 0, false),
                  Record(L"keyed", L"Windows 11 Professional", 1, true)});
    REQUIRE(snapshot.chosen.has_value());
    REQUIRE(snapshot.chosen->activationId == L"keyed");
    REQUIRE(snapshot.status == winactivate::LicenseSummaryStatus::Activated);
}

