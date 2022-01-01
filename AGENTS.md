# Codex project instructions: WinActivate Manager

You are implementing a native C++20 Windows 10/11 activation **manager for genuine licenses**, not a license bypass. Follow `CODEX_IMPLEMENTATION_PLAN.md` as the binding project specification.

## Engineering rules

1. Build real, tested C++ code; no placeholders for a deliverable phase. Run Windows MSVC builds and CTest when a Windows environment is available. Report limitations honestly if not.
2. Support Windows 10 and Windows 11 client x64 only; reject Windows 8/8.1 and Server. Both Windows 10 and 11 report OS version 10.0, so use WMI build number and ProductType.
3. Use Win32 Unicode UI; native WMI/COM (`ROOT\\CIMV2`) for status, installing valid product keys, and requesting existing-license activation. Use WMI provider method return codes and reread license state for verification.
4. Query Windows `SoftwareLicensingProduct` using application ID `55c92734-d682-4d71-983e-d6ec3f16059f`. Exclude add-on/mismatched entries and handle ambiguous records instead of guessing.
5. Never generate keys, add third-party KMS endpoints, disable licensing components, patch Windows licensing, or implement rearm-based bypasses.
6. Keep GUI `asInvoker`; elevate only `WinActivateHelper.exe` after user action and UAC approval. Use authenticated local IPC; never pass a full key via arguments, environment variables, files, logs, telemetry, crash dumps, or registry.
7. Use no external runtime services, backend, or telemetry. Validate input, guard sensitive buffers, do not expose the full entered key on error dialogs. Confirm explicitly before replacing a currently activated key.
8. No `system()`, `cmd /c`, PowerShell, or `slmgr.vbs /ipk <key>` subprocess. Do not parse localized slmgr text to decide status.
9. Don't block the Win32 UI thread with WMI or helper waits. Marshal events back through Win32 message posting.
10. Separate modules for OS detection, licensing state, product selection, activation, IPC, UI, error mapping; keep business logic testable via mock backend.
11. Handle every Windows HRESULT, WMI ReturnValue, UAC decline, ambiguous product status and IPC error. Show `Activated` only if the applicable Windows licensing record has LicenseStatus == 1 on a freshly read snapshot.
12. GitHub Actions should compile/test Windows code without running any real activation method. Real activation tests require an explicitly authorized owned Windows machine and legitimate key; never include keys in test artifacts.

## Execution procedure

Read the full `CODEX_IMPLEMENTATION_PLAN.md`, inspect the repo, then work phase by phase. After each phase: build, run tests, repair failures, report exactly which files changed, then proceed. Avoid asking about minor design choices already resolved in the spec. Do not claim verification you could not perform.

## Final deliverable

`WinActivate.exe` + `WinActivateHelper.exe`, CMake build scripts, mock-backed tests, Windows CI workflow, README, SECURITY and TROUBLESHOOTING docs, and clear final build/run instructions.
