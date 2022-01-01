#pragma once

#include "common/LicensingBackend.h"

namespace winactivate {

class WmiLicensingBackend final : public ILicensingBackend {
public:
    [[nodiscard]] SnapshotQueryResult QuerySnapshot() override;
};

}  // namespace winactivate

