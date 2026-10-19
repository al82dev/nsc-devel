#include "HirLoweringSupport.h"

#include <limits>
#include <optional>
#include <string>
#include <utility>

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

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExceptionOrExitStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    const auto cleanupOrdinalFor = [&](
        const MethodCleanupRegion* region) -> std::optional<std::size_t>
    {
        return cleanupRegionOrdinal(region);
    };
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
        {
            auto node = makeNode(ProjectOopHirNodeKind::TryRegion, statement, "builtin:void");
            node->hirRegionOrdinal = nextHirRegionOrdinal_++;
            const MethodCleanupRegion* region = cleanupRegion(statement);
            const std::optional<std::size_t> cleanupOrdinal = cleanupOrdinalFor(region);
            if (cleanupOrdinal.has_value())
            {
                node->cleanupRegionOrdinal = cleanupOrdinal;
                activeHirCleanupRegions_.push_back(*cleanupOrdinal);
            }

            const bool hasCatches = !statement->children().empty();
            if (hasCatches)
            {
                activeHirExceptionHandlers_.push_back(HirExceptionHandlerContext{
                    *node->hirRegionOrdinal, activeHirCleanupRegions_.size()});
            }
            auto protectedBlock = lowerBlock(statement->first(), memberOrdinal, error);
            if (hasCatches) activeHirExceptionHandlers_.pop_back();
            if (protectedBlock == nullptr)
            {
                if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                return nullptr;
            }
            node->children.push_back(std::move(protectedBlock));

            for (const ast::AstNode* catchClause : statement->children())
            {
                const MethodExceptionBinding* catchBinding = exceptionBinding(catchClause);
                if (catchBinding == nullptr || catchBinding->kind != MethodExceptionOperationKind::Catch)
                {
                    if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                    error = "Stage 45.6 catch clause has no Stage 45.2 catch binding";
                    return nullptr;
                }
                auto catchNode = makeNode(ProjectOopHirNodeKind::CatchRegion, catchClause, "builtin:void");
                catchNode->elementTypeKey = catchBinding->exceptionTypeKey;
                catchNode->exceptionTypeOrdinal = catchBinding->exceptionTypeOrdinal;
                catchNode->catchAll = catchBinding->catchAll;
                catchNode->catchHasFilter = catchBinding->hasFilter;
                catchNode->catchExhaustive = catchBinding->exhaustive;
                catchNode->catchMatchedErrorTypeKeys = catchBinding->matchedErrorTypeKeys;
                if (const MethodLocalSymbol* local = localSymbol(catchClause); local != nullptr)
                {
                    catchNode->localOrdinal = static_cast<std::size_t>(
                        local - methodBodyModel_->locals().data());
                    catchNode->stringValue = local->name;
                }
                if (catchBinding->hasFilter)
                {
                    auto catchFilter = lowerExpression(
                        catchClause->second(), memberOrdinal, false, error);
                    if (catchFilter == nullptr)
                    {
                        if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                        return nullptr;
                    }
                    catchNode->children.push_back(std::move(catchFilter));
                }
                auto catchBody = lowerBlock(catchClause->first(), memberOrdinal, error);
                if (catchBody == nullptr)
                {
                    if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                    return nullptr;
                }
                catchNode->children.push_back(std::move(catchBody));
                node->children.push_back(std::move(catchNode));
            }

            if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();

            if (statement->second() != nullptr)
            {
                if (!cleanupOrdinal.has_value())
                {
                    error = "Stage 45.6 finally block has no cleanup region";
                    return nullptr;
                }
                auto finallyNode = makeNode(ProjectOopHirNodeKind::FinallyRegion, statement->second(), "builtin:void");
                finallyNode->cleanupRegionOrdinal = cleanupOrdinal;
                auto finallyBody = lowerBlock(statement->second(), memberOrdinal, error);
                if (finallyBody == nullptr) return nullptr;
                finallyNode->children.push_back(std::move(finallyBody));
                auto continuation = makeNode(ProjectOopHirNodeKind::CleanupContinuation, statement, "builtin:void");
                continuation->cleanupRegionOrdinal = cleanupOrdinal;
                continuation->cleanupContinuationKind = MethodCleanupContinuationKind::Normal;
                finallyNode->children.push_back(std::move(continuation));
                node->children.push_back(std::move(finallyNode));
            }
            return node;
        }
        default:
            error = "non-exception/exit statement reached HIR exception lowering";
            return nullptr;
    }
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDestructionExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error)
{
    switch (expression->kind())
    {
        case ast::AstNodeKind::DeleteExpression:
        {
            const MethodExpressionTypeBinding* operandType = expressionType(expression->first());
            if (operandType == nullptr)
            {
                error = "Stage 41 cannot determine delete operand type";
                return nullptr;
            }
            if (expression->flag())
            {
                if (!ProjectTypeConversion::isNativeArray(operandType->typeKey))
                {
                    error = "Stage 41 delete[] requires a native dynamic array reference";
                    return nullptr;
                }
                const MethodCoreBinding* elementBinding = coreBinding(expression->first());
                const bool directElement = elementBinding != nullptr &&
                    (elementBinding->kind == MethodCoreOperationKind::ArrayElement ||
                     elementBinding->kind == MethodCoreOperationKind::RawElement);
                auto operand = lowerExpression(
                    expression->first(), memberOrdinal, directElement, error);
                if (operand == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::Deallocation, expression, "builtin:void");
                node->dynamicDestruction = false;
                // The array allocation is always released by delete[]. Element ownership is a
                // separate axis carried by the operand binding. In particular, a plain T[] or
                // borrowed T[] owns no T elements even when its containing class manually
                // releases the backing buffer in its destructor.
                node->storageOwnership = operandType->storageOwnership;
                // A jagged row is itself an owning native-array reference. Carrying the slot
                // address lets LLVM null it before recursive destruction, so an explicit row
                // delete followed by deletion of the outer array can never double-drop it.
                node->booleanValue = directElement;
                node->elementTypeKey = operandType->typeKey;
                node->children.push_back(std::move(operand));
                return node;
            }
            const bool rawPointer = operandType->typeKey.ends_with("*");
            const bool nativeString = operandType->typeKey == "builtin:string";
            const bool deletable = operandType->typeKey == "builtin:null" ||
                isReferenceType(operandType->typeKey) || rawPointer || nativeString;
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
                (isReferenceType(operandType->typeKey) || nativeString);
            auto operand = lowerExpression(
                expression->first(), memberOrdinal, clearsElementSlot, error);
            if (operand == nullptr)
            {
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::Deallocation, expression, "builtin:void");
            node->dynamicDestruction = isReferenceType(operandType->typeKey) && !nativeString;
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
            node->elementTypeKey = operandType->typeKey;
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::DestroyExpression:
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
        default:
            error = "non-destruction expression reached HIR destruction lowering";
            return nullptr;
    }
}
}
