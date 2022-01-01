#pragma once

#include <Windows.h>
#include <Wbemidl.h>
#include <wrl/client.h>

#include <string_view>
#include <vector>

namespace winactivate {

[[nodiscard]] HRESULT InitializeProcessComSecurity() noexcept;

class WmiConnection final {
public:
    WmiConnection();
    ~WmiConnection();

    WmiConnection(const WmiConnection&) = delete;
    WmiConnection& operator=(const WmiConnection&) = delete;

    [[nodiscard]] HRESULT status() const noexcept { return status_; }
    [[nodiscard]] HRESULT Query(
        std::wstring_view wql,
        std::vector<Microsoft::WRL::ComPtr<IWbemClassObject>>& results) const;

private:
    bool comInitialized_{};
    HRESULT status_{E_UNEXPECTED};
    Microsoft::WRL::ComPtr<IWbemServices> services_;
};

}  // namespace winactivate

