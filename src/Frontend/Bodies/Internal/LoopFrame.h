#pragma once

namespace nativesharp::frontend
{
struct ProjectMethodBodyModel::LoopFrame final
{
    const ast::AstNode* statement = nullptr;
    std::size_t scopeOrdinal = 0;
    LoopStage stage = LoopStage::Start;
    bool isDoWhile = false;
    bool conditionAlwaysTrue = false;
    OwnershipFlowSnapshot loopEntry;
    std::size_t localCountAtBodyEntry = 0;
    std::size_t callCountAtEntry = 0;
    std::size_t namespaceCallCountAtEntry = 0;
    std::size_t delegateCountAtEntry = 0;
    std::size_t eventCountAtEntry = 0;
    const ClassMemberSymbol* loopMethod = nullptr;
    std::optional<OwnershipFlowSnapshot> restoreAfterExit;
    static void validateViewOutputBackEdge(
        ProjectMethodBodyModel& model, const ast::AstNode* statement,
        std::size_t firstCall, std::size_t firstNamespaceCall, std::size_t firstDelegate, std::size_t firstEvent,
        std::size_t callable,
        diagnostics::DiagnosticEngine& diagnostics);
};
}
