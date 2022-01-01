#include "TestFramework.h"

#include "core/OsDetector.h"

using winactivate::ClassifyWindows;
using winactivate::WindowsFamily;

TEST_CASE("Windows 10 build 19045 is supported") {
    const auto os = ClassifyWindows(L"10.0.19045", L"19045", L"Windows 10 Pro", 48, 1);
    REQUIRE(os.isSupported);
    REQUIRE(os.family == WindowsFamily::Win10);
    REQUIRE(os.edition == L"Pro");
}

TEST_CASE("Windows 11 builds are distinguished by build number") {
    REQUIRE(ClassifyWindows(L"10.0", L"22000", L"Windows", 101, 1).family ==
            WindowsFamily::Win11);
    REQUIRE(ClassifyWindows(L"10.0.26100", L"26100", L"Windows", 48, 1).family ==
            WindowsFamily::Win11);
}

TEST_CASE("Windows 8.1 and Server are rejected") {
    REQUIRE(!ClassifyWindows(L"6.3", L"9600", L"Windows 8.1", 48, 1).isSupported);
    REQUIRE(!ClassifyWindows(L"10.0", L"20348", L"Windows Server", 0, 3).isSupported);
}

TEST_CASE("malformed WMI version or build is rejected") {
    REQUIRE(!ClassifyWindows(L"10", L"19045", L"Windows", 48, 1).isSupported);
    REQUIRE(!ClassifyWindows(L"10.0", L"unknown", L"Windows", 48, 1).isSupported);
    REQUIRE(!ClassifyWindows(L"10.0", L"9999", L"Windows", 48, 1).isSupported);
}

