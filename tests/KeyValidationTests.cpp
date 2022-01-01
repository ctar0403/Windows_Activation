#include "TestFramework.h"

#include "common/KeyValidation.h"

#include <string>

using winactivate::ValidateAndNormalizeProductKey;

TEST_CASE("product key accepts five groups and normalizes case") {
    const auto result = ValidateAndNormalizeProductKey(
        L"abcde-fghij-klmno-pqrst-uvwxy");
    REQUIRE(result.valid);
    REQUIRE(result.normalized == L"ABCDE-FGHIJ-KLMNO-PQRST-UVWXY");
}

TEST_CASE("product key accepts 25 undashed characters") {
    const auto result = ValidateAndNormalizeProductKey(L"ABCDE12345FGHIJ67890KLMNO");
    REQUIRE(result.valid);
    REQUIRE(result.normalized == L"ABCDE-12345-FGHIJ-67890-KLMNO");
}

TEST_CASE("product key rejects wrong length") {
    const auto result = ValidateAndNormalizeProductKey(L"ABCDE-FGHIJ");
    REQUIRE(!result.valid);
    REQUIRE(result.normalized.empty());
}

TEST_CASE("product key rejects whitespace and non-ASCII characters") {
    REQUIRE(!ValidateAndNormalizeProductKey(L"ABCDE FGHIJ KLMNO PQRST UVWXY").valid);
    REQUIRE(!ValidateAndNormalizeProductKey(L"ABCDE-FGHIJ-KLMNO-PQRST-UVWXYÉ").valid);
}

TEST_CASE("invalid result never returns the submitted key") {
    const std::wstring submitted = L"NOT-A-REAL-KEY";
    const auto result = ValidateAndNormalizeProductKey(submitted);
    REQUIRE(!result.valid);
    REQUIRE(result.error.find(submitted) == std::wstring::npos);
}

