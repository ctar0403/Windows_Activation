#include "core/WmiConnection.h"

#include <objbase.h>

namespace winactivate {
namespace {

class UniqueBstr final {
public:
    explicit UniqueBstr(const wchar_t* text) : value_(SysAllocString(text)) {}
    explicit UniqueBstr(std::wstring_view text)
        : value_(SysAllocStringLen(text.data(), static_cast<UINT>(text.size()))) {}
    ~UniqueBstr() { SysFreeString(value_); }

    UniqueBstr(const UniqueBstr&) = delete;
    UniqueBstr& operator=(const UniqueBstr&) = delete;

    [[nodiscard]] BSTR get() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept { return value_ != nullptr; }

private:
    BSTR value_{};
};

}  // namespace

HRESULT InitializeProcessComSecurity() noexcept {
    const HRESULT result = CoInitializeSecurity(
        nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    return result == RPC_E_TOO_LATE ? S_OK : result;
}

WmiConnection::WmiConnection() {
    status_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(status_)) {
        return;
    }
    comInitialized_ = true;

    Microsoft::WRL::ComPtr<IWbemLocator> locator;
    status_ = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                               IID_PPV_ARGS(&locator));
    if (FAILED(status_)) {
        return;
    }

    UniqueBstr nameSpace(L"ROOT\\CIMV2");
    if (!nameSpace.valid()) {
        status_ = E_OUTOFMEMORY;
        return;
    }
    status_ = locator->ConnectServer(nameSpace.get(), nullptr, nullptr, nullptr, 0,
                                     nullptr, nullptr, &services_);
    if (FAILED(status_)) {
        return;
    }

    status_ = CoSetProxyBlanket(
        services_.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
}

WmiConnection::~WmiConnection() {
    services_.Reset();
    if (comInitialized_) {
        CoUninitialize();
    }
}

HRESULT WmiConnection::Query(
    std::wstring_view wql,
    std::vector<Microsoft::WRL::ComPtr<IWbemClassObject>>& results) const {
    if (FAILED(status_)) {
        return status_;
    }

    UniqueBstr language(L"WQL");
    UniqueBstr query(wql);
    if (!language.valid() || !query.valid()) {
        return E_OUTOFMEMORY;
    }

    Microsoft::WRL::ComPtr<IEnumWbemClassObject> enumerator;
    HRESULT result = services_->ExecQuery(
        language.get(), query.get(),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    if (FAILED(result)) {
        return result;
    }

    for (;;) {
        Microsoft::WRL::ComPtr<IWbemClassObject> object;
        ULONG returned = 0;
        result = enumerator->Next(WBEM_INFINITE, 1, &object, &returned);
        if (FAILED(result)) {
            return result;
        }
        if (returned == 0) {
            return S_OK;
        }
        results.push_back(std::move(object));
    }
}

}  // namespace winactivate

