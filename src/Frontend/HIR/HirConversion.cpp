#include "HirLoweringSupport.h"

#include "nativesharp/Frontend/ProjectConversionClassifier.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::wrapImplicitConversion(
    std::unique_ptr<ProjectOopHirNode> value,
    const ProjectImplicitConversion& conversion,
    const ast::AstNode* syntax)
{
    // `never -> T` is a semantic coercion only. There is no source value to convert and no
    // destination value to materialize, so HIR preserves the divergent node itself. Parents use
    // their declared result type for reachable paths and LLVM observes the terminating block.
    if (conversion.kind == ProjectConversionKind::Never)
    {
        return value;
    }
    if ((conversion.kind == ProjectConversionKind::UserDefinedImplicit ||
         conversion.kind == ProjectConversionKind::UserDefinedExplicit) &&
        conversion.userDefinedMemberOrdinal.has_value())
    {
        auto node = makeNode(ProjectOopHirNodeKind::StaticCall, syntax, conversion.targetTypeKey);
        node->memberOrdinal = conversion.userDefinedMemberOrdinal;
        node->children.push_back(std::move(value));
        return node;
    }
    // NS304-T11-F02: a native array view (`Span<T>`/`ReadOnlySpan<T>`) reuses the SpanFromArray
    // core lowering (base+length, no temporary array). Two children signal the WHOLE array: the
    // lowering derives count from the object's own length header.
    if (conversion.kind == ProjectConversionKind::View &&
        ProjectTypeConversion::isNativeArray(conversion.sourceTypeKey))
    {
        if (const std::optional<std::string> element =
                ProjectTypeConversion::nativeArrayElementType(conversion.sourceTypeKey))
        {
            const auto terminal = [](std::string_view key) -> std::string_view
            {
                const std::size_t separator = key.find_last_of(".:+");
                return separator == std::string_view::npos ? key : key.substr(separator + 1U);
            };
            const bool readOnly =
                terminal(conversion.targetTypeKey).starts_with("ReadOnlySpan__g1__");
            auto node = makeNode(
                readOnly ? ProjectOopHirNodeKind::ReadOnlySpanFromArray
                         : ProjectOopHirNodeKind::SpanFromArray,
                syntax, conversion.targetTypeKey);
            node->elementTypeKey = *element;
            node->children.push_back(std::move(value));
            auto zero = makeNode(ProjectOopHirNodeKind::IntegerConstant, syntax, "builtin:int");
            zero->integerValue = 0;
            node->children.push_back(std::move(zero));
            return node;
        }
    }
    // NS304-T11-F02: a native string view (`StringView`) reuses the StringViewFromString core
    // lowering (UTF-8 base+length). Two children signal the WHOLE string.
    if (conversion.kind == ProjectConversionKind::View &&
        conversion.sourceTypeKey == "builtin:string")
    {
        auto node = makeNode(
            ProjectOopHirNodeKind::StringViewFromString, syntax, conversion.targetTypeKey);
        node->elementTypeKey = "builtin:string";
        node->children.push_back(std::move(value));
        auto zero = makeNode(ProjectOopHirNodeKind::IntegerConstant, syntax, "builtin:int");
        zero->integerValue = 0;
        node->children.push_back(std::move(zero));
        return node;
    }
    auto node = makeNode(ProjectOopHirNodeKind::Conversion, syntax, conversion.targetTypeKey);
    node->conversion = conversion;
    node->children.push_back(std::move(value));
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::wrapCallArgumentBorrow(
    std::unique_ptr<ProjectOopHirNode> value,
    const ast::AstNode* argumentSyntax,
    std::size_t targetMemberOrdinal,
    std::size_t targetParameterOrdinal,
    bool& hasOwnedTemporary)
{
    const MethodBorrowBinding* borrow = borrowBinding(argumentSyntax);
    if (borrow == nullptr || borrow->targetMemberOrdinal != targetMemberOrdinal ||
        borrow->targetParameterOrdinal != targetParameterOrdinal)
    {
        return value;
    }

    if (borrow->kind == MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter)
    {
        return wrapFreshOwnedTemporaryBorrow(
            std::move(value), argumentSyntax, hasOwnedTemporary);
    }

    auto borrowed = makeNode(ProjectOopHirNodeKind::BorrowValue, argumentSyntax, value->typeKey);
    borrowed->borrowKind = borrow->kind;
    borrowed->localOrdinal = borrow->sourceLocalOrdinal;
    borrowed->parameterOrdinal = borrow->sourceParameterOrdinal;
    borrowed->children.push_back(std::move(value));
    value = std::move(borrowed);

    return value;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::wrapFreshOwnedTemporaryBorrow(
    std::unique_ptr<ProjectOopHirNode> value,
    const ast::AstNode* argumentSyntax,
    bool& hasOwnedTemporary)
{
    auto borrowed = makeNode(ProjectOopHirNodeKind::BorrowValue, argumentSyntax, value->typeKey);
    borrowed->borrowKind = MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter;
    borrowed->children.push_back(std::move(value));
    value = std::move(borrowed);

    // The slot is both the owner and the exactly-once guard. It is initialized only after the
    // fresh expression succeeds; from that point until FullExpressionCleanup finishes, every
    // potentially-throwing nested call sees the synthetic cleanup as active.
    auto temporary = makeNode(ProjectOopHirNodeKind::OwnedTemporaryBorrow, argumentSyntax, value->typeKey);
    temporary->localOrdinal = nextSyntheticLocalOrdinal_++;
    temporary->cleanupRegionOrdinal = nextSyntheticCleanupRegionOrdinal_++;
    temporary->children.push_back(std::move(value));
    hasOwnedTemporary = true;
    return temporary;
}

ProjectImplicitConversion ProjectOopHirModel::classifyImplicit(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey,
    std::optional<ProjectIntegerConstant> integerConstant) const
{
    if (inheritanceModel_ != nullptr)
    {
        return ProjectConversionClassifier(*inheritanceModel_, interfaceModel_, memberModel_).classifyImplicit(
            sourceTypeKey, targetTypeKey, integerConstant);
    }
    return ProjectTypeConversion::classifyImplicit(
        sourceTypeKey, targetTypeKey, integerConstant, isReferenceType(targetTypeKey));
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerConversionExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool,
    std::string& error,
    const MethodExpressionTypeBinding*,
    std::string_view resultType)
{
    if (expression->kind() == ast::AstNodeKind::TypeTestExpression ||
        expression->kind() == ast::AstNodeKind::SafeCastExpression)
    {
        const MethodRuntimeTypeBinding* binding = runtimeTypeBinding(expression);
        if (binding == nullptr)
        {
            error = "Stage 39 runtime type operation lost its semantic binding";
            return nullptr;
        }
        auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
        if (operand == nullptr)
        {
            return nullptr;
        }
        auto node = makeNode(
            binding->kind == MethodRuntimeTypeOperationKind::TypeTest
                ? ProjectOopHirNodeKind::RuntimeTypeTest
                : ProjectOopHirNodeKind::SafeReferenceCast,
            expression,
            std::string(resultType));
        node->targetTypeOrdinal = binding->targetTypeOrdinal;
        node->targetIsInterface = binding->targetIsInterface;
        node->children.push_back(std::move(operand));
        return node;
    }

    const MethodRuntimeTypeBinding* runtimeBinding = runtimeTypeBinding(expression);
    if (runtimeBinding != nullptr &&
        runtimeBinding->kind == MethodRuntimeTypeOperationKind::CheckedCast)
    {
        auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
        if (operand == nullptr)
        {
            return nullptr;
        }
        auto node = makeNode(
            ProjectOopHirNodeKind::CheckedReferenceCast, expression, std::string(resultType));
        node->targetTypeOrdinal = runtimeBinding->targetTypeOrdinal;
        node->targetIsInterface = runtimeBinding->targetIsInterface;
        node->children.push_back(std::move(operand));
        return node;
    }
    const MethodCallBinding* conversionCall = callBinding(expression);
    if (conversionCall != nullptr &&
        conversionCall->resolution == MethodCallResolutionKind::Resolved &&
        conversionCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
        *conversionCall->selectedMemberOrdinal < memberModel_->members().size() &&
        memberModel_->members()[*conversionCall->selectedMemberOrdinal].isUserConversion)
    {
        error = "user conversion escaped iterative Stage 34 OOP HIR lowering";
        return nullptr;
    }
    auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
    if (operand == nullptr)
    {
        return nullptr;
    }
    auto node = makeNode(
        ProjectOopHirNodeKind::Conversion, expression, std::string(resultType));
    node->children.push_back(std::move(operand));
    return node;
}
}
