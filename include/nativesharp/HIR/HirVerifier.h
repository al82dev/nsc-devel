#pragma once

#include <string>
#include <vector>

namespace nativesharp::sema
{
class Type;
}

namespace nativesharp::hir::internal
{
struct VerificationTask;
}

namespace nativesharp::hir
{
class HirNode;

class HirVerifier final
{
public:
    bool verify(const HirNode* root);
    const std::string& error() const noexcept;

private:
    bool verifyNode(const HirNode* node, sema::Type* functionReturnType, int loopDepth);
    bool verifyNodeKind(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks,
        sema::Type* functionReturnType,
        int loopDepth);
    bool verifyGlobalDeclarationNode(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks,
        sema::Type* functionReturnType,
        int loopDepth);
    bool verifyCallableDeclarationNode(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks,
        sema::Type* functionReturnType,
        int loopDepth);
    bool verifyVariableDeclarationNode(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks,
        sema::Type* functionReturnType,
        int loopDepth);
    bool verifyFunctionDeclarationNode(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks);
    bool verifyControlFlowNode(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks,
        sema::Type* functionReturnType,
        int loopDepth);
    bool verifyExitOrStatementNode(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks,
        sema::Type* functionReturnType,
        int loopDepth);
    bool verifyValueNode(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks,
        sema::Type* functionReturnType,
        int loopDepth);
    bool verifyAccessNode(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks,
        sema::Type* functionReturnType,
        int loopDepth);
    bool verifyCallNode(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks,
        sema::Type* functionReturnType,
        int loopDepth);
    bool verifyIndexAddressNode(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks,
        sema::Type* functionReturnType,
        int loopDepth);
    bool verifyMutationOrAllocationNode(
        const HirNode* node,
        std::vector<internal::VerificationTask>& tasks,
        sema::Type* functionReturnType,
        int loopDepth);
    bool verifyPointerTo(const HirNode* address, const sema::Type* pointee);
    bool fail(const HirNode* node, std::string message);

    std::string error_;
};
}
