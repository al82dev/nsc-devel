#pragma once

#include "nativesharp/Frontend/ProjectOopMirAbiPassing.h"

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
struct ProjectOopMirStructLayout final
{
    std::size_t typeOrdinal = 0;
    std::string typeKey;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    bool isUnion = false;
    std::uint64_t pack = 0;
    std::uint64_t minimumAlignment = 0;
    bool hasExplicitFieldOffsets = false;
    ProjectOopMirAbiPassing abiPassing = ProjectOopMirAbiPassing::Direct;
    bool isImported = false;
    bool requiresDrop = false;
    std::string dropGlueEntryName;
    std::vector<ObjectFieldLayout> fields;
};
}
