#include "TestFramework.h"

#include "common/FormatError.h"

TEST_CASE("error codes are formatted as eight uppercase hexadecimal digits") {
    REQUIRE(winactivate::FormatErrorCode(0x80070005) == L"0x80070005");
    REQUIRE(winactivate::FormatErrorCode(5) == L"0x00000005");
}

TEST_CASE("known activation errors have actionable guidance") {
    REQUIRE(!winactivate::GuidanceForCode(0xC004F050).empty());
    REQUIRE(!winactivate::GuidanceForCode(0x80070005).empty());
    REQUIRE(winactivate::GuidanceForCode(0xDEADBEEF).empty());
}

TEST_CASE("unknown errors preserve their code") {
    winactivate::ActivationError error;
    error.stage = winactivate::ActivationStage::Query;
    error.hresult = static_cast<std::int32_t>(0xDEADBEEF);
    const std::wstring text = winactivate::DescribeActivationError(error);
    REQUIRE(text.find(L"0xDEADBEEF") != std::wstring::npos);
}

