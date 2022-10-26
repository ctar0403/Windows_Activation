#include "gui/MainWindow.h"

#include "common/FormatError.h"
#include "core/LicenseReader.h"
#include "core/ProductSelection.h"
#include "gui/resource.h"

#include <Windows.h>
#include <commctrl.h>
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
    if (titleFont_ != nullptr) {
        DeleteObject(titleFont_);
    }
    if (statusFont_ != nullptr) {
        DeleteObject(statusFont_);
    }
}

bool MainWindow::Create(int showCommand) {
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    windowClass.lpszClassName = kWindowClass;
    windowClass.hIconSm = windowClass.hIcon;
    if (RegisterClassExW(&windowClass) == 0) {
        return false;
    }

    window_ = CreateWindowExW(
        0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 820, 660, nullptr, nullptr, instance_, this);
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
            info->ptMinTrackSize = {720, 620};
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

        case WM_CTLCOLORSTATIC: {
            const HDC device = reinterpret_cast<HDC>(wParam);
            const HWND control = reinterpret_cast<HWND>(lParam);
            SetBkMode(device, TRANSPARENT);
            if (control == GetDlgItem(window_, IDC_LICENSE_VALUE)) {
                SetTextColor(device, licenseColor_);
            } else {
                SetTextColor(device, GetSysColor(COLOR_WINDOWTEXT));
            }
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
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

        LOGFONTW title = metrics.lfMessageFont;
        title.lfHeight = -24;
        title.lfWeight = FW_SEMIBOLD;
        titleFont_ = CreateFontIndirectW(&title);

        LOGFONTW status = metrics.lfMessageFont;
        status.lfHeight = -18;
        status.lfWeight = FW_SEMIBOLD;
        statusFont_ = CreateFontIndirectW(&status);
    }

    const DWORD labelStyle = SS_LEFT;
    const std::array controls{
        AddControl(window_, 0, L"STATIC", L"Windows activation", labelStyle,
                   IDC_HEADER),
        AddControl(window_, 0, L"STATIC",
                   L"Check this PC's genuine Windows license and open official activation tools.",
                   labelStyle, IDC_SUBTITLE),
        AddControl(window_, 0, L"BUTTON", L"License status", BS_GROUPBOX,
                   IDC_STATUS_GROUP),
        AddControl(window_, 0, L"STATIC", L"Windows", labelStyle,
                   IDC_WINDOWS_LABEL),
        AddControl(window_, 0, L"STATIC", L"Not checked", labelStyle,
                   IDC_WINDOWS_VALUE),
        AddControl(window_, 0, L"STATIC", L"OS build", labelStyle,
                   IDC_BUILD_LABEL),
        AddControl(window_, 0, L"STATIC", L"Unknown", labelStyle,
                   IDC_BUILD_VALUE),
        AddControl(window_, 0, L"STATIC", L"License", labelStyle,
                   IDC_LICENSE_LABEL),
        AddControl(window_, 0, L"STATIC", L"Unknown", labelStyle,
                   IDC_LICENSE_VALUE),
        AddControl(window_, 0, L"STATIC", L"Partial key", labelStyle,
                   IDC_PARTIAL_KEY_LABEL),
        AddControl(window_, 0, L"STATIC", L"Not available", labelStyle,
                   IDC_PARTIAL_KEY_VALUE),
        AddControl(window_, 0, L"STATIC", L"Reported channel", labelStyle,
                   IDC_CHANNEL_LABEL),
        AddControl(window_, 0, L"STATIC", L"Unknown", labelStyle,
                   IDC_CHANNEL_VALUE),
        AddControl(window_, 0, L"BUTTON", L"Refresh status", WS_TABSTOP | BS_PUSHBUTTON,
                   IDC_REFRESH),
        AddControl(window_, 0, L"BUTTON", L"Activate existing license",
                   WS_TABSTOP | BS_PUSHBUTTON, IDC_ACTIVATE_EXISTING),
        AddControl(window_, 0, L"BUTTON", L"Product key", BS_GROUPBOX,
                   IDC_KEY_GROUP),
        AddControl(window_, 0, L"STATIC", L"Enter a product key you already own",
                   labelStyle, IDC_KEY_PROMPT),
        AddControl(window_, WS_EX_CLIENTEDGE, L"EDIT", L"",
                   WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, IDC_PRODUCT_KEY),
        AddControl(window_, 0, L"BUTTON", L"Show", WS_TABSTOP | BS_AUTOCHECKBOX,
                   IDC_SHOW_KEY),
        AddControl(window_, 0, L"BUTTON", L"Install key && activate",
                   WS_TABSTOP | BS_DEFPUSHBUTTON, IDC_INSTALL_ACTIVATE),
        AddControl(window_, 0, L"STATIC",
                   L"Use a genuine 25-character key that matches this Windows edition.",
                   labelStyle, IDC_KEY_HINT),
        AddControl(window_, 0, L"STATIC",
                   L"Preview build: activation actions are disabled until the secure elevated helper is complete.",
                   labelStyle, IDC_PREVIEW_NOTE),
        AddControl(window_, 0, L"BUTTON", L"Activity", BS_GROUPBOX,
                   IDC_ACTIVITY_GROUP),
        AddControl(window_, 0, L"STATIC",
                   L"Checking Windows status…",
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
    if (titleFont_ != nullptr) {
        SendMessageW(GetDlgItem(window_, IDC_HEADER), WM_SETFONT,
                     reinterpret_cast<WPARAM>(titleFont_), TRUE);
    }
    if (statusFont_ != nullptr) {
        SendMessageW(GetDlgItem(window_, IDC_LICENSE_VALUE), WM_SETFONT,
                     reinterpret_cast<WPARAM>(statusFont_), TRUE);
    }
    SendMessageW(GetDlgItem(window_, IDC_PRODUCT_KEY), EM_SETLIMITTEXT, 64, 0);
    SendMessageW(GetDlgItem(window_, IDC_PRODUCT_KEY), EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(L"XXXXX-XXXXX-XXXXX-XXXXX-XXXXX"));
    EnableWindow(GetDlgItem(window_, IDC_ACTIVATE_EXISTING), FALSE);
    EnableWindow(GetDlgItem(window_, IDC_INSTALL_ACTIVATE), FALSE);
}

void MainWindow::LayoutControls(int width, int height) {
    const int margin = 24;
    const int contentWidth = width - 2 * margin;
    const int labelWidth = 138;
    const int valueLeft = margin + 24 + labelWidth;
    const int valueWidth = width - valueLeft - margin - 24;

    MoveWindow(GetDlgItem(window_, IDC_HEADER), margin, 18, contentWidth - 90, 32,
               TRUE);
    MoveWindow(GetDlgItem(window_, IDC_SUBTITLE), margin, 52, contentWidth, 24,
               TRUE);
    MoveWindow(GetDlgItem(window_, IDC_ABOUT), width - margin - 76, 20, 76, 28,
               TRUE);

    const int statusTop = 88;
    MoveWindow(GetDlgItem(window_, IDC_STATUS_GROUP), margin, statusTop,
               contentWidth, 196, TRUE);
    const std::array<int, 5> labelIds{IDC_WINDOWS_LABEL, IDC_BUILD_LABEL,
                                      IDC_LICENSE_LABEL, IDC_PARTIAL_KEY_LABEL,
                                      IDC_CHANNEL_LABEL};
    const std::array<int, 5> valueIds{IDC_WINDOWS_VALUE, IDC_BUILD_VALUE,
                                      IDC_LICENSE_VALUE, IDC_PARTIAL_KEY_VALUE,
                                      IDC_CHANNEL_VALUE};
    int row = statusTop + 28;
    for (std::size_t index = 0; index < valueIds.size(); ++index) {
        MoveWindow(GetDlgItem(window_, labelIds[index]), margin + 24, row,
                   labelWidth, 24, TRUE);
        MoveWindow(GetDlgItem(window_, valueIds[index]), valueLeft, row,
                   valueWidth, 24, TRUE);
        row += 28;
    }
    MoveWindow(GetDlgItem(window_, IDC_REFRESH), margin + 24, statusTop + 154,
               132, 30, TRUE);
    MoveWindow(GetDlgItem(window_, IDC_ACTIVATE_EXISTING), margin + 168,
               statusTop + 154, 202, 30, TRUE);

    const int keyTop = statusTop + 208;
    MoveWindow(GetDlgItem(window_, IDC_KEY_GROUP), margin, keyTop, contentWidth,
               150, TRUE);
    MoveWindow(GetDlgItem(window_, IDC_KEY_PROMPT), margin + 24, keyTop + 26,
               contentWidth - 48, 22, TRUE);
    MoveWindow(GetDlgItem(window_, IDC_PRODUCT_KEY), margin + 24, keyTop + 52,
               contentWidth - 48 - 88, 30, TRUE);
    MoveWindow(GetDlgItem(window_, IDC_SHOW_KEY), width - margin - 24 - 78,
               keyTop + 52, 78, 30, TRUE);
    MoveWindow(GetDlgItem(window_, IDC_KEY_HINT), margin + 24, keyTop + 88,
               contentWidth - 260, 22, TRUE);
    MoveWindow(GetDlgItem(window_, IDC_INSTALL_ACTIVATE), width - margin - 24 - 196,
               keyTop + 86, 196, 32, TRUE);
    MoveWindow(GetDlgItem(window_, IDC_PREVIEW_NOTE), margin + 24, keyTop + 118,
               contentWidth - 48, 22, TRUE);

    const int activityTop = keyTop + 164;
    const int footerTop = height - 58;
    MoveWindow(GetDlgItem(window_, IDC_ACTIVITY_GROUP), margin, activityTop,
               contentWidth, (std::max)(58, footerTop - activityTop - 10), TRUE);
    MoveWindow(GetDlgItem(window_, IDC_PROGRESS), margin + 24, activityTop + 26,
               contentWidth - 48, (std::max)(26, footerTop - activityTop - 38),
               TRUE);

    MoveWindow(GetDlgItem(window_, IDC_DIAGNOSTICS), margin, footerTop, 142, 30,
               TRUE);
    MoveWindow(GetDlgItem(window_, IDC_OPEN_SETTINGS), margin + 154, footerTop,
               246, 30, TRUE);
    MoveWindow(GetDlgItem(window_, IDC_PRIVACY_NOTE), margin, footerTop + 36,
               contentWidth, 22, TRUE);
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
    licenseColor_ = RGB(80, 80, 80);
    InvalidateRect(GetDlgItem(window_, IDC_LICENSE_VALUE), nullptr, TRUE);
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
        licenseColor_ = RGB(176, 32, 37);
        SetWindowTextW(GetDlgItem(window_, IDC_LICENSE_VALUE), L"Unknown (query failed)");
        SetWindowTextW(GetDlgItem(window_, IDC_PARTIAL_KEY_VALUE), L"Not available");
        SetWindowTextW(GetDlgItem(window_, IDC_CHANNEL_VALUE), L"Unknown");
        const std::wstring errorText = DescribeActivationError(*result.error);
        SetWindowTextW(GetDlgItem(window_, IDC_PROGRESS), errorText.c_str());
        InvalidateRect(GetDlgItem(window_, IDC_LICENSE_VALUE), nullptr, TRUE);
        return;
    }

    const wchar_t* statusText = L"Unknown";
    switch (result.snapshot.status) {
        case LicenseSummaryStatus::Activated:
            statusText = L"Activated";
            licenseColor_ = RGB(16, 124, 65);
            break;
        case LicenseSummaryStatus::NotActivated:
            statusText = L"Not activated";
            licenseColor_ = RGB(176, 32, 37);
            break;
        case LicenseSummaryStatus::Grace:
            statusText = L"Grace period";
            licenseColor_ = RGB(156, 87, 0);
            break;
        case LicenseSummaryStatus::Notification:
            statusText = L"Notification";
            licenseColor_ = RGB(156, 87, 0);
            break;
        case LicenseSummaryStatus::Unknown:
            licenseColor_ = RGB(80, 80, 80);
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
    InvalidateRect(GetDlgItem(window_, IDC_LICENSE_VALUE), nullptr, TRUE);
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

