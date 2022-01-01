# Test plan

Automated tests are local-only and must not call real WMI activation methods.
They cover key parsing, OS classification, licensing-record selection, status
mapping, error mapping, workflow state transitions, and bounded IPC parsing as
those modules are introduced.

CI builds x64 Release with MSVC and runs CTest. It must never contain or install
a real key or alter services, scheduled tasks, registry values, licensing files,
or activation configuration.

Manual activation tests require an owned or explicitly authorized Windows 10 or
Windows 11 x64 machine and a legitimate key or entitlement. Keep keys and device
identifiers out of screenshots and test artifacts. Verify normal-user status
reading, UAC cancellation, invalid/wrong-edition handling, offline behavior,
Windows Settings agreement, high DPI, keyboard access, and clean shutdown.

