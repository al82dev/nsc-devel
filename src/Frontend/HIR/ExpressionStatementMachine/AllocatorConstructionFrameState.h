#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "AllocatorConstructionFrameStage.h"

namespace nativesharp::frontend::internal
{
struct AllocatorConstructionFrameState final
{
    const ObjectConstructionBinding* binding = nullptr;
    AllocatorConstructionFrameStage stage =
        AllocatorConstructionFrameStage::Receiver;
    std::size_t constructionOrdinal = 0U;
    std::size_t computedSize = 0U;
    // EN: alignof(T) passed to IAllocator.Allocate (freeze 3.0, C-08).
    // ES: alignof(T) que recibe IAllocator.Allocate (congelamiento 3.0, C-08).
    std::size_t computedAlignment = 1U;
    std::size_t temporaryOrdinal = 0U;
    std::size_t contextOrdinal = 0U;
    std::optional<std::size_t> allocatorTypeOrdinal;
    bool allocatorIsStruct = false;
    const ast::AstNode* receiverSyntax = nullptr;
    std::vector<const ast::AstNode*> argumentSyntaxes;
    std::unique_ptr<ProjectOopHirNode> receiver;
    std::vector<std::unique_ptr<ProjectOopHirNode>> arguments;
    std::vector<std::size_t> argumentParameterOrdinals;
    std::size_t nextArgument = 0U;
};
}
