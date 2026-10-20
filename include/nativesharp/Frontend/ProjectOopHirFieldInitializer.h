#pragma once

#include "nativesharp/Frontend/ProjectOopHirNode.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>


namespace nativesharp::frontend
{
struct ProjectOopHirFieldInitializer final
{
    std::size_t memberOrdinal = 0;
    std::size_t declaringTypeOrdinal = 0;
    bool isStatic = false;
    bool isConst = false;
    std::unique_ptr<ProjectOopHirNode> value;
};
}
