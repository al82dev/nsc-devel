#pragma once

#include "nativesharp/Language/ErrorSemantics.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"
#include "nativesharp/Frontend/TemplateParseCache.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace nativesharp::frontend
{
// Debug and other late consumers need the authoritative physical class layout without reaching
// back into the mutable frontend pipeline.  This is the reference-shaped counterpart of
// ProjectOopMirStructLayout: object size includes the descriptor header and inherited subobject;
// fields carry their final byte offsets.
struct ProjectOopMirClassLayout final
{
    std::size_t typeOrdinal = 0;
    std::string typeKey;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    std::uint64_t headerSize = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::uint64_t baseSubobjectSize = 0;
    std::uint64_t referenceSize = 0;
    std::uint64_t referenceAlignment = 1;
    std::vector<ObjectFieldLayout> fields;
};
}
