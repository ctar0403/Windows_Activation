#pragma once

#include "common/LicensingBackend.h"

#include <functional>
#include <string_view>

namespace winactivate {

using ProgressCallback = std::function<void(ActivationStage)>;

class ActivationWorkflow final {
public:
    explicit ActivationWorkflow(IActivationBackend& backend,
                                ProgressCallback progress = {});

    [[nodiscard]] ActivationResult InstallAndActivate(
        std::wstring_view submittedKey, bool replaceActivatedConfirmed);
    [[nodiscard]] ActivationResult ActivateExisting();

private:
    [[nodiscard]] ActivationResult FinishWithRefresh(
        ActivationResult result, std::optional<ActivationError> operationError);
    [[nodiscard]] ActivationError ErrorFromCall(ActivationStage stage,
                                                const BackendCallResult& call,
                                                std::wstring message) const;
    void Report(ActivationStage stage) const;

    IActivationBackend& backend_;
    ProgressCallback progress_;
};

}  // namespace winactivate

