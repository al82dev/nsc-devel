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
            auto node = makeNode(ProjectOopHirNodeKind::Break, statement, "builtin:void");
            attachStructuredExit(*node);
            node->stringValue = statement->text();
            if (statement->first() != nullptr)
            {
                const MethodLoopBreakBinding* binding = methodBodyModel_ != nullptr
                    ? methodBodyModel_->loopBreakBinding(statement) : nullptr;
                if (binding == nullptr)
                {
                    error = "T07-F05 break value has no loop-expression semantic binding";
                    return nullptr;
                }
                auto value = lowerExpression(statement->first(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                if (binding->conversion.exists() &&
                    binding->conversion.kind != ProjectConversionKind::Identity)
                {
                    value = wrapImplicitConversion(
                        std::move(value), binding->conversion, statement->first());
                }
                node->children.push_back(std::move(value));
                node->elementTypeKey = binding->resultTypeKey;
            }
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
            auto node = makeNode(ProjectOopHirNodeKind::Return, statement, "builtin:void");
            attachStructuredExit(*node);
            if (statement->first() != nullptr)
            {
                bool referenceReturn = false;
                bool valueReturningCallable = false;
                std::string declaredTypeKey;
                sema::OwnershipKind declaredOwnership = sema::OwnershipKind::None;
                if (memberModel_ != nullptr && memberOrdinal < memberModel_->members().size())
                {
                    const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
                    referenceReturn = member.isReferenceReturn;
                    valueReturningCallable = member.kind == ClassMemberKind::Method;
                    declaredTypeKey = member.declaredTypeKey;
                    declaredOwnership = member.declaredOwnership;
                }
                else
                {
                    const std::size_t callableBase =
                        memberModel_ != nullptr ? memberModel_->members().size() : 0;
                    const std::size_t functionOrdinal =
                        memberOrdinal >= callableBase ? memberOrdinal - callableBase :
                        std::numeric_limits<std::size_t>::max();
                    if (namespaceFunctionModel_ == nullptr ||
                        functionOrdinal >= namespaceFunctionModel_->functions().size())
                    {
                        error = "Stage 34 return refers to invalid callable";
                        return nullptr;
                    }
                    const NamespaceFunctionSymbol& function =
                        namespaceFunctionModel_->functions()[functionOrdinal];
                    referenceReturn = function.isReferenceReturn;
                    valueReturningCallable = true;
                    declaredTypeKey = function.declaredTypeKey;
                    declaredOwnership = function.declaredOwnership;
                }

                auto value = lowerExpression(
                    statement->first(), memberOrdinal, referenceReturn, error);
                if (value == nullptr)
                {
                    return nullptr;
                }
                if (valueReturningCallable && declaredTypeKey != "builtin:void" && !referenceReturn)
                {
                    const MethodExpressionTypeBinding* type = expressionType(statement->first());
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        value->typeKey,
                        declaredTypeKey,
                        type != nullptr ? type->integerConstant : std::nullopt);
                    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                    {
                        value = wrapImplicitConversion(std::move(value), conversion, statement->first());
                    }
                    if (declaredOwnership == sema::OwnershipKind::Borrowed)
                    {
                        const MethodBorrowBinding* borrow = borrowBinding(statement->first());
                        if (borrow == nullptr)
                        {
                            error = "Stage 50.8 borrowed return has no frozen borrow binding";
                            return nullptr;
                        }
                        auto borrowed = makeNode(
                            ProjectOopHirNodeKind::BorrowValue,
                            statement->first(),
                            declaredTypeKey);
                        borrowed->borrowKind = borrow->kind;
                        borrowed->localOrdinal = borrow->sourceLocalOrdinal;
                        borrowed->parameterOrdinal = borrow->sourceParameterOrdinal;
                        borrowed->children.push_back(std::move(value));
                        value = std::move(borrowed);
                    }
                }
                if (statement->flag())
                {
                    switch (value->kind)
                    {
                        case ProjectOopHirNodeKind::InstanceCall:
                        case ProjectOopHirNodeKind::StaticCall:
                            value->mustTailCall = true;
                            break;
                        default:
                            error = "T08-F06 validated must_tail return did not lower to a direct call";
                            return nullptr;
                    }
                }
                node->children.push_back(std::move(value));
            }
            return node;
        }
        case ast::AstNodeKind::ThrowStatement:
        {
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

            if (statement->first() != nullptr)
            {
                auto value = lowerExpression(statement->first(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                node->children.push_back(std::move(value));
            }
            return node;
        }

        case ast::AstNodeKind::UsingStatement:
        {
            const MethodUsingBinding* binding = usingBinding(statement);
            const MethodCleanupRegion* region = cleanupRegion(statement);
            const std::optional<std::size_t> cleanupOrdinal = cleanupOrdinalFor(region);
            if (binding == nullptr || !cleanupOrdinal.has_value())
            {
                error = "Stage 45.6 using statement has no frozen using/cleanup binding";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::UsingRegion, statement, "builtin:void");
            node->localOrdinal = binding->resourceLocalOrdinal;
            node->elementTypeKey = binding->resourceTypeKey;
            if (binding->resourceLocalOrdinal < methodBodyModel_->locals().size())
                node->stringValue = methodBodyModel_->locals()[binding->resourceLocalOrdinal].name;
            node->cleanupRegionOrdinal = cleanupOrdinal;
            node->disposeContractMemberOrdinal = binding->disposeContractMemberOrdinal;
            node->disposeImplementationMemberOrdinal = binding->disposeImplementationMemberOrdinal;
            node->interfaceCleanupDispatch = binding->interfaceDispatch;

            auto initializer = lowerExpression(statement->first(), memberOrdinal, false, error);
            if (initializer == nullptr) return nullptr;
            const MethodExpressionTypeBinding* sourceType = expressionType(statement->first());
            const ProjectImplicitConversion conversion = classifyImplicit(
                initializer->typeKey, binding->resourceTypeKey,
                sourceType != nullptr ? sourceType->integerConstant : std::nullopt);
            if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                initializer = wrapImplicitConversion(std::move(initializer), conversion, statement->first());
            node->children.push_back(std::move(initializer));

            activeHirCleanupRegions_.push_back(*cleanupOrdinal);
            auto body = lowerBlock(statement->second(), memberOrdinal, error);
            activeHirCleanupRegions_.pop_back();
            if (body == nullptr) return nullptr;
            node->children.push_back(std::move(body));

            auto continuation = makeNode(ProjectOopHirNodeKind::CleanupContinuation, statement, "builtin:void");
            continuation->cleanupRegionOrdinal = cleanupOrdinal;
            continuation->cleanupContinuationKind = MethodCleanupContinuationKind::Normal;
            node->children.push_back(std::move(continuation));
            return node;
        }

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
}
