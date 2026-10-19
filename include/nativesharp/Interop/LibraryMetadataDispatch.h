#pragma once

#include "nativesharp/Interop/LibraryMetadataDispatchInterface.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::interop
{
struct LibraryMetadataDispatch final
{
    std::string typeKey;
    std::uint64_t stableTypeId = 0;
    std::string descriptorName;
    // NSMETA 1.6. PE imports functions and data through different indirections. Consumers call
    // this no-argument function to obtain the producer's actual descriptor address instead of
    // treating an import-address-table cell as the descriptor itself.
    std::string descriptorAccessorName;
    std::string vtableName;
    std::string destructionEntryName;
    std::vector<std::string> slotSignatures;
    std::vector<std::string> slotCalleeNames;
    std::vector<LibraryMetadataDispatchInterface> interfaces;
};
}
