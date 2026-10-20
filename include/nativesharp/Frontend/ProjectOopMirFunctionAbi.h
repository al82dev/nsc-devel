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
struct ProjectOopMirFunctionAbi final
{
    ProjectOopMirAbiPassing returnPassing = ProjectOopMirAbiPassing::Direct;
    std::optional<std::size_t> hiddenReturnParameterOrdinal;
    std::vector<ProjectOopMirAbiPassing> parameterPassing;
    std::vector<std::size_t> parameterPhysicalOrdinals;
    std::size_t physicalParameterCount = 0;
};
}
