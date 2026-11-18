#pragma once

#include "nativesharp/Frontend/PrecompiledFieldCertificateEntry.h"

#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct PrecompiledFieldCertificate final
{
    std::string typeIdentity;
    bool isClass = false;
    std::string baseTypeIdentity;
    std::uint64_t baseSize = 0;
    std::uint64_t baseAlignment = 0;
    std::uint64_t classSize = 0;
    std::uint64_t classAlignment = 0;
    std::vector<PrecompiledFieldCertificateEntry> fields;
};
}
