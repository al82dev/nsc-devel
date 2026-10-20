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
// Item 398: a single MIR-owned catalogue of every closed generic identity.  Consumers no longer
// need to rediscover which functions, static fields, and runtime tables belong to one
// specialization by scanning three unrelated collections.
struct ProjectOopMirGenericSpecialization final
{
    std::string key;
    std::vector<std::size_t> functionIndices;
    std::vector<std::size_t> staticFieldIndices;
    std::vector<std::size_t> dispatchTableIndices;
};
}
