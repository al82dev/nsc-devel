#pragma once

#include "HirAggregateSlot.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::frontend::internal
{
struct HirAggregateFrame final
{
    const ast::AstNode* syntax = nullptr;
    std::string targetTypeKey;
    std::uint64_t baseOffset = 0;
    std::vector<HirAggregateSlot> slots;
    const MethodAggregateInitializerBinding* binding = nullptr;
    std::size_t entryIndex = 0;
    bool initialized = false;
};
}
