# Security

## Trust boundary

WinActivate Manager is designed to run its GUI as the signed-in user. Read-only
licensing checks do not require elevation. A separate one-shot helper requests
administrator approval only after an explicit activation action. The helper and
authenticated local IPC are implemented in the privilege-separation phase.

The project must never:

- generate, guess, collect, or persist product keys;
- pass a full key through command-line arguments or environment variables;
- write a key to files, logs, telemetry, crash reports, the registry, or an IPC
  endpoint name;
- configure third-party KMS services, patch licensing components, or use rearm
  as an activation bypass;
- declare activation successful without rereading the applicable Windows
  licensing record and observing `LicenseStatus == 1`.

Sensitive mutable buffers are cleared with `SecureZeroMemory` where practical.
This reduces accidental retention but cannot promise erasure of every copy made
by Windows, the C++ runtime, or the WMI provider.

## Reporting a vulnerability

Do not include product keys, machine identifiers, account data, or diagnostic
dumps containing private information in a report. Share a minimal reproduction
with the project owner through a private channel established for this repository.

