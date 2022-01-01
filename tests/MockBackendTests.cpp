#include "TestFramework.h"

#include "core/ActivationWorkflow.h"
#include "core/MockLicensingBackend.h"
#include "core/OsDetector.h"
#include "core/ProductSelection.h"

namespace {

winactivate::LicenseSnapshot Snapshot(std::uint32_t status) {
    winactivate::LicenseSnapshot snapshot;
    snapshot.os = winactivate::ClassifyWindows(L"10.0", L"26100", L"Windows 11 Pro", 48, 1);
    winactivate::LicenseRecord record;
    record.wmiObjectPath = L"mock-product-path";
    record.applicationId = winactivate::kWindowsApplicationId;
    record.name = L"Windows Professional";
    record.licenseStatus = status;
    record.partialKey = L"ABCDE";
    snapshot.chosen = record;
    snapshot.records.push_back(record);
    snapshot.status = winactivate::MapLicenseStatus(status);
    return snapshot;
}

winactivate::SnapshotQueryResult Query(std::uint32_t status) {
    return {Snapshot(status), {}};
}

constexpr wchar_t kSyntheticKey[] = L"ABCDE-FGHIJ-KLMNO-PQRST-UVWXY";

}  // namespace

TEST_CASE("install failure never proceeds to activation and still refreshes status") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Query(0), Query(0)};
    backend.installResult = {false, {}, 0xC004F050};
    winactivate::ActivationWorkflow workflow(backend);
    const auto result = workflow.InstallAndActivate(kSyntheticKey, false);
    REQUIRE(result.installAttempted);
    REQUIRE(!result.activationAttempted);
    REQUIRE(backend.installCalls == 1);
    REQUIRE(backend.activationCalls == 0);
    REQUIRE(backend.queryCalls == 2);
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->stage == winactivate::ActivationStage::Install);
}

TEST_CASE("successful install activates only the reselected product and verifies") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Query(0), Query(0), Query(1)};
    winactivate::ActivationWorkflow workflow(backend);
    const auto result = workflow.InstallAndActivate(kSyntheticKey, false);
    REQUIRE(backend.receivedNormalizedKey);
    REQUIRE(backend.lastActivatedPath == L"mock-product-path");
    REQUIRE(result.installReportedSuccess);
    REQUIRE(result.activationReportedSuccess);
    REQUIRE(result.verifiedActivated);
    REQUIRE(!result.error.has_value());
}

TEST_CASE("provider success without licensed reread is not verified") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Query(0), Query(0), Query(0)};
    winactivate::ActivationWorkflow workflow(backend);
    const auto result = workflow.InstallAndActivate(kSyntheticKey, false);
    REQUIRE(result.activationReportedSuccess);
    REQUIRE(!result.verifiedActivated);
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->stage == winactivate::ActivationStage::Verify);
}

TEST_CASE("already activated key replacement requires confirmation") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Query(1)};
    winactivate::ActivationWorkflow workflow(backend);
    const auto result = workflow.InstallAndActivate(kSyntheticKey, false);
    REQUIRE(!result.installAttempted);
    REQUIRE(backend.installCalls == 0);
    REQUIRE(result.error.has_value());
}

TEST_CASE("activate existing returns immediately when already activated") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Query(1)};
    winactivate::ActivationWorkflow workflow(backend);
    const auto result = workflow.ActivateExisting();
    REQUIRE(result.verifiedActivated);
    REQUIRE(!result.activationAttempted);
    REQUIRE(backend.activationCalls == 0);
}

TEST_CASE("activation failure still rereads and reports actual final status") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Query(0), Query(0)};
    backend.activationResult = {false, static_cast<std::int32_t>(0x80072F8F), {}};
    winactivate::ActivationWorkflow workflow(backend);
    const auto result = workflow.ActivateExisting();
    REQUIRE(result.activationAttempted);
    REQUIRE(!result.verifiedActivated);
    REQUIRE(backend.refreshCalls == 1);
    REQUIRE(backend.queryCalls == 2);
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->stage == winactivate::ActivationStage::Activate);
}

TEST_CASE("ambiguous product selection refuses mutation") {
    winactivate::MockLicensingBackend backend;
    auto ambiguous = Query(0);
    ambiguous.snapshot.selectionIsAmbiguous = true;
    ambiguous.snapshot.chosen.reset();
    backend.snapshots = {ambiguous};
    winactivate::ActivationWorkflow workflow(backend);
    const auto result = workflow.ActivateExisting();
    REQUIRE(!result.activationAttempted);
    REQUIRE(backend.activationCalls == 0);
    REQUIRE(result.error.has_value());
}

