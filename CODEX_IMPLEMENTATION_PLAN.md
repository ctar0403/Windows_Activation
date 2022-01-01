# WinActivate Manager - Complete Codex Implementation Plan

Version: 1.0 | Scope: Windows 10 / Windows 11 client, x64 | Language: C++20

## 1. Goal and hard boundaries

Create a shippable, local-only native Windows desktop app, `WinActivate Manager`, to:

1. Detect the current Windows 10/11 edition and operating-system build.
2. Read Windows licensing state and explain it clearly.
3. Install an **existing, genuine, user-provided** 25-character product key through Microsoft licensing APIs.
4. Request online activation using a valid installed key or eligible existing digital entitlement.
5. Show an accurate result after rereading licensing state, plus actionable diagnostics and an official activation-settings link.

**Do not** generate or guess keys; bypass Microsoft activation; tamper with `sppsvc`, licensing files, or the registry; inject tickets or certificates; configure public/untrusted KMS servers; add activation cracks, rearm loops, or key harvesting; or claim successful activation solely because a method returned success.

Windows 10 Home/Pro mainstream support ended October 14, 2025. Keep Windows 10 functionality, but note its servicing situation in documentation; extended support varies by edition/program.

This is an ordinary C++ development task. Implement actual buildable source code, not pseudocode.

## 2. Implementation stack

- C++20 with MSVC; target Windows x64 only.
- Visual Studio 2022 or newer with Desktop development with C++ and Windows 10/11 SDK.
- CMake 3.24+; Visual Studio generator; CTest.
- Win32 API and common controls (`comctl32`) for the GUI, Unicode throughout, high-DPI-aware, keyboard accessible.
- Native COM/WMI (`IWbemLocator`, `IWbemServices` in `wbemidl.h` / `wbemuuid.lib`) for licensing.
- Windows SDK `Microsoft::WRL::ComPtr` for COM pointer lifetime, RAII wrappers for HANDLE and VARIANT/BSTR where appropriate; no mandatory third-party runtime.
- Two binaries: `WinActivate.exe` (asInvoker GUI) and `WinActivateHelper.exe` (requireAdministrator, one-shot helper).
- Use native UI styles with consistent spacing; do not let visual polish delay correct activation flows.
- No external servers, telemetry, account login, online key validation service, or app backend. Windows itself communicates with Microsoft's activation infrastructure.

## 3. Online references to read and follow

Official documentation (source of truth):
- Microsoft Support, Activate Windows: https://support.microsoft.com/en-us/windows/activation/activate-windows
- Microsoft Learn, Windows WMI licensing class `SoftwareLicensingProduct`: https://learn.microsoft.com/en-us/previous-versions/windows/desktop/sppwmi/softwarelicensingproduct
- Microsoft Learn, Windows WMI licensing class `SoftwareLicensingService`: https://learn.microsoft.com/en-us/previous-versions/windows/desktop/sppwmi/softwarelicensingservice
- `InstallProductKey` method: https://learn.microsoft.com/en-us/previous-versions/windows/desktop/sppwmi/installproductkey-softwarelicensingservice
- `Activate` method: https://learn.microsoft.com/en-us/previous-versions/windows/desktop/sppwmi/activate-softwarelicensingproduct
- WMI C++ example (ExecMethod): https://learn.microsoft.com/en-us/windows/win32/wmisdk/example--calling-a-provider-method
- Slmgr command semantics, diagnostic reference: https://learn.microsoft.com/en-us/windows-server/get-started/activation-slmgr-vbs-options
- Windows activation error explanations: https://support.microsoft.com/en-us/windows/activation/get-help-with-windows-activation-errors
- Win32_OperatingSystem fields: https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-operatingsystem
- Windows 10 / 11 OS versions (both 10.0): https://learn.microsoft.com/en-us/windows/win32/sysinfo/operating-system-version
- Windows Settings URI: https://learn.microsoft.com/en-us/windows/apps/develop/launch/launch-settings
- Principle of least privilege: https://learn.microsoft.com/en-us/windows/win32/secbp/running-with-administrator-privileges
- Named pipe security: https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights
- Windows 10 support end date: https://support.microsoft.com/en-us/windows/deployment/updates-lifecycle/windows-10-support-has-ended-on-october-14-2025

GitHub reference projects (use for techniques, not for activation bypasses):
- Microsoft classic Win32 samples: https://github.com/microsoft/Windows-classic-samples
- Microsoft's header-only Windows Implementation Libraries (RAII), optional inspiration: https://github.com/microsoft/wil
- Microsoft's Windows App SDK samples (optional future WinUI 3 redesign): https://github.com/microsoft/WindowsAppSDK-Samples
- WinLic licensing utility (GUI/workflows reference only, written in WPF/PowerShell, **not** a native C++ implementation): https://github.com/ardennguyen/WinLic

Do not copy code from third-party activators, especially scripts that set unauthorized KMS host addresses.

## 4. Supported environment and OS recognition

Only Windows **client/workstation** systems, not Windows Server and not Windows 8/8.1. Query WMI:

```sql
SELECT Version, BuildNumber, Caption, OperatingSystemSKU, ProductType FROM Win32_OperatingSystem
```

- Windows 10 and 11 both report `Version` 10.0. Do not distinguish only using `GetVersionEx` or string `Caption`.
- Require `ProductType == 1` (workstation) and version major 10/minor 0.
- Build number >= 22000: Windows 11; build number < 22000: Windows 10, within reasonable Windows 10 ranges. Treat other builds or parse errors as unsupported/unknown.
- Use `OperatingSystemSKU` to display Home/Pro/Education/Enterprise/Pro for Workstations if mapped, otherwise display a safe edition label based on returned metadata. `Caption` can be localized.
- Disable modifying actions on unsupported systems. Show detected OS and a clear message.
- Target x64 in CI. A possible x86 version is out of scope.

## 5. Main screen

Single-window layout, approx. 780x560 DIP minimum 700x480 DIP, scrollable diagnostics if needed:

```text
WinActivate Manager                              [About]
--------------------------------------------------------------
Windows: Windows 11 Pro          OS build: 26100
License:  Activated             Partial key: *****-3V66T
Channel: Retail (if available; otherwise Unknown)

[Refresh status]   [Activate existing license]

Enter a product key you already own:
[XXXXX-XXXXX-XXXXX-XXXXX-XXXXX        ]  [Show/Hide]
[Install key & activate]

Status / operation progress:
  Ready. Activation changes require administrator approval.

[View diagnostics]  [Open Windows Activation Settings]
--------------------------------------------------------------
No key is stored or uploaded by this application.
```

The key edit uses password masking by default; can show/hide at user request; support clipboard paste and clear the control after a completed attempt. Never use real sample product keys in mock data, screenshots, tests, or docs. Keep button labels precise. If already activated, do not auto-install a replacement key; warn explicitly that it can replace a functioning license. Avoid animations that block accessibility.

Display a richer status mapping using WMI `LicenseStatus` values: 0 unlicensed, 1 licensed, 2 initial grace, 3 extended grace due to expiry, 4 non-genuine grace, 5 notification, 6 extended grace; any other code = unknown. **Only 1 means Activated.** Do not mislabel `Unlicensed` as proof no digital entitlement exists. Do not promise to detect Microsoft-account-linked digital license state: WMI does not make that reliably available.

## 6. Required product data types

- `OsInfo`: `enum WindowsFamily { Unsupported, Win10, Win11 }`, `build`, `version`, `caption`, `sku`, `edition`, `isWorkstation`, `isSupported`.
- `LicenseRecord`: `wmiObjectPath` (internal only), `activationId`, `applicationId`, `name`, `description`, `licenseFamily`, `licenseStatus`, `licenseStatusReason`, `optional partialKey`, `isAddon`.
- `LicenseSnapshot`: OS, records, chosen `optional<LicenseRecord>`, status `enum { Activated, NotActivated, Grace, Notification, Unknown }`, `selectionIsAmbiguous`, human-readable reason.
- `ActivationError`: `stage` (query, validate, elevate, install, activate, refresh, verify, IPC), signed HRESULT and/or unsigned WMI method `ReturnValue`, translated message, remediation URL.
- `ActivationResult`: stage results, `finalSnapshot`, `boolean verifiedActivated`, optional `ActivationError`. Never return a raw product key.
- `ILicensingBackend` interface for OS/status query and activation actions. Implement `WmiLicensingBackend` and `MockLicensingBackend` for tests.

## 7. WMI implementation details

### 7.1 Initialization

On each worker thread using WMI, call `CoInitializeEx(nullptr, COINIT_MULTITHREADED)` and balance successful calls with `CoUninitialize`. Set process COM security once at startup using `CoInitializeSecurity`; if security was already initialized, handle `RPC_E_TOO_LATE` thoughtfully. Create `IWbemLocator`, call `ConnectServer` for local `ROOT\\CIMV2`, then apply `CoSetProxyBlanket` with appropriate impersonation. Use RAII for COM, VARIANT, BSTR. Never call slow WMI operations directly from the GUI thread.

### 7.2 Read licensing data

Read the Windows client app ID, `55c92734-d682-4d71-983e-d6ec3f16059f`, with WQL:

```sql
SELECT * FROM SoftwareLicensingProduct WHERE ApplicationID = '55c92734-d682-4d71-983e-d6ec3f16059f'
```

Filter out `LicenseIsAddon == TRUE`; strongly prefer records with a nonempty `PartialProductKey` and a product/edition matching the installed Windows SKU. `PartialProductKey` is only the last five characters. Read `LicenseStatus`, `LicenseStatusReason`, `Name`, `Description`, `ID`, `LicenseFamily`, `__PATH`. Some systems may return multiple Windows SKU records, some no product-key row, or ambiguous licensing objects. Do **not** infer active Windows status from an arbitrary Windows record having `LicenseStatus == 1`. If no deterministic edition match exists, show `Unknown` and refer to Windows Activation Settings rather than choosing the wrong SKU. Always preserve errors distinctly from an unlicensed state.

Where `Description` includes a channel (Retail, OEM, volume), display it as `Reported channel`, not as a guarantee about entitlement.

### 7.3 Install product key (privileged)

Query `SELECT * FROM SoftwareLicensingService`, obtain the **instance path** and invoke the instance method `InstallProductKey` through `IWbemServices::ExecMethod` using a `ProductKey` BSTR input parameter, not an OS command string. Validate, normalize and mask the key before invocation. Carefully read both `HRESULT` (transport/call errors) and `ReturnValue` from WMI provider: success of the COM method call does not necessarily mean success of the licensing operation. The service can reject invalid or incompatible keys.

An installed product key might replace the existing product key. If current Windows is activated, ask for explicit confirmation before replacing it.

### 7.4 Request activation (privileged)

After installation: requery Windows licensing records; identify the record applicable to the installed Windows edition. Call `SoftwareLicensingProduct.Activate()` on that instance via `ExecMethod` with no input parameters, not on an arbitrary product. For an existing installed license, do the same without key installation. If there's no unambiguous applicable product instance, stop gracefully and offer the Microsoft Settings activation troubleshooter. Use `SoftwareLicensingService.RefreshLicenseStatus()` when available and reread status. Method `ReturnValue=0` only means an operation did not report failure; final success requires reread `LicenseStatus=1` for the correct product.

Do not proactively switch Windows editions, reconfigure organizational KMS hosts, start unauthorized key activation, or perform registry modifications. Organization-managed volume licensing can already activate through its configured organizational infrastructure, but contact IT on failures.

### 7.5 Why not shell scripts

Microsoft's `slmgr.vbs /ipk`, `/ato`, `/xpr`, `/dlv` are documented equivalents for manual diagnostics, but **do not call** `system`, `cmd /c`, PowerShell, or `cscript` with the user key in command-line arguments. This avoids fragile localized-output parsing and process-command-line exposure. A read-only troubleshooting view can recommend documented commands for manual use, without silently executing them.

## 8. Privilege separation and IPC

- `WinActivate.exe`: explicitly manifest `asInvoker` and never ask for admin just to read status.
- `WinActivateHelper.exe`: explicitly manifest `requireAdministrator`. Launch it with `ShellExecuteExW` using `runas` only after an explicit user action. The executable path is fully qualified from the trusted application directory; don't search `PATH` or the current directory.
- No key on command line, in environment variables, temp files, registry, IPC names, logs, crash reports, or error dialogs.
- GUI creates a fresh locally restricted named pipe with an unpredictable per-operation name and security descriptor allowing only the intended user/session and necessary elevated identity. Reject remote clients (`PIPE_REJECT_REMOTE_CLIENTS`). Helper's arguments contain the pipe name/nonce (not key). Use a length-prefixed, versioned, strictly bounded protocol: `InstallAndActivate{key}` or `ActivateExisting{}` and a structured result. No generic shell-command operation.
- GUI verifies connecting helper process ID from `ShellExecuteExW` against `GetNamedPipeClientProcessId`, using an appropriate anti-race strategy; helper verifies server process identity using `GetNamedPipeServerProcessId` when appropriate. Authenticate endpoints and carefully design DACLs for UAC's admin account scenario.
- Only one activation operation at a time. Use bounded messages (<1 KiB), reasonable connect/read/operation timeouts, and explicit user-facing handling for UAC decline, helper failure, IPC disconnect, and cancellation before sending a key.
- Zero sensitive mutable key buffers after use with `SecureZeroMemory` where feasible; don't claim perfect memory erasure for every transient copy. Never persist or log the key.
- On any error, refresh status from Windows before displaying the final result.
- Close the helper as soon as one task completes; never install a service or schedule a task.

## 9. Responsive GUI and usability

- Main window constructed with `CreateWindowExW`, native controls, `WM_COMMAND`, and Win32 message pump.
- Use Unicode APIs (`*W`) throughout. Apply DPI scaling (`SetProcessDpiAwarenessContext` with fallback) and readable UI fonts.
- Use a background worker (`std::jthread` or `std::thread`) for read-only WMI queries and helper process supervision; use `PostMessageW` with `WM_APP + ...` messages to marshal results to UI thread.
- Disable conflicting controls during operations; use progress stage messages: Checking system, Requesting admin permission, Installing license key, Activating, Verifying.
- Support keyboard navigation, visible focus, high-contrast text, progress readable by screen readers, clear error messaging. No endlessly spinning UI.
- `Open activation settings` runs `ShellExecuteW(..., L"ms-settings:activation", ...)` and provides a fallback if opening fails.
- For a verified activated installation, show `Windows is activated`; never use "success" solely because `InstallProductKey` succeeded.
- On already-activated machines, retry should show already activated, not overwrite anything.

## 10. Error handling matrix

Always show 0xXXXXXXXX code plus short suggested action, preserving unknown errors unmodified:

| Code / condition | Guidance |
|---|---|
| `0xC004F050` | Key invalid/inapplicable: check key spelling and edition. |
| `0xC004F210`, `0xC004F212` | Wrong Windows edition: verify Home vs Pro vs Enterprise. |
| `0x803F7001` | No matching valid digital/product-key license found: use genuine key, purchase license, or official troubleshooter. |
| `0xC004C003` | Key blocked/rejected or invalid: contact seller/Microsoft. |
| `0xC004F074`, `0x8007232B` | Likely organization/KMS network or DNS: contact IT, connect to managed workplace network. |
| `0x80072F8F` | Check time, TLS connectivity, and internet access. |
| `0x80070005` / access denied | Admin permission, Windows policy, or WMI permissions: report exact stage and code. |
| RPC/WMI failure | Separate provider unavailability from not-activated. |
| Ambiguous product record | Refuse mutating action; open official Windows Activation Settings. |
| UAC denied | No changes; explain and allow retry. |
| Activation returns success, status still nonlicensed | Report pending/not confirmed; offer refresh, restart if appropriate, and Settings. |

Link error explanations to Microsoft's official Support page. Do not invent error meanings for unfamiliar codes. Never expose the user's entire key.

## 11. Suggested file tree

```text
WinActivate/
  CMakeLists.txt
  CMakePresets.json
  README.md
  LICENSE
  AGENTS.md
  docs/
    SECURITY.md
    TEST_PLAN.md
    TROUBLESHOOTING.md
  assets/
    WinActivate.ico
  src/
    common/
      Types.h
      LicensingBackend.h
      KeyValidation.h
      KeyValidation.cpp
      SecureBuffer.h
      FormatError.h
      FormatError.cpp
      IpcProtocol.h
      IpcProtocol.cpp
      WinHandle.h
    core/
      WmiConnection.h
      WmiConnection.cpp
      OsDetector.h
      OsDetector.cpp
      LicenseReader.h
      LicenseReader.cpp
      LicenseActivator.h
      LicenseActivator.cpp
      ProductSelection.h
      ProductSelection.cpp
    gui/
      Main.cpp
      MainWindow.h
      MainWindow.cpp
      Resources.rc
      App.manifest
    helper/
      HelperMain.cpp
      ElevatedHandler.h
      ElevatedHandler.cpp
      Helper.manifest
  tests/
    TestsMain.cpp
    KeyValidationTests.cpp
    OsDetectionTests.cpp
    ProductSelectionTests.cpp
    ErrorMappingTests.cpp
    IpcProtocolTests.cpp
    MockBackendTests.cpp
  .github/workflows/
    windows-build.yml
```

Use straightforward class boundaries. All licensing operations belong in `core`; GUI must never call WMI methods directly. `WmiConnection` owns COM/WMI setup on a worker thread. `ProductSelection` logic must be independently testable without WMI. Avoid hardcoding license activation IDs that change per edition/installation.

## 12. Build and automated verification

CMake requirements:
- `cmake_minimum_required(VERSION 3.24)` and `CMAKE_CXX_STANDARD 20`.
- Build x64 `WinActivate.exe` and `WinActivateHelper.exe` (GUI subsystem; no command-prompt windows). Explicit embedded manifests: `asInvoker`, `requireAdministrator`.
- Compile definitions: `UNICODE`, `_UNICODE`, `WIN32_LEAN_AND_MEAN`, `NOMINMAX`. Use `/W4`, `/permissive-`, modern C++ warnings; no ignoring build errors. Link Windows SDK libs as needed: `wbemuuid`, `ole32`, `oleaut32`, `advapi32`, `shell32`, `comctl32`, `bcrypt`.
- CTest test executable with a small local assertion-based test framework or reputable test framework, **ensuring assertions are not disabled in Release**.
- GitHub Actions `windows-latest` compile and run CTest on push/PR. Never run real install/activation on CI.

Local commands in a Visual Studio Developer Command Prompt:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

An `.exe` built for Windows cannot be validated by merely compiling in a Linux container. Codex must report if Windows build/test environment is not available.

## 13. Tests and acceptance criteria

Automated tests with fake backends and WMI records:

1. Valid 25-character ASCII key, with or without dashes normalized to 5x5; lowercase normalized; invalid characters/length rejected; never print the key.
2. Windows 10 build 19045 is accepted; Windows 11 build 22000 and 26100 accepted; Windows 8.1 (6.3) and Server (ProductType != 1) rejected; unknown WMI data yields Unsupported/Unknown.
3. WMI LicenseStatus 0 through 6 mapped precisely; null/nonmatching/multiple records not mislabeled as activated.
4. Activated installed SKU selected deterministically; multiple licensed records for other SKUs do not cause false positives. Ambiguity refuses mutation.
5. Key installation failure does not proceed to activation; if key install succeeds but activation fails, resulting status is still queried and shown.
6. IPC serialized request schema rejects oversized/unknown/malformed commands and remote connection; helper verifies identity; no command-line or logs contain product key.
7. UAC cancellation, WMI not running, access denied, timeouts, no key, offline network, wrong edition, callback races, and double clicks handled without crashing UI.
8. `Activated` is displayed only after a post-operation WMI snapshot returns licensed status for the selected OS product.
9. Unit/integration tests run without changing host machine licenses, keys, activation config, services, scheduled tasks, or registry.

Manual tests (only on owned devices with permission, with backups or VM checkpoints as appropriate):
- Windows 10 22H2 x64, Windows 11 current x64, both an already-activated and an unactivated environment if legitimately available.
- Check status with normal privileges, opens activation Settings, cancel UAC, activate with a legally owned key, confirm Windows Settings agrees, test wrong key and wrong edition safely.
- Test high-DPI 125/150/200%, keyboard navigation, disconnected network, enterprise-managed machine without modifying its KMS configuration.
- Keep real keys and personal identifiers out of screenshots, logs, fixtures, and CI artifacts.

## 14. Codex phases / commits

1. **Scaffold**: repository, CMake, manifests, resources, README, buildable Win32 GUI skeleton, basic local-only tests.
2. **Read-only OS and licensing**: WMI RAII and COM security, OS detection, Windows license selection, status UI, error reporting. Compile and test.
3. **Mock activation workflows**: key validation, mock backend, state machine, progress UI, all branch tests. Build and test without modifying any license.
4. **Privilege and IPC**: elevated helper, launch only on explicit command, authenticated restricted named pipe, secure key transfer, structured results, unit tests.
5. **Real activation via WMI**: InstallProductKey, correct product's Activate, refresh, reread, error mapping. Must not use `slmgr` subprocess or external KMS.
6. **UX/polish**: DPI, accessibility, warnings, diagnostics link, no hidden failures, robust window shutdown/thread cleanup.
7. **Verification/release**: GitHub Actions Windows build + CTest, manual checklist, package two EXEs and README/license, optional code signing; disclose any untested areas.

End of every phase: inspect changes, compile with MSVC, run tests, fix issues, summarize the actual files and evidence of completion. Do not skip building or invent successful test results. Keep commits small and descriptive.

## 15. Definition of done

- x64 Release builds on supported Windows with MSVC and CMake; GUI and helper launch correctly.
- Reads OS/edition/build and Windows license state without admin.
- Only activates existing lawful Windows entitlements or installs user-entered authentic keys via WMI with UAC.
- Never displays full key after input or leaks it into logs, files, command lines, or Git history.
- Does not silently change volume activation/KMS settings or bypass licensing.
- Correctly handles ambiguous and unknown licensing states and code values.
- Re-reads Windows state after attempts, never claiming unverified success.
- Tests pass on Windows CI and document manual activation testing without embedding a real key.
- Complete README with setup, build, operation, security boundaries, supported OSes, Windows 10 lifecycle caveat, and troubleshooting links.
