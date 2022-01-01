#include "gui/MainWindow.h"

#include "common/FormatError.h"
#include "core/LicenseReader.h"
#include "core/ProductSelection.h"
#include "gui/resource.h"

#include <Windows.h>
#include <shellapi.h>

#include <array>
#include <memory>
#include <sstream>
#include <string>

namespace winactivate {
namespace {

constexpr wchar_t kWindowClass[] = L"WinActivateManager.MainWindow";
constexpr wchar_t kWindowTitle[] = L"WinActivate Manager";
constexpr UINT kStatusReady = WM_APP + 1;

HWND AddControl(HWND parent, DWORD exStyle, const wchar_t* className,
                const wchar_t* text, DWORD style, int id) {
    return CreateWindowExW(exStyle, className, text, style | WS_CHILD | WS_VISIBLE,
                           0, 0, 0, 0, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           GetModuleHandleW(nullptr), nullptr);
}

}  // namespace

MainWindow::MainWindow(HINSTANCE instance) noexcept : instance_(instance) {}

MainWindow::~MainWindow() {
    closing_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }
    if (font_ != nullptr) {
        DeleteObject(font_);
    }
}

bool MainWindow::Create(int showCommand) {
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClass;
    windowClass.hIconSm = windowClass.hIcon;
    if (RegisterClassExW(&windowClass) == 0) {
        return false;
    }

    window_ = CreateWindowExW(
        0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, instance_, this);
    if (window_ == nullptr) {
        return false;
    }

    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
    return true;
}

int MainWindow::RunMessageLoop() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window_, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK MainWindow::WindowProcedure(HWND window, UINT message,
                                              WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
    }
    return self != nullptr ? self->HandleMessage(message, wParam, lParam)
                           : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            CreateControls();
            PostMessageW(window_, WM_COMMAND, MAKEWPARAM(IDC_REFRESH, BN_CLICKED),
                         reinterpret_cast<LPARAM>(GetDlgItem(window_, IDC_REFRESH)));
            return 0;

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize = {700, 480};
            return 0;
        }

        case WM_SIZE:
            LayoutControls(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_SHOW_KEY: {
                    const HWND edit = GetDlgItem(window_, IDC_PRODUCT_KEY);
                    const bool checked =
                        SendMessageW(GetDlgItem(window_, IDC_SHOW_KEY), BM_GETCHECK,
                                     0, 0) == BST_CHECKED;
                    SendMessageW(edit, EM_SETPASSWORDCHAR, checked ? 0 : 0x25CF, 0);
                    InvalidateRect(edit, nullptr, TRUE);
                    return 0;
                }
                case IDC_OPEN_SETTINGS:
                    OpenActivationSettings();
                    return 0;
                case IDC_ABOUT:
                    MessageBoxW(window_,
                                L"WinActivate Manager\n\nA local-only manager for genuine "
                                L"Windows licenses. It does not generate keys or bypass "
                                L"Microsoft activation.",
                                kWindowTitle, MB_OK | MB_ICONINFORMATION);
                    return 0;
                case IDC_REFRESH:
                    StartRefresh();
                    return 0;
                case IDC_ACTIVATE_EXISTING:
                case IDC_INSTALL_ACTIVATE:
                    MessageBoxW(window_,
                                L"Activation changes are introduced after the read-only status phase.",
                                kWindowTitle, MB_OK | MB_ICONINFORMATION);
                    return 0;
                case IDC_DIAGNOSTICS:
                    ShowDiagnostics();
                    return 0;
                default:
                    break;
            }
            break;

        case kStatusReady: {
            std::unique_ptr<SnapshotQueryResult> result(
                reinterpret_cast<SnapshotQueryResult*>(lParam));
            FinishRefresh(std::move(*result));
            return 0;
        }

        case WM_DESTROY:
            closing_.store(true);
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

void MainWindow::CreateControls() {
    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics,
                              0)) {
        font_ = CreateFontIndirectW(&metrics.lfMessageFont);
    }

    const DWORD labelStyle = WS_TABSTOP | SS_LEFT;
    const std::array controls{
        AddControl(window_, 0, L"STATIC", L"Windows:", labelStyle, -1),
        AddControl(window_, 0, L"STATIC", L"Not checked", labelStyle,
                   IDC_WINDOWS_VALUE),
        AddControl(window_, 0, L"STATIC", L"OS build:", labelStyle, -1),
        AddControl(window_, 0, L"STATIC", L"Unknown", labelStyle,
                   IDC_BUILD_VALUE),
        AddControl(window_, 0, L"STATIC", L"License:", labelStyle, -1),
        AddControl(window_, 0, L"STATIC", L"Unknown", labelStyle,
                   IDC_LICENSE_VALUE),
        AddControl(window_, 0, L"STATIC", L"Partial key:", labelStyle, -1),
        AddControl(window_, 0, L"STATIC", L"Not available", labelStyle,
                   IDC_PARTIAL_KEY_VALUE),
        AddControl(window_, 0, L"STATIC", L"Reported channel:", labelStyle, -1),
        AddControl(window_, 0, L"STATIC", L"Unknown", labelStyle,
                   IDC_CHANNEL_VALUE),
        AddControl(window_, 0, L"BUTTON", L"Refresh status", WS_TABSTOP | BS_PUSHBUTTON,
                   IDC_REFRESH),
        AddControl(window_, 0, L"BUTTON", L"Activate existing license",
                   WS_TABSTOP | BS_PUSHBUTTON, IDC_ACTIVATE_EXISTING),
        AddControl(window_, 0, L"STATIC", L"Enter a product key you already own:",
                   labelStyle, -1),
        AddControl(window_, WS_EX_CLIENTEDGE, L"EDIT", L"",
                   WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, IDC_PRODUCT_KEY),
        AddControl(window_, 0, L"BUTTON", L"Show", WS_TABSTOP | BS_AUTOCHECKBOX,
                   IDC_SHOW_KEY),
        AddControl(window_, 0, L"BUTTON", L"Install key && activate",
                   WS_TABSTOP | BS_DEFPUSHBUTTON, IDC_INSTALL_ACTIVATE),
        AddControl(window_, 0, L"STATIC",
                   L"Ready. Activation changes require administrator approval.",
                   labelStyle, IDC_PROGRESS),
        AddControl(window_, 0, L"BUTTON", L"View diagnostics",
                   WS_TABSTOP | BS_PUSHBUTTON, IDC_DIAGNOSTICS),
        AddControl(window_, 0, L"BUTTON", L"Open Windows Activation Settings",
                   WS_TABSTOP | BS_PUSHBUTTON, IDC_OPEN_SETTINGS),
        AddControl(window_, 0, L"STATIC",
                   L"No key is stored or uploaded by this application.",
                   labelStyle, IDC_PRIVACY_NOTE),
        AddControl(window_, 0, L"BUTTON", L"About", WS_TABSTOP | BS_PUSHBUTTON,
                   IDC_ABOUT),
    };

    for (const HWND control : controls) {
        ApplyFont(control);
    }
    SendMessageW(GetDlgItem(window_, IDC_PRODUCT_KEY), EM_SETLIMITTEXT, 64, 0);
    EnableWindow(GetDlgItem(window_, IDC_ACTIVATE_EXISTING), FALSE);
    EnableWindow(GetDlgItem(window_, IDC_INSTALL_ACTIVATE), FALSE);
}

void MainWindow::LayoutControls(int width, int height) {
    const int margin = 24;
    const int rowHeight = 24;
    const int labelWidth = 145;
    const int valueLeft = margin + labelWidth;
    const int valueWidth = width - valueLeft - margin;

    const std::array<int, 5> valueIds{
        IDC_WINDOWS_VALUE, IDC_BUILD_VALUE, IDC_LICENSE_VALUE,
        IDC_PARTIAL_KEY_VALUE, IDC_CHANNEL_VALUE};
    int y = 24;
    for (std::size_t index = 0; index < valueIds.size(); ++index) {
        const auto labels = std::array{L"Windows:", L"OS build:", L"License:",
                                       L"Partial key:", L"Reported channel:"};
        HWND label = FindWindowExW(window_, nullptr, L"STATIC", labels[index]);
        MoveWindow(label, margin, y, labelWidth, rowHeight, TRUE);
        MoveWindow(GetDlgItem(window_, valueIds[index]), valueLeft, y, valueWidth,
                   rowHeight, TRUE);
        y += 30;
    }

    MoveWindow(GetDlgItem(window_, IDC_ABOUT), width - margin - 80, 18, 80, 28,
               TRUE);
    MoveWindow(GetDlgItem(window_, IDC_REFRESH), margin, y + 8, 130, 32, TRUE);
    MoveWindow(GetDlgItem(window_, IDC_ACTIVATE_EXISTING), margin + 142, y + 8, 205,
               32, TRUE);
    y += 62;

    HWND prompt = FindWindowExW(window_, nullptr, L"STATIC",
                                L"Enter a product key you already own:");
    MoveWindow(prompt, margin, y, width - 2 * margin, rowHeight, TRUE);
    y += 28;

    MoveWindow(GetDlgItem(window_, IDC_PRODUCT_KEY), margin, y,
               width - 2 * margin - 94, 30, TRUE);
    MoveWindow(GetDlgItem(window_, IDC_SHOW_KEY), width - margin - 82, y, 82, 30,
               TRUE);
    y += 42;
    MoveWindow(GetDlgItem(window_, IDC_INSTALL_ACTIVATE), margin, y, 190, 34, TRUE);
    y += 54;
    MoveWindow(GetDlgItem(window_, IDC_PROGRESS), margin, y, width - 2 * margin,
               48, TRUE);

    const int bottom = height - margin;
    MoveWindow(GetDlgItem(window_, IDC_PRIVACY_NOTE), margin, bottom - 28,
               width - 2 * margin, rowHeight, TRUE);
    MoveWindow(GetDlgItem(window_, IDC_DIAGNOSTICS), margin, bottom - 74, 150, 32,
               TRUE);
    MoveWindow(GetDlgItem(window_, IDC_OPEN_SETTINGS), margin + 162, bottom - 74,
               255, 32, TRUE);
}

void MainWindow::ApplyFont(HWND control) const {
    if (control != nullptr && font_ != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
}

void MainWindow::OpenActivationSettings() {
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
        window_, L"open", L"ms-settings:activation", nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        MessageBoxW(window_,
                    L"Windows Activation Settings could not be opened. Open Settings, "
                    L"then go to System > Activation.",
                    kWindowTitle, MB_OK | MB_ICONWARNING);
    }
}

void MainWindow::StartRefresh() {
    if (busy_ || closing_.load()) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    SetBusy(true);
    SetWindowTextW(GetDlgItem(window_, IDC_PROGRESS), L"Checking system and license status…");

    worker_ = std::thread([this] {
        WmiLicensingBackend backend;
        auto result = std::make_unique<SnapshotQueryResult>(backend.QuerySnapshot());
        if (closing_.load()) {
            return;
        }
        if (PostMessageW(window_, kStatusReady, 0,
                         reinterpret_cast<LPARAM>(result.get()))) {
            result.release();
        }
    });
}

void MainWindow::FinishRefresh(SnapshotQueryResult result) {
    if (worker_.joinable()) {
        worker_.join();
    }
    SetBusy(false);
    lastSnapshot_ = result.snapshot;
    lastError_ = result.error;

    const OsInfo& os = result.snapshot.os;
    std::wstring windowsText = L"Unsupported or unknown";
    if (os.isSupported) {
        windowsText = os.family == WindowsFamily::Win11 ? L"Windows 11 " : L"Windows 10 ";
        windowsText += os.edition;
    } else if (!os.caption.empty()) {
        windowsText = os.caption;
    }
    SetWindowTextW(GetDlgItem(window_, IDC_WINDOWS_VALUE), windowsText.c_str());
    const std::wstring buildText = os.build == 0 ? L"Unknown" : std::to_wstring(os.build);
    SetWindowTextW(GetDlgItem(window_, IDC_BUILD_VALUE), buildText.c_str());

    if (result.error.has_value()) {
        SetWindowTextW(GetDlgItem(window_, IDC_LICENSE_VALUE), L"Unknown (query failed)");
        SetWindowTextW(GetDlgItem(window_, IDC_PARTIAL_KEY_VALUE), L"Not available");
        SetWindowTextW(GetDlgItem(window_, IDC_CHANNEL_VALUE), L"Unknown");
        const std::wstring errorText = DescribeActivationError(*result.error);
        SetWindowTextW(GetDlgItem(window_, IDC_PROGRESS), errorText.c_str());
        return;
    }

    const wchar_t* statusText = L"Unknown";
    switch (result.snapshot.status) {
        case LicenseSummaryStatus::Activated:
            statusText = L"Activated";
            break;
        case LicenseSummaryStatus::NotActivated:
            statusText = L"Not activated";
            break;
        case LicenseSummaryStatus::Grace:
            statusText = L"Grace period";
            break;
        case LicenseSummaryStatus::Notification:
            statusText = L"Notification";
            break;
        case LicenseSummaryStatus::Unknown:
            break;
    }
    SetWindowTextW(GetDlgItem(window_, IDC_LICENSE_VALUE), statusText);

    std::wstring partial = L"Not available";
    std::wstring channel = L"Unknown";
    if (result.snapshot.chosen.has_value()) {
        if (result.snapshot.chosen->partialKey.has_value()) {
            partial = L"*****-" + *result.snapshot.chosen->partialKey;
        }
        channel = ReportedChannel(*result.snapshot.chosen);
    }
    SetWindowTextW(GetDlgItem(window_, IDC_PARTIAL_KEY_VALUE), partial.c_str());
    SetWindowTextW(GetDlgItem(window_, IDC_CHANNEL_VALUE), channel.c_str());
    SetWindowTextW(GetDlgItem(window_, IDC_PROGRESS), result.snapshot.reason.c_str());
}

void MainWindow::SetBusy(bool busy) {
    busy_ = busy;
    EnableWindow(GetDlgItem(window_, IDC_REFRESH), busy ? FALSE : TRUE);
}

void MainWindow::ShowDiagnostics() {
    if (!lastSnapshot_.has_value()) {
        MessageBoxW(window_, L"Status has not been read yet.", kWindowTitle,
                    MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wostringstream text;
    text << L"OS version: "
         << (lastSnapshot_->os.version.empty() ? L"Unknown" : lastSnapshot_->os.version)
         << L"\nBuild: " << lastSnapshot_->os.build
         << L"\nEdition: " << lastSnapshot_->os.edition
         << L"\nWorkstation: " << (lastSnapshot_->os.isWorkstation ? L"Yes" : L"No")
         << L"\nSupported: " << (lastSnapshot_->os.isSupported ? L"Yes" : L"No")
         << L"\nLicensing records: " << lastSnapshot_->records.size()
         << L"\nSelection ambiguous: "
         << (lastSnapshot_->selectionIsAmbiguous ? L"Yes" : L"No")
         << L"\n\n" << lastSnapshot_->reason;
    if (lastSnapshot_->chosen.has_value()) {
        text << L"\n\nSelected product: " << lastSnapshot_->chosen->name
             << L"\nDescription: " << lastSnapshot_->chosen->description
             << L"\nLicense status: " << lastSnapshot_->chosen->licenseStatus
             << L"\nStatus reason: "
             << FormatErrorCode(lastSnapshot_->chosen->licenseStatusReason);
    }
    if (lastError_.has_value()) {
        text << L"\n\n" << DescribeActivationError(*lastError_);
    }
    MessageBoxW(window_, text.str().c_str(), L"WinActivate diagnostics",
                MB_OK | (lastError_.has_value() ? MB_ICONWARNING : MB_ICONINFORMATION));
}

}  // namespace winactivate

