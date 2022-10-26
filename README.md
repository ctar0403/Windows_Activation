# WinActivate Manager

WinActivate Manager is a local-only native Windows desktop application for
inspecting Windows licensing and activating genuine Windows 10 or Windows 11
licenses. It does not generate product keys, bypass Microsoft activation, alter
KMS configuration, or communicate with any third-party service.

> Development status: phases 1–3 are implemented. The application has a native
> Win32 shell, read-only WMI OS/license discovery, deterministic product
> selection, and a mock-tested activation workflow. The elevated helper, secure
> IPC, and real WMI mutation methods are not implemented yet, so this revision
> intentionally cannot install a key or request activation.

## Supported systems

- Windows 10 or Windows 11 client/workstation, x64
- Windows Server, Windows 8/8.1, and x86 are not supported
- Visual Studio 2022 or newer with Desktop development with C++
- Windows 10 or Windows 11 SDK
- CMake 3.24 or newer

Windows 10 Home and Pro reached end of support on October 14, 2025. Activation
does not provide security updates; servicing availability depends on the edition
and any applicable Microsoft program.

## Build and test

The easiest option is to double-click `build.cmd`. It configures an x64 Release
build, compiles the application, runs the mock-backed tests, and copies the
preview executable to `dist\WinActivate.exe`. The script prefers the required
Visual Studio/CMake toolchain. If only MinGW is installed, it can create a local
interface-preview build, but that fallback is not a release-validation build.

From a terminal, the equivalent convenient command is:

```bat
build.cmd Release --no-pause
```

Use `build.cmd Debug --no-pause` for a Debug build.

From a Visual Studio Developer PowerShell:

```powershell
cmake --preset vs2022-x64
cmake --build --preset release
ctest --preset release
```

The CMake Release application is written beneath
`build/vs2022-x64/Release`. `build.cmd` additionally copies it to `dist`.

The current preview is read-only: status refresh, diagnostics, and the official
Windows Activation Settings link work, while the two activation buttons remain
disabled until the elevated helper and authenticated IPC are implemented.

## Safety and privacy

The application accepts only a key the user already owns. It has no telemetry,
backend, account login, or key-validation service. A product key is never put in
command-line arguments, environment variables, files, logs, the registry, or
error dialogs. See [Security](docs/SECURITY.md) and
[Troubleshooting](docs/TROUBLESHOOTING.md).

Real activation testing must be performed only on an owned or explicitly
authorized Windows device using a legitimate license. Automated tests never
install a key or change host licensing state.

