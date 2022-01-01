#include "core/LicenseReader.h"

#include "common/FormatError.h"
#include "core/OsDetector.h"
#include "core/ProductSelection.h"
#include "core/WmiConnection.h"

#include <Windows.h>
#include <Wbemidl.h>
#include <wrl/client.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace winactivate {
namespace {

class ScopedVariant final {
public:
    ScopedVariant() { VariantInit(&value_); }
    ~ScopedVariant() { VariantClear(&value_); }
    ScopedVariant(const ScopedVariant&) = delete;
    ScopedVariant& operator=(const ScopedVariant&) = delete;
    VARIANT* put() noexcept { return &value_; }
    const VARIANT& get() const noexcept { return value_; }

private:
    VARIANT value_{};
};

HRESULT ReadString(IWbemClassObject* object, const wchar_t* property,
                   std::wstring& output, bool optional = false) {
    ScopedVariant value;
    const HRESULT result = object->Get(property, 0, value.put(), nullptr, nullptr);
    if (FAILED(result)) {
        return optional && result == WBEM_E_NOT_FOUND ? S_OK : result;
    }
    if (value.get().vt == VT_NULL || value.get().vt == VT_EMPTY) {
        output.clear();
        return S_OK;
    }
    if (value.get().vt != VT_BSTR || value.get().bstrVal == nullptr) {
        return WBEM_E_TYPE_MISMATCH;
    }
    output.assign(value.get().bstrVal, SysStringLen(value.get().bstrVal));
    return S_OK;
}

HRESULT ReadUint32(IWbemClassObject* object, const wchar_t* property,
                   std::uint32_t& output) {
    ScopedVariant value;
    const HRESULT result = object->Get(property, 0, value.put(), nullptr, nullptr);
    if (FAILED(result)) {
        return result;
    }
    if (value.get().vt == VT_I4) {
        output = static_cast<std::uint32_t>(value.get().lVal);
        return S_OK;
    }
    if (value.get().vt == VT_UI4) {
        output = value.get().ulVal;
        return S_OK;
    }
    if (value.get().vt == VT_I2) {
        output = static_cast<std::uint32_t>(value.get().iVal);
        return S_OK;
    }
    return WBEM_E_TYPE_MISMATCH;
}

HRESULT ReadBool(IWbemClassObject* object, const wchar_t* property, bool& output) {
    ScopedVariant value;
    const HRESULT result = object->Get(property, 0, value.put(), nullptr, nullptr);
    if (FAILED(result)) {
        return result;
    }
    if (value.get().vt != VT_BOOL) {
        return WBEM_E_TYPE_MISMATCH;
    }
    output = value.get().boolVal == VARIANT_TRUE;
    return S_OK;
}

ActivationError QueryError(HRESULT result, std::wstring message) {
    ActivationError error;
    error.stage = ActivationStage::Query;
    error.hresult = static_cast<std::int32_t>(result);
    error.message = std::move(message);
    error.remediationUrl =
        L"https://support.microsoft.com/windows/get-help-with-windows-activation-errors-09d8fb64-ab7c-4b75-8b1d-0d1efb40bfa7";
    return error;
}

HRESULT ParseOsRecord(IWbemClassObject* object, OsInfo& os) {
    std::wstring version;
    std::wstring build;
    std::wstring caption;
    std::uint32_t sku = 0;
    std::uint32_t productType = 0;
    HRESULT result = ReadString(object, L"Version", version);
    if (SUCCEEDED(result)) result = ReadString(object, L"BuildNumber", build);
    if (SUCCEEDED(result)) result = ReadString(object, L"Caption", caption);
    if (SUCCEEDED(result)) result = ReadUint32(object, L"OperatingSystemSKU", sku);
    if (SUCCEEDED(result)) result = ReadUint32(object, L"ProductType", productType);
    if (SUCCEEDED(result)) {
        os = ClassifyWindows(version, build, caption, sku, productType);
    }
    return result;
}

HRESULT ParseLicenseRecord(IWbemClassObject* object, LicenseRecord& record) {
    std::wstring partialKey;
    HRESULT result = ReadString(object, L"__PATH", record.wmiObjectPath);
    if (SUCCEEDED(result)) result = ReadString(object, L"ID", record.activationId);
    if (SUCCEEDED(result)) result = ReadString(object, L"ApplicationID", record.applicationId);
    if (SUCCEEDED(result)) result = ReadString(object, L"Name", record.name);
    if (SUCCEEDED(result)) result = ReadString(object, L"Description", record.description);
    if (SUCCEEDED(result)) result = ReadString(object, L"LicenseFamily", record.licenseFamily, true);
    if (SUCCEEDED(result)) result = ReadUint32(object, L"LicenseStatus", record.licenseStatus);
    if (SUCCEEDED(result)) result = ReadUint32(object, L"LicenseStatusReason", record.licenseStatusReason);
    if (SUCCEEDED(result)) result = ReadString(object, L"PartialProductKey", partialKey, true);
    if (SUCCEEDED(result)) result = ReadBool(object, L"LicenseIsAddon", record.isAddon);
    if (SUCCEEDED(result) && !partialKey.empty()) {
        record.partialKey = std::move(partialKey);
    }
    return result;
}

}  // namespace

SnapshotQueryResult WmiLicensingBackend::QuerySnapshot() {
    SnapshotQueryResult result;
    WmiConnection connection;
    if (FAILED(connection.status())) {
        result.error = QueryError(connection.status(),
                                  L"Windows Management Instrumentation could not be opened.");
        return result;
    }

    std::vector<Microsoft::WRL::ComPtr<IWbemClassObject>> osObjects;
    HRESULT queryResult = connection.Query(
        L"SELECT Version, BuildNumber, Caption, OperatingSystemSKU, ProductType "
        L"FROM Win32_OperatingSystem",
        osObjects);
    if (FAILED(queryResult) || osObjects.size() != 1) {
        result.error = QueryError(
            FAILED(queryResult) ? queryResult : WBEM_E_FAILED,
            L"The operating-system record could not be read unambiguously.");
        return result;
    }

    OsInfo os;
    queryResult = ParseOsRecord(osObjects.front().Get(), os);
    if (FAILED(queryResult)) {
        result.error = QueryError(queryResult,
                                  L"The operating-system record contains unreadable fields.");
        return result;
    }

    std::vector<Microsoft::WRL::ComPtr<IWbemClassObject>> licenseObjects;
    queryResult = connection.Query(
        L"SELECT * FROM SoftwareLicensingProduct WHERE ApplicationID = "
        L"'55c92734-d682-4d71-983e-d6ec3f16059f'",
        licenseObjects);
    if (FAILED(queryResult)) {
        result.snapshot.os = os;
        result.error = QueryError(queryResult,
                                  L"Windows licensing records could not be queried.");
        return result;
    }

    std::vector<LicenseRecord> records;
    records.reserve(licenseObjects.size());
    for (const auto& object : licenseObjects) {
        LicenseRecord record;
        queryResult = ParseLicenseRecord(object.Get(), record);
        if (FAILED(queryResult)) {
            result.snapshot.os = os;
            result.error = QueryError(queryResult,
                                      L"A Windows licensing record contains unreadable fields.");
            return result;
        }
        records.push_back(std::move(record));
    }
    result.snapshot = SelectApplicableProduct(os, records);
    return result;
}

}  // namespace winactivate

