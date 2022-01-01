#include "gui/MainWindow.h"

#include "core/WmiConnection.h"

#include <Windows.h>
#include <commctrl.h>

namespace {

void EnableBestAvailableDpiAwareness() noexcept {
    using SetDpiContext = BOOL(WINAPI*)(HANDLE);
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const auto setDpiContext = reinterpret_cast<SetDpiContext>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setDpiContext != nullptr &&
        setDpiContext(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4)))) {
        return;
    }
    SetProcessDPIAware();
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    EnableBestAvailableDpiAwareness();

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(comResult);
    if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
        MessageBoxW(nullptr, L"Windows COM initialization failed.",
                    L"WinActivate Manager", MB_OK | MB_ICONERROR);
        return 1;
    }
    const HRESULT securityResult = winactivate::InitializeProcessComSecurity();
    if (FAILED(securityResult)) {
        if (comInitialized) {
            CoUninitialize();
        }
        MessageBoxW(nullptr, L"Windows COM security initialization failed.",
                    L"WinActivate Manager", MB_OK | MB_ICONERROR);
        return 1;
    }

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    winactivate::MainWindow window(instance);
    if (!window.Create(showCommand)) {
        MessageBoxW(nullptr, L"WinActivate Manager could not create its main window.",
                    L"WinActivate Manager", MB_OK | MB_ICONERROR);
        if (comInitialized) {
            CoUninitialize();
        }
        return 1;
    }
    const int exitCode = window.RunMessageLoop();
    if (comInitialized) {
        CoUninitialize();
    }
    return exitCode;
}

