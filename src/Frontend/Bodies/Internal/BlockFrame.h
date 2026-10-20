#pragma once

namespace nativesharp::frontend
{
struct ProjectMethodBodyModel::BlockFrame final
{
    const ast::AstNode* block = nullptr;
    std::size_t activeScope = 0;
    std::size_t nextStatement = 0;
    std::size_t cleanupDepthAtEntry = 0;
    bool deferCaptureBoundary = false;
    std::optional<OwnershipFlowSnapshot> restoreAfterExit;
};
}
