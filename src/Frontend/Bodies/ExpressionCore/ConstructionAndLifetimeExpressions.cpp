#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "../Support/OwnershipArgumentClassification.h"
#include "../Support/OperatorClassification.h"
#include "../Support/IntegralConstantArithmetic.h"
#include "../Support/OwnershipFlowSnapshotQueries.h"


#include "../MethodBodyInternals.h"
#include "../../Internal/Stage37ConstantValue.h"

#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectCallableResultMerger.h"
#include "nativesharp/Frontend/ProjectCallableWaveReducer.h"
#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/SumTypeSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;

namespace
{
constexpr std::string_view kGenericRawNewAuxiliary = "stage52-generic-raw-new";

// EN: True when `member` takes exactly the parameter types in `expected`, in order.
// ES: Verdadero cuando `member` recibe exactamente los tipos de `expected`, en orden.
template <std::size_t Count>
bool allocatorMemberHasParameters(
    const ClassMemberSymbol& member,
    const std::array<std::string_view, Count>& expected)
{
    if (member.parameters.size() != Count) return false;
    for (std::size_t index = 0; index < Count; ++index)
    {
        if (member.parameters[index].typeKey != expected[index]) return false;
    }
    return true;
}
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindOffsetofExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::OffsetofExpression:
        {
            if (memberModel_ == nullptr || nameLookup_ == nullptr || inheritanceModel_ == nullptr)
            {
                break;
            }
            const std::string measuredTypeKey =
                memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
            if (isErrorType(measuredTypeKey)) break;
            const std::optional<std::size_t> typeOrdinal =
                inheritanceModel_->typeOrdinalForTypeKey(measuredTypeKey);
            if (!typeOrdinal.has_value() || *typeOrdinal >= nameLookup_->namedTypes().size() ||
                nameLookup_->namedTypes()[*typeOrdinal].kind != NamedTypeDeclarationKind::Struct)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOffsetof,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "offsetof requires a complete struct or union type");
                break;
            }

            std::optional<std::size_t> selectedField;
            for (const std::size_t candidate : memberModel_->findMembers(*typeOrdinal, expression->text()))
            {
                if (candidate >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& member = memberModel_->members()[candidate];
                if (member.kind == ClassMemberKind::Field && !member.isStatic)
                {
                    selectedField = candidate;
                    break;
                }
            }
            if (!selectedField.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOffsetof,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "type '" + measuredTypeKey + "' has no instance field named '" +
                        expression->text() + "'");
                break;
            }

            MethodCoreBinding binding;
            binding.kind = MethodCoreOperationKind::OffsetofConstant;
            binding.syntax = expression;
            binding.methodMemberOrdinal = methodMemberOrdinal;
            binding.scopeOrdinal = scopeOrdinal;
            binding.resultTypeKey = "builtin:nuint";
            binding.elementTypeKey = measuredTypeKey;
            binding.implicitFieldMemberOrdinal = selectedField;
            coreBindings_.push_back(std::move(binding));

            result.typeKey = "builtin:nuint";
            result.valid = true;
            break;
        }
        // Stage 52: default(T) -- the zero-equivalent value of T (0/false/'\0' for a numeric
        // primitive, null for a reference type or string, an all-zero-fields instance for a
        // value-typed struct). This is what lets a generic method give an honest "nothing found"
        // result (List<T>.Find's "return default(T);" when no element matches) that works
        // uniformly across every T the method gets instantiated with, exactly like real C#.
        // Treated like NullLiteralExpression for ownership purposes -- it names no existing
        // owner anywhere, so it needs no move()/ownership-transfer ceremony to flow into an
        // owned-typed return, field, or parameter.
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindDefaultValueExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::DefaultValueExpression:
            if (memberModel_ != nullptr && nameLookup_ != nullptr)
            {
                const std::string typeKey = memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
                if (isErrorType(typeKey))
                {
                    break;
                }
                if (typeKey == "builtin:void")
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "default(void) is not a valid expression");
                    break;
                }
                result.typeKey = typeKey;
                result.valid = true;
            }
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindObjectCreationExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::ObjectCreationExpression:
            if (memberModel_ != nullptr && nameLookup_ != nullptr)
            {
                // NS304-T09-F07: `new()` target-typed toma el tipo objetivo activo (el del
                // contexto que lo analiza); sin tipo objetivo reporta un error claro.
                const bool targetTyped = expression->typeSyntax() == nullptr &&
                    expression->auxiliaryText() == "target-typed";
                if (targetTyped && activeTargetTypeKey_.empty())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "target-typed new() requires a typed target context");
                    result.valid = false;
                    break;
                }
                const std::string objectType = targetTyped
                    ? activeTargetTypeKey_
                    : memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
                ProjectConstructionSite site;
                site.syntax = expression;
                site.callableOrdinal = methodMemberOrdinal;
                site.kind = ProjectConstructionSiteKind::Object;
                site.targetTypeKey = objectType;
                site.targetTypeOrdinal = targetTyped
                    ? namedTypeOrdinalForTypeKey(objectType)
                    : nameLookup_->resolvedTypeOrdinal(expression->typeSyntax());
                site.argumentsTyped = true;
                bool argumentsValid = true;
                for (const ast::AstNode* argument : expression->children())
                {
                    // NSC-LIMIT-010: los argumentos de un constructor se analizan CON
                    // `allowMove`, igual que los de una llamada ordinaria.
                    //
                    // Hasta el 2026-09-04 se analizaban sin el, y aqui habia un comentario que
                    // explicaba el hueco: ProjectConstructionModel elegia constructor mirando solo
                    // tipos y conversiones fisicas, sin validacion de propiedad, asi que admitir
                    // `move(...)` habria producido una transferencia que nadie comprobaba. El
                    // hueco esta cerrado: la regla vive en classifyOwnershipArgument y
                    // bindConstruction la aplica sobre los mismos datos.
                    //
                    // Sin `allowMove` no basta con que el constructor exista: `move(x)` como
                    // argumento se rechazaba ANTES de llegar a la resolucion, y el lowering de HIR
                    // habria muerto despues en «move expression has no semantic move binding»,
                    // porque el binding se crea aqui o no se crea.
                    const ExpressionResult argumentResult = analyzeExpression(
                        argument, methodMemberOrdinal, scopeOrdinal, diagnostics,
                        /*allowMethodGroup=*/false, /*allowEvent=*/false, /*allowMove=*/true);
                    const bool argumentValid = argumentResult.valid &&
                        !argumentResult.methodGroup && !isErrorType(argumentResult.typeKey);
                    argumentsValid = argumentsValid && argumentValid;
                    site.argumentsTyped = site.argumentsTyped && argumentValid;
                    if (argumentValid)
                    {
                        site.argumentTypes.push_back(makeExpressionTypeBinding(
                            argument,
                            methodMemberOrdinal,
                            scopeOrdinal,
                            argumentResult));
                    }
                }
                recordConstructionSite(std::move(site));
                result.typeKey = objectType;
                result.valid = argumentsValid && !isErrorType(objectType);
                // NSC-LIMIT-010: `new T(...)` sobre un tipo con almacenamiento propio produce un
                // OWNER RECIEN CREADO. No se marca transferencia --no hay fuente con nombre-- sino
                // el tercer estado, que solo relaja el contrato de un parametro `owned`. Un
                // parametro sin calificador sigue viendo exactamente lo que veia antes.
                result.ownershipFreshOwner = result.valid && typeKeyIsOwnable(objectType);
                if (result.valid)
                {
                    result.objectLifetimeOwnership = sema::OwnershipKind::Owned;
                    result.storageOwnership = sema::OwnershipKind::Owned;
                }
            }
            break;
        default:
            break;
    }
    return result;
}

void ProjectMethodBodyModel::configureAllocatorConstruction(
    ProjectConstructionSite& site,
    const ast::AstNode* expression,
    const ExpressionResult& storage) const
{
    if (inheritanceModel_ == nullptr || interfaceModel_ == nullptr) return;
    const std::optional<std::size_t> storageType =
        inheritanceModel_->typeOrdinalForTypeKey(storage.typeKey);
    const std::optional<std::size_t> allocatorTrait =
        namedTypeOrdinalForTypeKey("type:NativeSharpCore::NativeSharp.Core.IAllocator");
    if (!storageType.has_value() || !allocatorTrait.has_value() ||
        !interfaceModel_->classImplementsInterface(*storageType, *allocatorTrait)) return;
    site.kind = ProjectConstructionSiteKind::Allocator;
    site.allocatorTypeKey = storage.typeKey;
    // EN: Freeze 3.0 C-08: select the members that implement the frozen IAllocator protocol by
    //     signature -- Allocate(long, int) and Deallocate(byte*, long, int). A class may also keep
    //     other overloads (ArenaAllocator keeps the historic Allocator.Allocate(int)), and taking
    //     the first member named "Allocate" would call the wrong one.
    // ES: Congelamiento 3.0 C-08: se eligen por firma los miembros que implementan el protocolo
    //     congelado de IAllocator -- Allocate(long, int) y Deallocate(byte*, long, int). Una clase
    //     puede conservar otras sobrecargas (ArenaAllocator conserva Allocate(int) de Allocator), y
    //     tomar el primer miembro llamado "Allocate" llamaria al equivocado.
    const std::array<std::string_view, 2> allocateParameters = {"builtin:long", "builtin:int"};
    const std::array<std::string_view, 3> deallocateParameters = {
        "builtin:byte*", "builtin:long", "builtin:int"};
    for (const std::size_t memberOrdinal :
         memberModel_->findMembers(*storageType, "Allocate"))
    {
        if (memberOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.isStatic || !allocatorMemberHasParameters(member, allocateParameters)) continue;
        site.allocatorAllocateMemberOrdinal = memberOrdinal;
        break;
    }
    // NS304-T13-F07: the provenance pair -- which Deallocate releases the
    // bytes this Allocate hands out.
    for (const std::size_t memberOrdinal :
         memberModel_->findMembers(*storageType, "Deallocate"))
    {
        if (memberOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.isStatic || !allocatorMemberHasParameters(member, deallocateParameters)) continue;
        site.allocatorDeallocateMemberOrdinal = memberOrdinal;
        break;
    }
    // NS304-T13-F08: the explicit allocation-failure profile.
    const std::string& profile = expression->allocatorFailureProfile();
    if (profile == "null")
        site.allocatorFailureProfile = ProjectAllocatorFailureProfile::Null;
    else if (profile == "result")
        site.allocatorFailureProfile = ProjectAllocatorFailureProfile::Result;
    else if (profile == "throws")
        site.allocatorFailureProfile = ProjectAllocatorFailureProfile::Throws;
    else if (profile == "panic")
        site.allocatorFailureProfile = ProjectAllocatorFailureProfile::Panic;
}

bool ProjectMethodBodyModel::analyzeConstructionArguments(
    ProjectConstructionSite& site,
    const ast::AstNode* expression,
    std::size_t constructorArgumentCount,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    bool argumentsValid = true;
    for (std::size_t argumentIndex = 0;
         argumentIndex < constructorArgumentCount; ++argumentIndex)
    {
        const ast::AstNode* argument = expression->children()[argumentIndex];
        const ExpressionResult argumentResult = analyzeExpression(
            argument, methodMemberOrdinal, scopeOrdinal, diagnostics,
            /*allowMethodGroup=*/false, /*allowEvent=*/false, /*allowMove=*/true);
        const bool argumentValid = argumentResult.valid &&
            !argumentResult.methodGroup && !isErrorType(argumentResult.typeKey);
        argumentsValid = argumentsValid && argumentValid;
        site.argumentsTyped = site.argumentsTyped && argumentValid;
        if (argumentValid)
        {
            site.argumentTypes.push_back(makeExpressionTypeBinding(
                argument, methodMemberOrdinal, scopeOrdinal, argumentResult));
        }
    }
    return argumentsValid;
}

std::string ProjectMethodBodyModel::configureAllocatorResultProfile(
    ProjectConstructionSite& site,
    const ast::AstNode* expression)
{
    std::string resultCarrierTypeKey;
    if (expression->allocatorFailureProfile() != "result" ||
        expression->typeSyntaxes().empty() || expression->typeSyntaxes()[0] == nullptr)
    {
        return resultCarrierTypeKey;
    }
    resultCarrierTypeKey =
        memberModel_->canonicalTypeKey(expression->typeSyntaxes()[0], *nameLookup_);
    site.allocatorResultCarrierTypeKey = resultCarrierTypeKey;
    if (const std::optional<MethodResultPropagationBinding> carrier =
            resolveResultCarrier(resultCarrierTypeKey))
    {
        site.allocatorResultOkFactoryMemberOrdinal =
            carrier->sourceSuccessFactoryMemberOrdinal;
        site.allocatorResultErrorFactoryMemberOrdinal =
            carrier->returnErrorFactoryMemberOrdinal;
        resultFactoryDiscoveries_.push_back(carrier->sourceSuccessFactoryMemberOrdinal);
        resultFactoryDiscoveries_.push_back(carrier->returnErrorFactoryMemberOrdinal);
    }
    return resultCarrierTypeKey;
}

void ProjectMethodBodyModel::completePlacementConstructionResult(
    ExpressionResult& result,
    const NamedTypeDeclaration* target,
    std::string_view targetType,
    std::string_view requiredStorageType,
    std::string_view resultCarrierTypeKey,
    bool trailingConstruction,
    bool resultFailureProfile,
    bool allocatorConstruction,
    bool constructible,
    bool storageValid,
    bool argumentsValid) const
{
    // A placed struct is exposed through the raw T* that names its supplied storage.
    // A class expression keeps NativeSharp's ordinary class-reference type even
    // though its placement operand is T*: physically both are the object address,
    // while logically this lets `placed.Member` use the class contract instead of
    // treating the address as a pointer-to-reference slot.
    // NS304-T13-F06: a `trailing:` construction yields the header struct BY VALUE (the
    // header plus its flexible tail is addressed through that value).
    result.typeKey = target != nullptr &&
            (target->kind == NamedTypeDeclarationKind::Class || trailingConstruction)
        ? std::string(targetType)
        : std::string(requiredStorageType);
    // NS304-T13-F08 `failure: result`: the expression type is the closed Result<T,E>.
    if (resultFailureProfile && !resultCarrierTypeKey.empty())
        result.typeKey = resultCarrierTypeKey;
    result.valid = constructible && storageValid && argumentsValid &&
        !isErrorType(targetType);
    if (result.valid)
    {
        result.objectLifetimeOwnership = sema::OwnershipKind::Owned;
        // Raw placement borrows caller-supplied T* storage. `new (allocator)`, in
        // contrast, returns the owner that must eventually call that allocator's
        // Deallocate; the descriptor sidecar preserves how to discharge the owned
        // storage obligation without changing the object header.
        result.storageOwnership = allocatorConstruction
            ? sema::OwnershipKind::Owned
            : sema::OwnershipKind::Borrowed;
    }
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::analyzeExpressionKindPlacementConstructionExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    if (memberModel_ == nullptr || nameLookup_ == nullptr) return result;
    const std::string targetType =
        memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
    ProjectConstructionSite site;
    site.syntax = expression;
    site.callableOrdinal = methodMemberOrdinal;
    site.kind = ProjectConstructionSiteKind::Placement;
    site.targetTypeKey = targetType;
    site.targetTypeOrdinal = nameLookup_->resolvedTypeOrdinal(expression->typeSyntax());
    site.argumentsTyped = true;
    const NamedTypeDeclaration* target =
        nameLookup_->resolvedDeclaration(expression->typeSyntax());
    const bool constructible = target != nullptr &&
        (target->kind == NamedTypeDeclarationKind::Class ||
         target->kind == NamedTypeDeclarationKind::Struct) &&
        expression->typeSyntax() != nullptr &&
        expression->typeSyntax()->modifiers().empty();
    if (!constructible)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "placement construction requires an unmodified class or struct type");
    }
    const ExpressionResult storage = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    const std::string requiredStorageType = targetType + "*";
    bool storageValid = storage.valid && !storage.methodGroup;
    // NS304-T13-F02: `new (allocator) T(args)` with `allocator : IAllocator` is allocator
    // selection, not raw placement against a T* destination.
    if (storageValid) configureAllocatorConstruction(site, expression, storage);
    if (storageValid && site.kind != ProjectConstructionSiteKind::Allocator &&
        storage.typeKey != requiredStorageType)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "placement construction of '" + targetType +
                "' requires destination storage of exact type '" + requiredStorageType + "'");
        storageValid = false;
    }
    // NS304-T13-F06: the trailing `trailing: capacity` child is not a constructor
    // argument; it is the flexible-array extent and is validated separately.
    const bool trailingConstruction = expression->auxiliaryText() == "trailing";
    const std::size_t constructorArgumentCount =
        expression->children().size() - (trailingConstruction ? 1U : 0U);
    const bool argumentsValid = analyzeConstructionArguments(
        site, expression, constructorArgumentCount,
        methodMemberOrdinal, scopeOrdinal, diagnostics);
    // NS304-T13-F08 `failure: result`: resolve the closed Result<T,E> carrier (the
    // synthesized type use) and its Ok/Error factories before the site is consumed.
    const bool resultFailureProfile = expression->allocatorFailureProfile() == "result";
    const std::string resultCarrierTypeKey =
        configureAllocatorResultProfile(site, expression);
    const bool allocatorConstruction =
        site.kind == ProjectConstructionSiteKind::Allocator;
    recordConstructionSite(std::move(site));
    completePlacementConstructionResult(
        result, target, targetType, requiredStorageType, resultCarrierTypeKey,
        trailingConstruction, resultFailureProfile, allocatorConstruction,
        constructible, storageValid, argumentsValid);
    return result;
}

bool ProjectMethodBodyModel::analyzeRangeArrayAllocation(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::string_view element,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    if (!expression->auxiliaryText().starts_with("range-array-allocation:"))
        return false;
    std::vector<const ast::AstNode*> dimensions;
    dimensions.push_back(expression->first());
    dimensions.insert(
        dimensions.end(), expression->children().begin(), expression->children().end());
    bool dimensionsValid = dimensions.size() >= 2;
    for (const ast::AstNode* dimension : dimensions)
    {
        const ExpressionResult extent = analyzeExpression(
            dimension, methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (!extent.valid || !ProjectTypeConversion::isIntegral(extent.typeKey) ||
            extent.typeKey == "builtin:bool")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArrayCount,
                diagnostics::DiagnosticSeverity::Error,
                dimension->range(),
                "rectangular array dimensions must be integral");
            dimensionsValid = false;
        }
    }
    if (element == "builtin:void")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "rectangular arrays cannot have void elements");
        dimensionsValid = false;
    }
    result.typeKey =
        std::string(element) + '[' + std::string(dimensions.size() - 1, ',') + ']';
    result.valid = dimensionsValid && !isErrorType(element);
    if (result.valid)
    {
        result.storageOwnership = sema::OwnershipKind::Owned;
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::RangeArrayAllocation,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            result.typeKey,
            std::string(element),
            false, std::nullopt, std::nullopt});
    }
    return true;
}

void ProjectMethodBodyModel::analyzeNativeArrayAllocation(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::string_view element,
    const ExpressionResult& count,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const bool inferredInitializer =
        expression->auxiliaryText() == "inferred-array-initializer";
    bool countValid = inferredInitializer
        ? expression->first() == nullptr
        : count.valid && ProjectTypeConversion::isIntegral(count.typeKey);
    if (inferredInitializer && expression->first() != nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArrayCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "an inferred native array initializer must omit the element count");
        countValid = false;
    }
    if (!countValid)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArrayCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "native array allocation requires an integral element count");
    }
    if (element == "builtin:void")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "native arrays cannot have void elements");
    }
    bool elementsValid = true;
    if (inferredInitializer)
    {
        for (const ast::AstNode* initializer : expression->children())
        {
            const ExpressionResult value = analyzeExpression(
                initializer, methodMemberOrdinal, scopeOrdinal, diagnostics,
                /*allowMethodGroup=*/false, /*allowEvent=*/false, /*allowMove=*/true);
            if (!value.valid || !classifyImplicit(
                    value.typeKey, element, value.integerConstant).exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    initializer->range(),
                    "native array initializer element is not implicitly convertible to '" +
                        std::string(element) + "'");
                elementsValid = false;
            }
        }
    }
    result.typeKey = std::string(element) + "[]";
    result.valid = !isErrorType(element) && element != "builtin:void" &&
        countValid && elementsValid;
    if (result.valid) result.storageOwnership = sema::OwnershipKind::Owned;
    if (result.valid)
    {
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::ArrayAllocation,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            result.typeKey,
            std::string(element),
            false, std::nullopt, std::nullopt});
    }
}

bool ProjectMethodBodyModel::validateRawAllocationElement(
    const ast::AstNode* expression,
    std::string_view element,
    bool isStackalloc,
    bool isRawNew,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    // CC1 fases 4B y 4C. Hasta aqui llegan las dos formas que quedaban SIN VALIDAR y
    // sin binding: `stackalloc T[n]` y el `new T` escalar. Las dos ponian `T*`,
    // marcaban valido, y morian despues al bajar -- `stackalloc` en el `default` del
    // HIR y `new T` en un error que decia literalmente que el raw new "remains outside
    // this pipeline". Mitad delantera presente, feature ausente; la misma forma que
    // `sizeof` en la fase 4A.
    //
    // AVISO SOBRE EL TEXTO DE LA TAREA 47. Pedia "portar a ProjectMethodBodyModel las
    // validaciones de stackalloc DEL PIPELINE CLASICO". Barrido completo del arbol:
    // `stackalloc` solo aparece en el lexer, el parser, el AST y este caso. NO HAY
    // pipeline clasico del que portar nada, igual que no lo habia para `sizeof`. Las
    // validaciones de abajo estan ESCRITAS, no portadas, y cada una dice por que
    // existe. (DiagnosticId::InvalidStackalloc llevaba tiempo declarado y sin usar en
    // ningun sitio -- ahora se usa.)
    bool allocationValid = !isErrorType(element);
    if (!isStackalloc && !isRawNew) return allocationValid;
    // Sin tamano no hay reserva posible, ni en la pila ni en el monton.
    if (element == "builtin:void")
    {
        diagnostics.report(
            isStackalloc ? diagnostics::DiagnosticId::InvalidStackalloc
                         : diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            isStackalloc ? "stackalloc requires an element type with a size; void has none"
                         : "new requires an element type with a size; void has none");
        allocationValid = false;
    }
    // Un elemento con destructor o con campos `owned` tiene un ciclo de vida que
    // alguien debe cerrar. Ni la memoria cruda del monton ni un buffer de pila lo
    // cierran: no hay nada que llame al destructor al salir del ambito, asi que el
    // recurso se filtraria -- y en el caso de la pila, la memoria desaparece bajo
    // los pies del objeto. Se prohibe en vez de inventar una semantica de
    // destruccion nueva para el nivel crudo.
    if (allocationValid && !elementTypeHasTrivialLifetime(element))
    {
        diagnostics.report(
            isStackalloc ? diagnostics::DiagnosticId::InvalidStackalloc
                         : diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            std::string(isStackalloc ? "stackalloc" : "raw new") +
                " requires an element type with no destructor and no owned fields; '" +
                std::string(element) + "' owns a lifetime that raw storage cannot discharge");
        allocationValid = false;
    }
    return allocationValid;
}

bool ProjectMethodBodyModel::validateStackalloc(
    const ast::AstNode* expression,
    const ExpressionResult& count,
    bool allocationValid,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    // El contador. La forma dinamica de `new T[n]` ya lo exige arriba; esta rama
    // no lo hacia, asi que `stackalloc int[unaCadena]` tipaba sin una palabra.
    if (!count.valid || !ProjectTypeConversion::isIntegral(count.typeKey) ||
        count.typeKey == "builtin:bool")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidStackalloc,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "stackalloc requires an integral element count");
        allocationValid = false;
    }
    // =============================================================================
    // ID 123 -- ENDURECIMIENTO DE `stackalloc`.
    //
    // Lo que ya se comprobaba antes de esta ronda: que el elemento tenga tamano
    // (no `void`), que su vida sea trivial (sin destructor y sin campos `owned`) y
    // que el contador sea integral. Nada mas. Un `stackalloc` con un contador
    // NEGATIVO, o con uno tan grande que la reserva no cabe en la pila, pasaba
    // entero y llegaba a LLVM como un `alloca` que revienta en ejecucion.
    //
    // Las cuatro comprobaciones NO CABEN TODAS AQUI, y el reparto no es una
    // preferencia: dos necesitan el TAMANO DEL ELEMENTO, y este modelo no lo
    // tiene. El orden de construccion del pipeline es modelo de cuerpos de metodo
    // PRIMERO y ProjectObjectLayoutModel DESPUES -- `build()` ni siquiera recibe
    // el modelo de layout, y esta escrito en el comentario de SizeofConstant en
    // ProjectMethodBodyModel.h por la misma razon. Duplicar aqui las reglas de
    // tamano seria justo la duplicacion que el milestone CC1 existe para quitar.
    //
    // Reparto, entonces:
    //   AQUI (necesitan solo el contador): contador constante negativo, y
    //        `stackalloc` dentro de un bucle.
    //   EN ProjectOopHirModel (necesitan el tamano): que la reserva total quepa.
    //
    // Lo que se gana teniendo estas dos aqui: salen por el motor de diagnosticos,
    // con el rango del codigo fuente y el texto que lee la persona. Las del HIR
    // salen por su cadena `error`, que habla de etapas internas -- es la
    // limitacion de esa capa, no una eleccion.
    // =============================================================================
    // ID 123 (1/3). CONTADOR CONSTANTE NEGATIVO.
    //
    // Se puede comprobar desde que el ID 119 cerro la constancia de los literales
    // negativos: antes de aquello `-1` ni siquiera llegaba aqui como constante, y
    // por eso esta comprobacion no se pudo escribir en su momento.
    //
    // Sobre el tipo, no sobre los bits: un contador `uint` con el bit alto puesto
    // NO es negativo, y leerlo como si lo fuera rechazaria `stackalloc byte[n]`
    // con n = 3000000000u, que es perfectamente legitimo escribirlo aunque luego
    // no quepa -- de que no quepa se encarga la comprobacion del HIR.
    if (allocationValid && expression->first() != nullptr)
    {
        // `declaringTypeOrdinal` va en nullopt: ese parametro solo se usa
        // mientras se expande el inicializador de un campo `const`, y el
        // contador de un `stackalloc` esta en el cuerpo de un metodo. Es lo
        // mismo que pasa el plegado de las etiquetas de `switch`.
        const std::optional<ProjectIntegerValue> foldedCount =
            foldIntegerConstant(expression->first(), std::nullopt, methodMemberOrdinal);
        if (foldedCount.has_value() &&
            ProjectIntegerEvaluator::isSignedInteger(foldedCount->typeKey) &&
            foldedCount->asSigned() < 0)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidStackalloc,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "stackalloc element count is negative (" +
                    std::to_string(foldedCount->asSigned()) +
                    "); a reservation cannot have fewer than zero elements");
            allocationValid = false;
        }
    }
    // ID 123 (2/3). `stackalloc` DENTRO DE UN BUCLE.
    //
    // Es la trampa clasica de C y merece decirse: un `alloca` dentro de un bucle
    // reserva OTRA VEZ en cada vuelta y NADA se libera hasta salir de la funcion.
    // La pila crece con las iteraciones, no con la profundidad de la llamada. Un
    // bucle de mil vueltas con `stackalloc int[256]` son un mega de pila, y el
    // programa parece correcto leyendolo.
    //
    // AVISO Y NO ERROR, y es deliberado: la forma es legitima cuando el bucle da
    // pocas vueltas, y este lenguaje no tiene con que demostrar cuantas da. Un
    // error aqui prohibiria codigo correcto; callarse deja pasar un desbordamiento
    // de pila con forma de programa sano. El aviso es lo unico honesto.
    //
    // ES EL PRIMER WARNING DEL COMPILADOR: `DiagnosticSeverity::Warning` existia y
    // `DiagnosticEngine::warningCount()` tambien, pero ningun sitio del arbol los
    // usaba (barrido completo: cero apariciones de `DiagnosticSeverity::Warning`
    // en src/). Comprobado que no rompe nada: el exito se decide con
    // `hasErrors()`, que solo cuenta los de severidad Error.
    if (allocationValid && !loopStack_.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidStackalloc,
            diagnostics::DiagnosticSeverity::Warning,
            expression->range(),
            "stackalloc inside a loop reserves again on every iteration and "
            "nothing is released until the function returns; hoist it out of the "
            "loop or use a heap allocation");
    }
    return allocationValid;
}

void ProjectMethodBodyModel::recordRawAllocation(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::string_view element,
    bool allocationValid,
    bool isStackalloc,
    bool isRawNew,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal)
{
    result.typeKey = std::string(element) + '*';
    result.valid = allocationValid;
    if (result.valid) result.storageOwnership = sema::OwnershipKind::Owned;
    if (allocationValid && (isStackalloc || isRawNew))
    {
        MethodCoreBinding binding;
        binding.kind = isStackalloc
            ? MethodCoreOperationKind::StackAllocation
            : MethodCoreOperationKind::RawAllocation;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.resultTypeKey = result.typeKey;
        binding.elementTypeKey = element;
        coreBindings_.push_back(std::move(binding));
    }
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::analyzeExpressionKindNewExpressionOrStackallocExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    if (memberModel_ == nullptr || nameLookup_ == nullptr) return result;
    const std::string element =
        memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
    ExpressionResult count;
    if (expression->first() != nullptr)
    {
        count = analyzeExpression(
            expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    const bool dynamicArrayNew =
        expression->kind() == ast::AstNodeKind::NewExpression && expression->flag();
    if (dynamicArrayNew)
    {
        if (analyzeRangeArrayAllocation(
                result, expression, element,
                methodMemberOrdinal, scopeOrdinal, diagnostics))
        {
            return result;
        }
        analyzeNativeArrayAllocation(
            result, expression, element, count,
            methodMemberOrdinal, scopeOrdinal, diagnostics);
        return result;
    }
    const NamedTypeDeclaration* declared =
        nameLookup_->resolvedDeclaration(expression->typeSyntax());
    const bool genericRawNew = expression->auxiliaryText() == kGenericRawNewAuxiliary;
    const bool classNew = expression->kind() == ast::AstNodeKind::NewExpression &&
        !genericRawNew && declared != nullptr &&
        declared->kind == NamedTypeDeclarationKind::Class;
    if (classNew)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "class allocation requires object-construction syntax new C(args) in Stage 34");
    }
    const bool isStackalloc =
        expression->kind() == ast::AstNodeKind::StackallocExpression;
    const bool isRawNew =
        expression->kind() == ast::AstNodeKind::NewExpression && !classNew;
    bool allocationValid = !classNew && validateRawAllocationElement(
        expression, element, isStackalloc, isRawNew, diagnostics);
    if (isStackalloc)
    {
        allocationValid = validateStackalloc(
            expression, count, allocationValid, methodMemberOrdinal, diagnostics);
    }
    recordRawAllocation(
        result, expression, element, allocationValid, isStackalloc, isRawNew,
        methodMemberOrdinal, scopeOrdinal);
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindDeleteExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::DeleteExpression:
        {
            bool deletesRaiiLocal = false;
            if (expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const std::optional<ScopeName> target = lookupScopeName(scopeOrdinal, expression->first()->text());
                deletesRaiiLocal = target.has_value() && target->kind == ScopeNameKind::Local &&
                    target->semanticOrdinal < locals_.size() && locals_[target->semanticOrdinal].isRaii;
                if (deletesRaiiLocal)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "explicit delete of a RAII local is forbidden; lexical cleanup owns destruction");
                }
            }
            const ExpressionResult operand = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            const ClassMemberSymbol* context = methodSymbol(methodMemberOrdinal);
            const bool deletesCurrentThis = context != nullptr &&
                context->kind == ClassMemberKind::Destructor &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::ThisExpression;
            if (deletesCurrentThis)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "delete this is forbidden inside a destructor; the generated destruction entry owns "
                    "final deallocation");
            }
            // Bloque 1 (audit finding "delete holder.slot"): explicit delete of an `owned` FIELD
            // is forbidden, for the same reason delete of a RAII local is forbidden two checks
            // above -- the storage's own discharge machinery owns destruction. Measured before
            // this rule: `delete h.slot` destroyed the object AND left the slot's pointer intact,
            // so the container's destruction entry released the same allocation again -- 2
            // destructions of 1 object, -O0 and -O2 alike. A double free the compiler itself
            // guaranteed.
            //
            // Prohibition, not auto-nulling: clearing the slot as a side effect of `delete` would
            // couple delete to the identity of its operand lvalue (delete via any alias would
            // still double-free), and the language already has the honest spellings for every
            // intent -- `h.slot = null` to clear (releases exactly once, verified), overwrite to
            // replace, move-out to take. The audit reached the same conclusion: prohibiting is
            // simpler and safer than making a destruction operator silently mutate its operand.
            //
            // Scope: only owned fields that OWN storage (reference type or native string).
            // `owned T[]` array fields stay deletable -- `delete items[i]` / `delete[] items` is
            // the manual array model ~List() is built on, and an array element read never carries
            // the field's member ordinal anyway.
            bool deletesOwnedField = false;
            if (operand.valid && operand.assignmentMemberOrdinal.has_value() &&
                memberModel_ != nullptr &&
                *operand.assignmentMemberOrdinal < memberModel_->members().size())
            {
                const ClassMemberSymbol& deletedMember =
                    memberModel_->members()[*operand.assignmentMemberOrdinal];
                deletesOwnedField = deletedMember.kind == ClassMemberKind::Field &&
                    !deletedMember.isStatic &&
                    deletedMember.declaredOwnership == sema::OwnershipKind::Owned &&
                    (isReferenceTypeKey(deletedMember.declaredTypeKey) ||
                     deletedMember.declaredTypeKey == "builtin:string");
                if (deletesOwnedField)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "explicit delete of owned field '" + deletedMember.name +
                            "' is forbidden; the container releases it on destruction -- clear it "
                            "with '= null', replace it, or take it with move(...)");
                }
            }
            result.typeKey = "builtin:void";
            result.valid = operand.valid && !deletesCurrentThis && !deletesRaiiLocal &&
                !deletesOwnedField;
            break;
        }
        default:
            break;
    }
    return result;
}

bool ProjectMethodBodyModel::validateDestroyTarget(
    const ast::AstNode* expression,
    const ExpressionResult& operand,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!operand.valid) return false;
    // A class/interface reference carries the runtime descriptor needed for dynamic
    // destruction. A raw T* is accepted only when T is a concrete class or struct;
    // the latter currently has trivial destruction but still has a real lifetime to
    // end after placement construction.
    bool validTarget = isReferenceTypeKey(operand.typeKey);
    if (!validTarget && operand.typeKey.ends_with('*') && operand.typeKey.size() > 1)
    {
        const std::string_view pointee(
            operand.typeKey.data(), operand.typeKey.size() - 1);
        const std::optional<std::size_t> ordinal = namedTypeOrdinalForTypeKey(pointee);
        validTarget = ordinal.has_value() && nameLookup_ != nullptr &&
            *ordinal < nameLookup_->namedTypes().size() &&
            (inheritanceModel_->isClass(*ordinal) ||
             nameLookup_->namedTypes()[*ordinal].kind == NamedTypeDeclarationKind::Struct);
    }
    if (!validTarget)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "destroy requires a class/interface reference or a pointer to a concrete class or struct");
    }
    return validTarget;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindDestroyExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::DestroyExpression:
        {
            const ExpressionResult operand = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);

            const bool validTarget = validateDestroyTarget(expression, operand, diagnostics);

            bool destroysRaiiLocal = false;
            if (expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const std::optional<ScopeName> target =
                    lookupScopeName(scopeOrdinal, expression->first()->text());
                destroysRaiiLocal = target.has_value() && target->kind == ScopeNameKind::Local &&
                    target->semanticOrdinal < locals_.size() && locals_[target->semanticOrdinal].isRaii;
                if (destroysRaiiLocal)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "explicit destroy of a RAII local is forbidden; lexical cleanup owns destruction");
                }
            }

            const ClassMemberSymbol* context = methodSymbol(methodMemberOrdinal);
            const bool destroysCurrentThis = context != nullptr &&
                context->kind == ClassMemberKind::Destructor &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::ThisExpression;
            if (destroysCurrentThis)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "destroy this is forbidden inside a destructor; the active destruction entry owns the "
                    "object lifetime");
            }

            const bool destroysOwnedValue = operand.valid &&
                operand.ownership == sema::OwnershipKind::Owned && !destroysRaiiLocal;
            if (destroysOwnedValue)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "explicit destroy of an owned value is forbidden; its owner must also discharge storage");
            }

            result.typeKey = "builtin:void";
            result.valid = operand.valid && validTarget && !destroysRaiiLocal &&
                !destroysCurrentThis && !destroysOwnedValue;
            break;
        }
        default:
            break;
    }
    return result;
}

}
