#pragma once

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
enum class ProjectCollectionSegmentKind : std::uint8_t
{
    Element,
    Spread
};
}
