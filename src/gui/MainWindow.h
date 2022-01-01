#pragma once

#include "common/LicensingBackend.h"

#include <Windows.h>

#include <atomic>
#include <optional>
#include <thread>

namespace winactivate {

class MainWindow final {
public:
    explicit MainWindow(HINSTANCE instance) noexcept;
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    bool Create(int showCommand);
    int RunMessageLoop();

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                             WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void LayoutControls(int width, int height);
    void ApplyFont(HWND control) const;
    void OpenActivationSettings();
    void StartRefresh();
    void FinishRefresh(SnapshotQueryResult result);
    void SetBusy(bool busy);
    void ShowDiagnostics();

    HINSTANCE instance_{};
    HWND window_{};
    HFONT font_{};
    std::thread worker_;
    std::atomic_bool closing_{};
    bool busy_{};
    std::optional<LicenseSnapshot> lastSnapshot_;
    std::optional<ActivationError> lastError_;
};

}  // namespace winactivate

