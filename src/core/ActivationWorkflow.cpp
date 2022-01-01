#include "core/ActivationWorkflow.h"

#include "common/KeyValidation.h"
#include "common/SecureBuffer.h"

#include <utility>

namespace winactivate {
namespace {

constexpr wchar_t kHelpUrl[] =
    L"https://support.microsoft.com/windows/get-help-with-windows-activation-errors-09d8fb64-ab7c-4b75-8b1d-0d1efb40bfa7";

ActivationError SimpleError(ActivationStage stage, std::wstring message) {
    ActivationError error;
    error.stage = stage;
    error.message = std::move(message);
    error.remediationUrl = kHelpUrl;
    return error;
}

bool CanMutate(const LicenseSnapshot& snapshot) {
    return snapshot.os.isSupported && !snapshot.selectionIsAmbiguous;
}

}  // namespace

ActivationWorkflow::ActivationWorkflow(IActivationBackend& backend,
                                       ProgressCallback progress)
    : backend_(backend), progress_(std::move(progress)) {}

ActivationResult ActivationWorkflow::InstallAndActivate(
    std::wstring_view submittedKey, bool replaceActivatedConfirmed) {
    ActivationResult result;
    Report(ActivationStage::Validate);
    KeyValidationResult validation = ValidateAndNormalizeProductKey(submittedKey);
    if (!validation.valid) {
        result.error = SimpleError(ActivationStage::Validate, validation.error);
        return result;
    }
    SecureWideString key(std::move(validation.normalized));

    Report(ActivationStage::Query);
    SnapshotQueryResult initial = backend_.QuerySnapshot();
    result.finalSnapshot = initial.snapshot;
    if (initial.error.has_value()) {
        key.clear();
        result.error = std::move(initial.error);
        return result;
    }
    if (!CanMutate(initial.snapshot)) {
        key.clear();
        result.error = SimpleError(
            ActivationStage::Verify,
            initial.snapshot.selectionIsAmbiguous
                ? L"The applicable Windows product is ambiguous; no key was installed."
                : L"This Windows installation is not supported.");
        return result;
    }
    if (initial.snapshot.status == LicenseSummaryStatus::Activated &&
        !replaceActivatedConfirmed) {
        key.clear();
        result.error = SimpleError(
            ActivationStage::Validate,
            L"Replacing a currently activated product key requires explicit confirmation.");
        return result;
    }

    Report(ActivationStage::Install);
    result.installAttempted = true;
    const BackendCallResult install = backend_.InstallProductKey(key.get());
    key.clear();
    result.installReportedSuccess = install.succeeded;
    if (!install.succeeded) {
        return FinishWithRefresh(
            std::move(result),
            ErrorFromCall(ActivationStage::Install, install,
                          L"Windows did not accept the product key."));
    }

    Report(ActivationStage::Refresh);
    static_cast<void>(backend_.RefreshLicenseStatus());
    SnapshotQueryResult afterInstall = backend_.QuerySnapshot();
    result.finalSnapshot = afterInstall.snapshot;
    if (afterInstall.error.has_value()) {
        result.error = std::move(afterInstall.error);
        return result;
    }
    if (!afterInstall.snapshot.chosen.has_value() ||
        afterInstall.snapshot.selectionIsAmbiguous) {
        return FinishWithRefresh(
            std::move(result),
            SimpleError(ActivationStage::Activate,
                        L"The installed key could not be matched safely to a Windows product."));
    }

    Report(ActivationStage::Activate);
    result.activationAttempted = true;
    const BackendCallResult activation = backend_.ActivateProduct(
        afterInstall.snapshot.chosen->wmiObjectPath);
    result.activationReportedSuccess = activation.succeeded;
    std::optional<ActivationError> error;
    if (!activation.succeeded) {
        error = ErrorFromCall(ActivationStage::Activate, activation,
                              L"Windows activation reported an error.");
    }
    return FinishWithRefresh(std::move(result), std::move(error));
}

ActivationResult ActivationWorkflow::ActivateExisting() {
    ActivationResult result;
    Report(ActivationStage::Query);
    SnapshotQueryResult initial = backend_.QuerySnapshot();
    result.finalSnapshot = initial.snapshot;
    if (initial.error.has_value()) {
        result.error = std::move(initial.error);
        return result;
    }
    if (initial.snapshot.status == LicenseSummaryStatus::Activated) {
        result.verifiedActivated = true;
        return result;
    }
    if (!CanMutate(initial.snapshot) || !initial.snapshot.chosen.has_value()) {
        result.error = SimpleError(
            ActivationStage::Activate,
            L"No unambiguous installed Windows license is available to activate.");
        return result;
    }

    Report(ActivationStage::Activate);
    result.activationAttempted = true;
    const BackendCallResult activation =
        backend_.ActivateProduct(initial.snapshot.chosen->wmiObjectPath);
    result.activationReportedSuccess = activation.succeeded;
    std::optional<ActivationError> error;
    if (!activation.succeeded) {
        error = ErrorFromCall(ActivationStage::Activate, activation,
                              L"Windows activation reported an error.");
    }
    return FinishWithRefresh(std::move(result), std::move(error));
}

ActivationResult ActivationWorkflow::FinishWithRefresh(
    ActivationResult result, std::optional<ActivationError> operationError) {
    Report(ActivationStage::Refresh);
    const BackendCallResult refresh = backend_.RefreshLicenseStatus();
    if (!refresh.succeeded && !operationError.has_value()) {
        operationError = ErrorFromCall(ActivationStage::Refresh, refresh,
                                       L"Windows license status could not be refreshed.");
    }

    Report(ActivationStage::Verify);
    SnapshotQueryResult final = backend_.QuerySnapshot();
    result.finalSnapshot = std::move(final.snapshot);
    result.verifiedActivated = !final.error.has_value() &&
                              result.finalSnapshot.chosen.has_value() &&
                              result.finalSnapshot.status == LicenseSummaryStatus::Activated &&
                              result.finalSnapshot.chosen->licenseStatus == 1;
    if (final.error.has_value()) {
        result.error = std::move(final.error);
    } else if (operationError.has_value()) {
        result.error = std::move(operationError);
    } else if (!result.verifiedActivated) {
        result.error = SimpleError(
            ActivationStage::Verify,
            L"The operation completed, but activation was not confirmed by a fresh license snapshot.");
    }
    return result;
}

ActivationError ActivationWorkflow::ErrorFromCall(
    ActivationStage stage, const BackendCallResult& call, std::wstring message) const {
    ActivationError error;
    error.stage = stage;
    error.hresult = call.hresult;
    error.wmiReturnValue = call.wmiReturnValue;
    error.message = std::move(message);
    error.remediationUrl = kHelpUrl;
    return error;
}

void ActivationWorkflow::Report(ActivationStage stage) const {
    if (progress_) {
        progress_(stage);
    }
}

}  // namespace winactivate

