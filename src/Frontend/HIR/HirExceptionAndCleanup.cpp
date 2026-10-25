#include "HirLoweringSupport.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
std::optional<std::size_t> ProjectOopHirModel::cleanupRegionOrdinal(
    const MethodCleanupRegion* region) const
{
    if (region == nullptr || methodBodyModel_ == nullptr) return std::nullopt;
    const auto regions = methodBodyModel_->cleanupRegions();
    if (regions.empty() || region < regions.data() || region >= regions.data() + regions.size())
        return std::nullopt;
    return static_cast<std::size_t>(region - regions.data());
}

void ProjectOopHirModel::applyStructuredExitBinding(
    ProjectOopHirNode& node,
    const ast::AstNode* syntax) const
{
    const MethodStructuredExitBinding* binding = structuredExitBinding(syntax);
    if (binding == nullptr) return;
    node.cleanupRegionOrdinals = binding->cleanupRegionOrdinals;
    node.targetScopeOrdinal = binding->targetScopeOrdinal;
    switch (binding->kind)
    {
        case MethodStructuredExitKind::Return:
            node.cleanupContinuationKind = MethodCleanupContinuationKind::Return;
            break;
        case MethodStructuredExitKind::Break:
            node.cleanupContinuationKind = MethodCleanupContinuationKind::Break;
            break;
        case MethodStructuredExitKind::Continue:
            node.cleanupContinuationKind = MethodCleanupContinuationKind::Continue;
            break;
    }
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::wrapFullExpressionCleanup(
    std::unique_ptr<ProjectOopHirNode> value,
    const ast::AstNode* syntax,
    bool hasOwnedTemporary)
{
    if (!hasOwnedTemporary) return value;
    auto fullExpression = makeNode(ProjectOopHirNodeKind::FullExpressionCleanup, syntax, value->typeKey);
    fullExpression->children.push_back(std::move(value));
    return fullExpression;
}

// EN: Freeze 3.0 C-11 (found while probing value lifetimes): the result of a call that hands its
//     caller a value to destroy (an owned object or string, or a struct with a destructor or
//     owned fields) leaked when it was discarded (`Make();`) or only used as a receiver
//     (`Make().Count`, `Make().Id()`). Such a temporary now gets a hidden owner that the full
//     expression destroys. A receiver is owned only when its consumer yields a plain scalar,
//     because a reference or a string taken from the temporary could outlive it.
// ES: Congelamiento 3.0 C-11 (hallazgo al sondear lifetimes de valores): el resultado de una
//     llamada que entrega al llamador un valor que destruir (un objeto o string owned, o un
//     struct con destructor o campos owned) se fugaba cuando se descartaba (`Make();`) o solo se
//     usaba como receptor (`Make().Count`, `Make().Id()`). Ese temporal recibe ahora un dueno
//     oculto que la expresion completa destruye. Un receptor solo se posee cuando su consumidor
//     produce un escalar simple, porque una referencia o un string tomado del temporal podria
//     sobrevivirle.
namespace
{
bool hasPlainResult(const ProjectOopHirNode& node)
{
    if (node.referenceResult) return false;
    const std::string& type = node.typeKey;
    return type == "builtin:void" ||
        (type.starts_with("builtin:") && type != "builtin:string" && !type.ends_with("*") &&
         !type.ends_with("[]"));
}

bool consumesReceiver(const ProjectOopHirNode& node)
{
    return !node.children.empty() &&
        (node.kind == ProjectOopHirNodeKind::InstanceCall ||
         node.kind == ProjectOopHirNodeKind::FieldValue ||
         node.kind == ProjectOopHirNodeKind::StringLength ||
         node.kind == ProjectOopHirNodeKind::StringElementValue);
}
}

bool ProjectOopHirModel::isFreshDroppableTemporary(const ProjectOopHirNode* node) const
{
    if (node == nullptr || node->referenceResult || memberModel_ == nullptr) return false;
    const bool producer = node->kind == ProjectOopHirNodeKind::InstanceCall ||
        node->kind == ProjectOopHirNodeKind::StaticCall ||
        node->kind == ProjectOopHirNodeKind::DelegateInvoke ||
        node->kind == ProjectOopHirNodeKind::StringConcat ||
        node->kind == ProjectOopHirNodeKind::StringSubstring;
    if (!producer) return false;
    if (memberModel_->typeRequiresValueDrop(node->typeKey)) return true;
    if (!isReferenceType(node->typeKey) && node->typeKey != "builtin:string") return false;
    const MethodExpressionTypeBinding* binding = expressionType(node->syntax);
    return binding != nullptr && binding->ownership == sema::OwnershipKind::Owned &&
        binding->ownershipTransfer && !binding->ownershipSourceLocalOrdinal.has_value() &&
        !binding->ownershipSourceParameterOrdinal.has_value();
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::ownTemporary(
    std::unique_ptr<ProjectOopHirNode> value)
{
    auto temporary = makeNode(ProjectOopHirNodeKind::OwnedTemporaryBorrow, value->syntax, value->typeKey);
    temporary->localOrdinal = nextSyntheticLocalOrdinal_++;
    temporary->cleanupRegionOrdinal = nextSyntheticCleanupRegionOrdinal_++;
    temporary->children.push_back(std::move(value));
    return temporary;
}

bool ProjectOopHirModel::ownReceiverTemporary(std::unique_ptr<ProjectOopHirNode>& consumer)
{
    if (!consumesReceiver(*consumer) || !hasPlainResult(*consumer)) return false;
    std::unique_ptr<ProjectOopHirNode>* receiver = &consumer->children.front();
    if (*receiver != nullptr && (*receiver)->kind == ProjectOopHirNodeKind::TemporaryAddress &&
        (*receiver)->children.size() == 1)
        receiver = &(*receiver)->children.front();
    if (!isFreshDroppableTemporary(receiver->get())) return false;
    *receiver = ownTemporary(std::move(*receiver));
    const ast::AstNode* syntax = consumer->syntax;
    consumer = wrapFullExpressionCleanup(std::move(consumer), syntax, true);
    return true;
}

void ProjectOopHirModel::ownDroppableTemporaries(ProjectOopHirNode* root)
{
    std::vector<ProjectOopHirNode*> pending{root};
    while (!pending.empty())
    {
        ProjectOopHirNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr) continue;
        for (std::unique_ptr<ProjectOopHirNode>& child : node->children)
        {
            if (child == nullptr) continue;
            if (node->kind == ProjectOopHirNodeKind::ExpressionStatement &&
                isFreshDroppableTemporary(child.get()))
            {
                ProjectOopHirNode* producer = child.get();
                const ast::AstNode* syntax = producer->syntax;
                child = wrapFullExpressionCleanup(ownTemporary(std::move(child)), syntax, true);
                pending.push_back(producer);
                continue;
            }
            ProjectOopHirNode* original = child.get();
            ownReceiverTemporary(child);
            pending.push_back(original);
        }
    }
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExceptionOrExitStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    const auto attachStructuredExit = [&](ProjectOopHirNode& node)
    {
        applyStructuredExitBinding(node, statement);
    };

    switch (statement->kind())
    {
        case ast::AstNodeKind::BreakStatement:
        {
            if (statement->first() != nullptr)
                return lowerExitValueStatement(statement, memberOrdinal, error);
            auto node = makeNode(ProjectOopHirNodeKind::Break, statement, "builtin:void");
            attachStructuredExit(*node);
            node->stringValue = statement->text();
            return node;
        }
        case ast::AstNodeKind::ContinueStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::Continue, statement, "builtin:void");
            attachStructuredExit(*node);
            node->stringValue = statement->text();
            return node;
        }
        case ast::AstNodeKind::ReturnStatement:
        {
            if (statement->first() != nullptr)
                return lowerExitValueStatement(statement, memberOrdinal, error);
            auto node = makeNode(ProjectOopHirNodeKind::Return, statement, "builtin:void");
            attachStructuredExit(*node);
            return node;
        }
        case ast::AstNodeKind::ThrowStatement:
        {
            if (statement->first() != nullptr)
                return lowerExitValueStatement(statement, memberOrdinal, error);
            const MethodExceptionBinding* binding = exceptionBinding(statement);
            if (binding == nullptr)
            {
                error = "Stage 45.6 throw statement has no Stage 45.2 exception binding";
                return nullptr;
            }
            auto node = makeNode(
                binding->kind == MethodExceptionOperationKind::Rethrow
                    ? ProjectOopHirNodeKind::Rethrow
                    : ProjectOopHirNodeKind::Throw,
                statement, "builtin:void");
            node->typeKey = binding->exceptionTypeKey.empty() ? "builtin:void" : binding->exceptionTypeKey;
            node->exceptionTypeOrdinal = binding->exceptionTypeOrdinal;
            node->cleanupContinuationKind = MethodCleanupContinuationKind::Exception;

            const std::size_t retainedDepth = activeHirExceptionHandlers_.empty()
                ? 0
                : activeHirExceptionHandlers_.back().retainedCleanupDepth;
            for (std::size_t index = activeHirCleanupRegions_.size(); index > retainedDepth; --index)
                node->cleanupRegionOrdinals.push_back(activeHirCleanupRegions_[index - 1]);
            if (!activeHirExceptionHandlers_.empty())
                node->exceptionTargetRegionOrdinal = activeHirExceptionHandlers_.back().hirRegionOrdinal;

            return node;
        }

        case ast::AstNodeKind::UsingStatement:
            return lowerUsingStatement(statement, memberOrdinal, error);

        case ast::AstNodeKind::TryStatement:
            return lowerTryStatement(statement, memberOrdinal, error);
        default:
            error = "non-exception/exit statement reached HIR exception lowering";
            return nullptr;
    }
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerArrayDeleteExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    const MethodExpressionTypeBinding& operandType,
    std::string& error)
{
    if (!ProjectTypeConversion::isNativeArray(operandType.typeKey))
    {
        error = "Stage 41 delete[] requires a native dynamic array reference";
        return nullptr;
    }
    const MethodCoreBinding* elementBinding = coreBinding(expression->first());
    const bool directElement = elementBinding != nullptr &&
        (elementBinding->kind == MethodCoreOperationKind::ArrayElement ||
         elementBinding->kind == MethodCoreOperationKind::RawElement);
    auto operand = lowerExpression(expression->first(), memberOrdinal, directElement, error);
    if (operand == nullptr) return nullptr;
    auto node = makeNode(ProjectOopHirNodeKind::Deallocation, expression, "builtin:void");
    node->dynamicDestruction = false;
    // The array allocation is always released by delete[]. Element ownership is a
    // separate axis carried by the operand binding. In particular, a plain T[] or
    // borrowed T[] owns no T elements even when its containing class manually
    // releases the backing buffer in its destructor.
    node->storageOwnership = operandType.storageOwnership;
    // A jagged row is itself an owning native-array reference. Carrying the slot
    // address lets LLVM null it before recursive destruction, so an explicit row
    // delete followed by deletion of the outer array can never double-drop it.
    node->booleanValue = directElement;
    node->elementTypeKey = operandType.typeKey;
    node->children.push_back(std::move(operand));
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerScalarDeleteExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    const MethodExpressionTypeBinding& operandType,
    std::string& error)
{
    const bool rawPointer = operandType.typeKey.ends_with("*");
    const bool nativeString = operandType.typeKey == "builtin:string";
    const bool deletable = operandType.typeKey == "builtin:null" ||
        isReferenceType(operandType.typeKey) || rawPointer || nativeString;
    // Stage 52 generic collections: a container's cleanup code (e.g. List<T>.Clear())
    // must delete each stored element uniformly, but T may monomorphize to a plain
    // value type (int, bool, a struct, ...) that never owned heap memory in the first
    // place. Rather than forbid `delete` on such values -- which would make it
    // impossible to write ONE generic Clear()/destructor body that works for every T --
    // treat `delete <value-typed expression>;` as a legal no-op: the operand is still
    // evaluated (so e.g. an array-index bounds check still fires), but no runtime
    // deallocation call is emitted. This mirrors how `delete[] arr;` already tolerates
    // arrays of any element type. `delete` on a genuinely non-deletable receiver
    // (unknown/void expression, etc.) is caught earlier by expressionType() returning a
    // real value type here -- there is no way to reach this branch with something that
    // isn't a fully-typed value expression.
    const MethodCoreBinding* elementBinding = coreBinding(expression->first());
    const bool directElement = elementBinding != nullptr &&
        (elementBinding->kind == MethodCoreOperationKind::ArrayElement ||
         elementBinding->kind == MethodCoreOperationKind::RawElement);
    const bool clearsElementSlot = directElement &&
        (isReferenceType(operandType.typeKey) || nativeString);
    auto operand = lowerExpression(
        expression->first(), memberOrdinal, clearsElementSlot, error);
    if (operand == nullptr) return nullptr;
    auto node = makeNode(ProjectOopHirNodeKind::Deallocation, expression, "builtin:void");
    node->dynamicDestruction = isReferenceType(operandType.typeKey) && !nativeString;
    node->valueTypeNoOpDeletion = !deletable;
    if (!node->valueTypeNoOpDeletion)
    {
        node->objectLifetimeOwnership = node->dynamicDestruction
            ? sema::OwnershipKind::Owned : sema::OwnershipKind::None;
        node->storageOwnership = sema::OwnershipKind::Owned;
    }
    // Deleting a directly addressable reference element is an ownership operation on the
    // slot, not merely on a copied pointer.  Carry its address and type so LLVM can load
    // and clear it before destruction.  This prevents a later release-before-store from
    // observing a dangling owner and destroying the same allocation twice.
    node->booleanValue = clearsElementSlot;
    node->elementTypeKey = operandType.typeKey;
    node->children.push_back(std::move(operand));
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDeleteExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error)
{
    const MethodExpressionTypeBinding* operandType = expressionType(expression->first());
    if (operandType == nullptr)
    {
        error = "Stage 41 cannot determine delete operand type";
        return nullptr;
    }
    return expression->flag()
        ? lowerArrayDeleteExpression(expression, memberOrdinal, *operandType, error)
        : lowerScalarDeleteExpression(expression, memberOrdinal, *operandType, error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDestroyExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error)
{
    const MethodExpressionTypeBinding* operandType = expressionType(expression->first());
    if (operandType == nullptr)
    {
        error = "item 319 cannot determine destroy operand type";
        return nullptr;
    }
    auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
    if (operand == nullptr) return nullptr;
    bool dynamicDestruction = isReferenceType(operandType->typeKey);
    if (!dynamicDestruction && operandType->typeKey.ends_with('*'))
    {
        const std::string pointee = operandType->typeKey.substr(
            0, operandType->typeKey.size() - 1);
        dynamicDestruction = isReferenceType(pointee);
    }
    auto node = makeNode(
        ProjectOopHirNodeKind::ObjectDestruction, expression, "builtin:void");
    node->dynamicDestruction = dynamicDestruction;
    node->elementTypeKey = operandType->typeKey;
    node->objectLifetimeOwnership = sema::OwnershipKind::Owned;
    node->storageOwnership = sema::OwnershipKind::Borrowed;
    node->children.push_back(std::move(operand));
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDestructionExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error)
{
    switch (expression->kind())
    {
        case ast::AstNodeKind::DeleteExpression:
            return lowerDeleteExpression(expression, memberOrdinal, error);
        case ast::AstNodeKind::DestroyExpression:
            return lowerDestroyExpression(expression, memberOrdinal, error);
        default:
            error = "non-destruction expression reached HIR destruction lowering";
            return nullptr;
    }
}
}
