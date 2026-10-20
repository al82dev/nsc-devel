#pragma once

#include "nativesharp/Frontend/ProjectOopMirNonlocalExit.h"

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
struct ProjectOopMirExceptionFlow final
{
    bool requiresPersonality = false;
    std::vector<std::size_t> protectedRegionOrdinals;
    std::size_t catchRegionCount = 0;
    std::size_t finallyRegionCount = 0;
    std::vector<ProjectOopMirNonlocalExit> nonlocalExits;
};
}
