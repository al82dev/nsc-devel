#include "DriverTestSupport.h"

namespace nsc::suite
{
using namespace driver_support;

void registerDriverInstallation433To496Module(TestRegistry& registry)
{
    addInfrastructureContract(registry, 55U, "integration",
        "CF-DRIVER-055 requires a synthetic installation root with canonical and escaping path fixtures");
    addInfrastructureContract(registry, 56U, "integration",
        "CF-DRIVER-056 requires a complete expected-file inventory and independently hashed installation tree");
    addInfrastructureContract(registry, 57U, "integration",
        "CF-DRIVER-057 requires authenticated manifest mutations and residue fixtures");
    addInfrastructureContract(registry, 58U, "integration",
        "CF-DRIVER-058 requires a complete supported installation layout and capability documents");
    addInfrastructureContract(registry, 59U, "integration",
        "CF-DRIVER-059 requires paired independently produced installation manifests and volatile-field fixtures");
    addInfrastructureContract(registry, 60U, "integration",
        "CF-DRIVER-060 requires current and next authenticated package manifests");
    addInfrastructureContract(registry, 61U, "integration",
        "CF-DRIVER-061 requires an authenticated installed-file manifest for removal planning");
    addInfrastructureContract(registry, 62U, "integration",
        "CF-DRIVER-062 requires filesystem fault injection to prove rollback and directory pruning");
}
}
