#include "HirLoweringSupport.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

namespace nativesharp::frontend
{
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerBlock(
    const ast::AstNode* block,
    std::size_t memberOrdinal,
    std::string& error)
{
    if (block == nullptr || block->kind() != ast::AstNodeKind::BlockStatement)
    {
        error = "Stage 34 OOP HIR requires block statement body";
        return nullptr;
    }
    auto result = makeNode(ProjectOopHirNodeKind::Block, block, "builtin:void");
    const std::size_t cleanupDepthAtEntry = activeHirCleanupRegions_.size();
    for (const ast::AstNode* statement : block->children())
    {
        std::unique_ptr<ProjectOopHirNode> lowered = lowerStatement(statement, memberOrdinal, error);
        if (lowered == nullptr)
        {
            activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
            return nullptr;
        }
        if (lowered->kind == ProjectOopHirNodeKind::LocalDeclaration &&
            lowered->cleanupRegionOrdinal.has_value())
        {
            activeHirCleanupRegions_.push_back(*lowered->cleanupRegionOrdinal);
        }
        result->children.push_back(std::move(lowered));
    }
    activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
    return result;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDeconstructionSource(
    const ast::AstNode* bindingSyntax,
    const ast::AstNode* sourceSyntax,
    std::size_t memberOrdinal,
    std::string& error)
{
    const MethodDeconstructionBinding* binding = methodBodyModel_ != nullptr
        ? methodBodyModel_->deconstructionBinding(bindingSyntax) : nullptr;
    if (binding == nullptr)
    {
        error = "T02-F03 deconstruction has no semantic source binding";
        return nullptr;
    }
    if (!binding->selectedMemberOrdinal.has_value())
        return lowerExpression(sourceSyntax, memberOrdinal, false, error);

    auto receiver = lowerReceiver(
        sourceSyntax, binding->sourceTypeKey, memberOrdinal, error);
    if (receiver == nullptr) return nullptr;
    auto call = makeNode(
        ProjectOopHirNodeKind::InstanceCall, sourceSyntax, binding->tupleTypeKey);
    call->memberOrdinal = *binding->selectedMemberOrdinal;
    call->children.push_back(std::move(receiver));
    return call;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerReceiver(
    const ast::AstNode* receiverSyntax,
    std::string_view receiverTypeKey,
    std::size_t memberOrdinal,
    std::string& error)
{
    if (receiverTypeKey.empty())
    {
        error = "Stage 40 receiver has no semantic class/interface/struct type";
        return nullptr;
    }
    if (receiverSyntax == nullptr || receiverSyntax->kind() == ast::AstNodeKind::BaseExpression)
    {
        if (currentLambda_ != nullptr)
        {
            const std::optional<std::size_t> capture = currentLambdaThisCapture();
            if (!capture.has_value())
            {
                error = "Stage 43 implicit instance receiver has no captured this";
                return nullptr;
            }
            auto node = makeNode(
                isStructValueType(receiverTypeKey)
                    ? ProjectOopHirNodeKind::ClosureCaptureAddress
                    : ProjectOopHirNodeKind::ClosureCaptureValue,
                receiverSyntax,
                std::string(receiverTypeKey));
            node->integerValue = *capture;
            return node;
        }
        return makeNode(ProjectOopHirNodeKind::ThisValue, receiverSyntax, std::string(receiverTypeKey));
    }
    // Struct instance methods receive the address of existing value storage.  Unlike a class
    // receiver, lowering a struct receiver as a value here would copy it and mutations of `this`
    // would be lost.  Stage 40 therefore requires an addressable receiver and preserves that
    // address explicitly into MIR/LLVM.
    //
    // C6 "struct members": ... and a struct temporary has no address until one is made for it.
    // Before C6 that did not come up much, because a struct expression was almost always a local
    // or a field. Operators change that completely -- `(a + b).x`, `Cross(u, v).z`,
    // `(-v).LengthSquared`, `new Vector3(1,2,3)[0]` are ordinary math, and every one of them
    // reaches a member of a value that lives nowhere. Measured before this: the operator path had
    // no addressability check at all and produced IR the verifier rejected ("GEP base pointer is
    // not a vector", GEPing into a loaded `[12 x i8]`), while the call path reported "temporary
    // struct values are not addressable".
    //
    // So a temporary receiver is MATERIALIZED: given a home, and the member reached through it.
    // Anything that is naturally addressable keeps its own address, unchanged -- this adds a slot
    // only where there was none to begin with.
    if (isStructValueType(receiverTypeKey) && !isNaturallyAddressable(receiverSyntax))
    {
        auto value = lowerExpression(receiverSyntax, memberOrdinal, false, error);
        if (value == nullptr) return nullptr;
        auto node = makeNode(
            ProjectOopHirNodeKind::TemporaryAddress, receiverSyntax, std::string(receiverTypeKey));
        node->elementTypeKey = std::string(receiverTypeKey);
        node->children.push_back(std::move(value));
        return node;
    }
    return lowerExpression(receiverSyntax, memberOrdinal, isStructValueType(receiverTypeKey), error);
}
}
