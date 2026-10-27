#include "HirLoweringSupport.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/OwnershipQualifierSyntaxKind.h"
#include "nativesharp/AST/TypeSyntax.h"
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

// EN: Freeze 3.0 (plan item 118): an object handed to an `owned` parameter (a `new`, a `move`, an owned
//     return) is owned by a hidden slot from its evaluation until the call starts. A later argument that
//     throws, or an exception that leaves the coroutine before the call, destroys it through the slot's
//     cleanup; the call clears the slot right before it runs (booleanValue marks the transfer).
// ES: Congelamiento 3.0 (elemento 118 del plan): un objeto entregado a un parámetro `owned` (un `new`, un
//     `move`, un retorno owned) es de un slot oculto desde su evaluación hasta que empieza la llamada. Un
//     argumento posterior que lanza, o una excepción que sale de la corrutina antes de la llamada, lo destruye
//     por el cleanup del slot; la llamada limpia el slot justo antes de correr (booleanValue marca la entrega).
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::wrapOwnedArgumentTransfer(
    std::unique_ptr<ProjectOopHirNode> value,
    const ast::AstNode* argumentSyntax,
    bool ownedParameter,
    bool& hasOwnedTemporary)
{
    if (!ownedParameter || value == nullptr || value->kind == ProjectOopHirNodeKind::OwnedTemporaryBorrow ||
        !ownedTransferType(value->typeKey))
        return value;
    auto temporary = makeNode(ProjectOopHirNodeKind::OwnedTemporaryBorrow, argumentSyntax, value->typeKey);
    temporary->localOrdinal = nextSyntheticLocalOrdinal_++;
    temporary->cleanupRegionOrdinal = nextSyntheticCleanupRegionOrdinal_++;
    temporary->booleanValue = true;
    temporary->children.push_back(std::move(value));
    hasOwnedTemporary = true;
    return temporary;
}

bool ProjectOopHirModel::ownedValueParameter(const MethodParameterSymbol& parameter) noexcept
{
    return parameter.ownership == sema::OwnershipKind::Owned &&
        parameter.passKind == ParameterPassKind::Value && !parameter.isParameterPack;
}

bool ProjectOopHirModel::ownedTransferType(std::string_view typeKey) const
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::optional<std::size_t> ordinal = nameLookup_->typeOrdinalForIdentity(typeKey.substr(5));
    const NamedTypeDeclaration* type = ordinal.has_value() ? nameLookup_->typeForOrdinal(*ordinal) : nullptr;
    return type != nullptr &&
        (type->kind == NamedTypeDeclarationKind::Class || type->kind == NamedTypeDeclarationKind::Interface);
}

bool ProjectOopHirModel::constructionParameterOwned(
    std::optional<std::size_t> constructorOrdinal, std::size_t parameterIndex) const
{
    if (memberModel_ == nullptr || !constructorOrdinal.has_value() ||
        *constructorOrdinal >= memberModel_->members().size())
        return false;
    const ClassMemberSymbol& constructor = memberModel_->members()[*constructorOrdinal];
    return parameterIndex < constructor.parameters.size() &&
        ownedValueParameter(constructor.parameters[parameterIndex]);
}

// EN: A delegate parameter is owned when its declaration says `owned` (a structural function type has none).
// ES: Un parámetro de delegate es owned cuando su declaración dice `owned` (un tipo función estructural no
//     tiene declaración).
bool ProjectOopHirModel::delegateParameterOwned(std::size_t delegateTypeOrdinal, std::size_t index) const
{
    if (nameLookup_ == nullptr || delegateTypeOrdinal >= nameLookup_->namedTypes().size()) return false;
    const NamedTypeDeclaration& declaration = nameLookup_->namedTypes()[delegateTypeOrdinal];
    if (declaration.kind != NamedTypeDeclarationKind::Delegate || declaration.syntax == nullptr) return false;
    std::size_t position = 0;
    for (const ast::AstNode* child : declaration.syntax->children())
    {
        if (child == nullptr || child->kind() != ast::AstNodeKind::ParameterDeclaration) continue;
        if (position++ != index) continue;
        const ast::TypeSyntax* type = child->typeSyntax();
        return type != nullptr && type->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::Owned;
    }
    return false;
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
