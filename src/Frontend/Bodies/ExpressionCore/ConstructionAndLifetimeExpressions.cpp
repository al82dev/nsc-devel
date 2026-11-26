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
#include <vector>

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

// EN: True when `name` may be read after `point` inside `root`: a later identifier with that name,
//     or any use inside a loop that contains `point` but not `declaration`, since the next
//     iteration reads it after `point`. A shadowing local of the same name counts as a use, which
//     only makes the answer conservative.
// ES: Verdadero cuando `name` puede leerse despues de `point` dentro de `root`: un identificador
//     posterior con ese nombre, o cualquier uso dentro de un bucle que contiene `point` pero no
//     `declaration`, porque la siguiente iteracion lo lee despues de `point`. Un local que lo
//     oculta con el mismo nombre cuenta como uso, lo que solo vuelve conservadora la respuesta.
bool ProjectMethodBodyModel::identifierMayBeReadAfter(
    const ast::AstNode* root,
    const std::string& name,
    const ast::AstNode* point,
    const ast::AstNode* declaration)
{
    if (root == nullptr || point == nullptr) return true;
    const std::uint32_t pointBegin = point->range().begin().offset();
    const std::uint32_t pointEnd = point->range().end().offset();
    const std::uint32_t declarationBegin = declaration != nullptr ? declaration->range().begin().offset() : 0U;
    const auto isLoop = [](const ast::AstNode* node)
    {
        return node->kind() == ast::AstNodeKind::WhileStatement ||
            node->kind() == ast::AstNodeKind::DoWhileStatement ||
            node->kind() == ast::AstNodeKind::ForStatement ||
            node->kind() == ast::AstNodeKind::ForeachStatement;
    };
    struct Frame
    {
        const ast::AstNode* node;
        bool insideRepeatingLoop;
    };
    std::vector<Frame> pending{{root, false}};
    std::unordered_set<const ast::AstNode*> visited;
    while (!pending.empty())
    {
        const Frame frame = pending.back();
        pending.pop_back();
        if (frame.node == nullptr || !visited.insert(frame.node).second) continue;
        const SourceRange range = frame.node->range();
        bool repeating = frame.insideRepeatingLoop;
        if (isLoop(frame.node) && range.begin().offset() <= pointBegin && pointEnd <= range.end().offset() &&
            !(range.begin().offset() <= declarationBegin && declarationBegin <= range.end().offset()))
            repeating = true;
        if (frame.node->kind() == ast::AstNodeKind::IdentifierExpression && frame.node->text() == name &&
            (repeating || range.begin().offset() >= pointEnd))
            return true;
        pending.push_back({frame.node->first(), repeating});
        pending.push_back({frame.node->second(), repeating});
        pending.push_back({frame.node->third(), repeating});
        for (const ast::AstNode* child : frame.node->children()) pending.push_back({child, repeating});
    }
    return false;
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
                const auto objectOrdinal = targetTyped
                    ? namedTypeOrdinalForTypeKey(objectType)
                    : nameLookup_->resolvedTypeOrdinal(expression->typeSyntax());
                // EN: T19-002. A C struct or union has no constructors: `new S()` without arguments is its zero
                //     value, as `default(S)` and C's `(S){0}`, and goes through the implicit parameterless
                //     construction of a NativeSharp struct. With arguments it is rejected.
                // ES: T19-002. Un struct o una union C no tiene constructores: `new S()` sin argumentos es su valor
                //     cero, como `default(S)` y `(S){0}` de C, y pasa por la construccion implicita sin parametros de
                //     un struct NativeSharp. Con argumentos se rechaza.
                const auto* cImported = objectOrdinal ? nameLookup_->cppImportedType(*objectOrdinal) : nullptr;
                const bool cAggregate = cImported && cImported->objectLayout && !cImported->isIncomplete &&
                    cImported->deferredImportError.empty() &&
                    (cImported->origin == CppImportedTypeOrigin::CStruct ||
                     cImported->origin == CppImportedTypeOrigin::CUnion);
                if (cAggregate && !expression->children().empty())
                {
                    diagnostics.report(diagnostics::DiagnosticId::InvalidAllocationType,
                        diagnostics::DiagnosticSeverity::Error, expression->range(),
                        "C type '" + cImported->identity.fullyQualifiedName() + "' has no constructors; "
                        "new without arguments gives its zero value");
                    result.valid = false;
                    break;
                }
                if (objectOrdinal && !cAggregate && nameLookup_->cppImportedTypeIndex(*objectOrdinal) &&
                    !cppLifetimeAnalysisForTests_)
                {
                    diagnostics.report(diagnostics::DiagnosticId::InvalidAllocationType,
                        diagnostics::DiagnosticSeverity::Error, expression->range(),
                        "C/C++ object construction requires its object lifetime to be implemented");
                    result.valid = false;
                    break;
                }
                ProjectConstructionSite site;
                site.syntax = expression;
                site.callableOrdinal = methodMemberOrdinal;
                site.kind = ProjectConstructionSiteKind::Object;
                site.targetTypeKey = objectType;
                site.targetTypeOrdinal = objectOrdinal;
                site.argumentsTyped = true;
                site.activeErrorHandlers = activeErrorHandlers();
                site.enclosingErrorTypeKey = enclosingErrorTypeKey(methodMemberOrdinal);
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
                    ExpressionResult argumentResult = analyzeExpression(
                        argument, methodMemberOrdinal, scopeOrdinal, diagnostics,
                        /*allowMethodGroup=*/true, /*allowEvent=*/false, /*allowMove=*/true);
                    // EN: A method group passed to a constructor's delegate parameter (`new Thread(Worker.Run)`)
                    //     binds to that delegate, as a call argument does: the target is the one delegate type the
                    //     constructors with this arity take at this position.
                    // ES: Un grupo de metodos pasado al parametro delegado de un constructor se liga a ese
                    //     delegado, como un argumento de llamada: el destino es el unico tipo delegado que toman en
                    //     esta posicion los constructores con esta aridad.
                    if (argumentResult.methodGroup)
                    {
                        const std::size_t position = static_cast<std::size_t>(
                            std::find(expression->children().begin(), expression->children().end(), argument) -
                            expression->children().begin());
                        std::vector<std::string> targets;
                        if (objectOrdinal)
                            for (const ClassMemberSymbol& candidate : memberModel_->members())
                            {
                                if (candidate.kind != ClassMemberKind::Constructor ||
                                    candidate.declaringTypeOrdinal != *objectOrdinal ||
                                    candidate.parameters.size() != expression->children().size()) continue;
                                const std::string& target = candidate.parameters[position].typeKey;
                                if (methodGroupExactlyMatchesDelegate(argumentResult, target) &&
                                    std::ranges::find(targets, target) == targets.end())
                                    targets.push_back(target);
                            }
                        if (targets.size() == 1)
                            argumentResult = bindMethodGroupToDelegate(argument, argumentResult, targets.front(),
                                methodMemberOrdinal, scopeOrdinal, diagnostics);
                        else
                            diagnostics.report(targets.empty() ? diagnostics::DiagnosticId::InvalidConversion
                                                               : diagnostics::DiagnosticId::InvalidCall,
                                diagnostics::DiagnosticSeverity::Error, argument->range(),
                                targets.empty()
                                    ? "method group does not match any delegate parameter of a constructor of '" +
                                        objectType + "'"
                                    : "method group argument has more than one compatible delegate target in the "
                                      "constructors of '" + objectType + "'");
                    }
                    if (argumentResult.readonlyBorrowedObject)
                        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error, argument->range(),
                            "readonly borrowed object cannot be passed to an unchecked constructor");
                    const bool argumentValid = argumentResult.valid && !argumentResult.readonlyBorrowedObject &&
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
    // EN: Freeze C-08: select the members that implement the frozen IAllocator protocol by
    //     signature -- Allocate(long, int) and Deallocate(byte*, long, int). A class may also keep
    //     other overloads (ArenaAllocator keeps the historic Allocator.Allocate(int)), and taking
    //     the first member named "Allocate" would call the wrong one.
    // ES: Congelamiento C-08: se eligen por firma los miembros que implementan el protocolo
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
        if (argumentResult.readonlyBorrowedObject)
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, argument->range(),
                "readonly borrowed object cannot be passed to an unchecked constructor");
        const bool argumentValid = argumentResult.valid && !argumentResult.readonlyBorrowedObject &&
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
    const auto targetOrdinal = nameLookup_->resolvedTypeOrdinal(expression->typeSyntax());
    if (targetOrdinal && nameLookup_->cppImportedTypeIndex(*targetOrdinal))
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error, expression->range(),
            "C/C++ placement construction requires its object lifetime to be implemented");
        result.valid = false;
        return result;
    }
    ProjectConstructionSite site;
    site.syntax = expression;
    site.callableOrdinal = methodMemberOrdinal;
    site.kind = ProjectConstructionSiteKind::Placement;
    site.targetTypeKey = targetType;
    site.targetTypeOrdinal = targetOrdinal;
    site.argumentsTyped = true;
    site.activeErrorHandlers = activeErrorHandlers();
    site.enclosingErrorTypeKey = enclosingErrorTypeKey(methodMemberOrdinal);
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
    bool countValid = inferredInitializer && expression->first() == nullptr
        ? true : count.valid && ProjectTypeConversion::isIntegral(count.typeKey);
    if (inferredInitializer && expression->first() != nullptr)
    {
        bool hasSpread = false;
        for (const ast::AstNode* initializer : expression->children())
            hasSpread = hasSpread || (initializer != nullptr &&
                initializer->kind() == ast::AstNodeKind::SpreadElementExpression);
        if (!count.integerConstant.has_value() || count.integerConstant->isNegative ||
            count.integerConstant->value != expression->children().size() || hasSpread)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArrayCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "array initializer count must be constant and match its elements");
            countValid = false;
        }
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
    // EN: X2-06. Array elements are zero-initialized and copied bytewise, so a non-trivial C++ record cannot be one
    //     (the allocation reached the backend before, with no carrier). / ES: X2-06. Los elementos de un arreglo se
    //     inician en cero y se copian por bytes, asi que un record C++ no trivial no puede serlo.
    bool cppElementValid = true;
    if (nameLookup_ != nullptr && element.starts_with("type:") && !element.ends_with('*'))
        if (const auto ordinal = nameLookup_->typeOrdinalForIdentity(element.substr(5)))
            if (const auto* imported = nameLookup_->cppImportedType(*ordinal);
                imported && imported->origin == CppImportedTypeOrigin::CppRecord &&
                !nameLookup_->cppTrivialValueEligible(*ordinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidAllocationType,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "C++ type '" + std::string(element.substr(5)) +
                        "' is not trivially copyable and destructible, so a NativeSharp value (struct field, array "
                        "element or generic argument) cannot hold it; hold it in a class field or a local, or by pointer");
                cppElementValid = false;
            }
    bool elementsValid = true;
    if (inferredInitializer)
    {
        for (const ast::AstNode* initializer : expression->children())
        {
            const ExpressionResult value = analyzeExpression(
                initializer, methodMemberOrdinal, scopeOrdinal, diagnostics,
                /*allowMethodGroup=*/false, /*allowEvent=*/false, /*allowMove=*/true);
            if (value.pointerPointeeVolatile && ProjectTypeConversion::isPointer(element))
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error, initializer->range(),
                    "array element cannot yet preserve pointer pointee volatile qualification");
                elementsValid = false;
                continue;
            }
            if (value.valid && value.readonlyBorrowedObject)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    initializer->range(),
                    "readonly borrowed value cannot be stored in an array without lifetime tracking");
                elementsValid = false;
                continue;
            }
            if (!value.valid || !classifyImportedFlexibleConversion(value, element).exists())
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
        countValid && elementsValid && cppElementValid;
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
    if (allocationValid && nameLookup_ != nullptr && element.starts_with("type:"))
    {
        const auto ordinal = nameLookup_->typeOrdinalForIdentity(element.substr(5));
        if (ordinal && nameLookup_->cppImportedTypeIndex(*ordinal))
        {
            diagnostics.report(
                isStackalloc ? diagnostics::DiagnosticId::InvalidStackalloc
                             : diagnostics::DiagnosticId::InvalidAllocationType,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "C/C++ object lifetime is not implemented for raw allocation of '" +
                    std::string(element) + "'");
            return false;
        }
    }
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
            // EN: Freeze DEC-06: `delete[]` of a local declared `owned T[]` consumes it. The
            //     local is moved-from afterwards (a later read or delete is a use of a moved value),
            //     the operation clears its slot and the lexical cleanup skips it, so it is destroyed
            //     exactly once on every path. `raii T[]` keeps lexical cleanup as its only owner.
            // ES: Congelamiento DEC-06: `delete[]` de un local declarado `owned T[]` lo consume.
            //     Despues el local queda movido (leerlo o borrarlo otra vez es usar un valor movido),
            //     la operacion vacia su slot y la limpieza lexica lo omite, asi se destruye
            //     exactamente una vez en todo camino. `raii T[]` conserva la limpieza lexica como
            //     unico dueno.
            std::optional<std::size_t> consumedOwnedArrayLocal;
            if (expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const std::optional<ScopeName> target = lookupScopeName(scopeOrdinal, expression->first()->text());
                deletesRaiiLocal = target.has_value() && target->kind == ScopeNameKind::Local &&
                    target->semanticOrdinal < locals_.size() && locals_[target->semanticOrdinal].isRaii;
                if (deletesRaiiLocal)
                {
                    const MethodLocalSymbol& local = locals_[target->semanticOrdinal];
                    const bool ownedArrayLocal = expression->flag() && !local.isPinned &&
                        ProjectTypeConversion::isNativeArray(local.typeKey) && local.syntax != nullptr &&
                        local.syntax->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
                        local.syntax->typeSyntax() != nullptr &&
                        local.syntax->typeSyntax()->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::Owned;
                    if (ownedArrayLocal) consumedOwnedArrayLocal = target->semanticOrdinal;
                    deletesRaiiLocal = !ownedArrayLocal;
                }
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
            const MethodNameBinding* deletedName = expression->first() != nullptr
                ? nameBinding(expression->first(), methodMemberOrdinal) : nullptr;
            const bool deletesBorrowedFieldObject = operand.valid &&
                expression->first() != nullptr &&
                isReferenceTypeKey(operand.typeKey) &&
                ((expression->first()->kind() == ast::AstNodeKind::MemberExpression &&
                  hasActiveViewOfStorage(expression->first(), methodMemberOrdinal, scopeOrdinal)) ||
                 (expression->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
                  hasActiveViewOfStorage(expression->first(), methodMemberOrdinal, scopeOrdinal, true)) ||
                 (expression->first()->kind() == ast::AstNodeKind::IndexExpression &&
                  hasActiveViewOfStorage(expression->first(), methodMemberOrdinal, scopeOrdinal, true)) ||
                 ((expression->first()->kind() == ast::AstNodeKind::CallExpression ||
                   (deletedName != nullptr && (deletedName->kind == MethodNameBindingKind::StaticField ||
                                               deletedName->kind == MethodNameBindingKind::NamespaceState))) &&
                  std::ranges::any_of(locals_, [&](const MethodLocalSymbol& local)
                  {
                      return local.methodMemberOrdinal == methodMemberOrdinal &&
                          scopeContains(local.scopeOrdinal, scopeOrdinal) && !local.viewSourceFields.empty();
                  })));
            if (deletesBorrowedFieldObject)
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "cannot delete owner field object while a borrowed view is alive");
            const bool deletesBorrowedArray = operand.valid && expression->flag() &&
                ProjectTypeConversion::isNativeArray(operand.typeKey) &&
                !consumedOwnedArrayLocal.has_value() &&
                (hasActiveViewOfStorage(expression->first(), methodMemberOrdinal, scopeOrdinal, true, expression) ||
                 ((expression->first()->kind() == ast::AstNodeKind::CallExpression ||
                   (deletedName != nullptr && (deletedName->kind == MethodNameBindingKind::StaticField ||
                                               deletedName->kind == MethodNameBindingKind::NamespaceState))) &&
                  std::ranges::any_of(locals_, [&](const MethodLocalSymbol& local)
                  {
                      return local.methodMemberOrdinal == methodMemberOrdinal &&
                          scopeContains(local.scopeOrdinal, scopeOrdinal) && !local.viewSourceFields.empty();
                  })));
            if (deletesBorrowedArray)
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, expression->range(),
                    "cannot delete[] array while a borrowed view is alive");
            // LKF-063: a `ref` local bound to an element of the deleted array would read freed storage.
            // The element alias is recognized from the ref local's initializer (`ref a[i]`, possibly
            // through member accesses) resolving to the same array variable as the deleted operand.
            bool deletesArrayWithElementReference = false;
            if (operand.valid && expression->flag() && ProjectTypeConversion::isNativeArray(operand.typeKey) &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const std::optional<ScopeName> deletedArray =
                    lookupScopeName(scopeOrdinal, expression->first()->text());
                // EN: LKF-325: the element reference was matched by the array's name only, so
                //     `int[] b = a; ref int x = ref a[0]; delete[] b;` freed the storage `x` reads. The
                //     locals that may share the deleted array's storage, through copies in either
                //     direction, are now treated as the same array.
                // ES: LKF-325: la referencia al elemento se emparejaba solo por el nombre del arreglo,
                //     así que `int[] b = a; ref int x = ref a[0]; delete[] b;` liberaba lo que lee `x`.
                //     Los locales que pueden compartir el almacenamiento del arreglo borrado, por copias
                //     en cualquier sentido, se tratan ahora como el mismo arreglo.
                std::unordered_set<std::size_t> sharedStorageLocals;
                if (deletedArray.has_value() && deletedArray->kind == ScopeNameKind::Local)
                {
                    std::vector<std::size_t> pending{deletedArray->semanticOrdinal};
                    while (!pending.empty())
                    {
                        const std::size_t current = pending.back();
                        pending.pop_back();
                        if (current >= locals_.size() || !sharedStorageLocals.insert(current).second) continue;
                        for (const std::size_t source : locals_[current].possibleReferenceSourceLocals)
                            pending.push_back(source);
                        for (std::size_t other = 0; other < locals_.size(); ++other)
                        {
                            const auto& sources = locals_[other].possibleReferenceSourceLocals;
                            if (std::find(sources.begin(), sources.end(), current) != sources.end())
                                pending.push_back(other);
                        }
                    }
                }
                for (const MethodLocalSymbol& alias : locals_)
                {
                    if (!deletedArray.has_value() || deletesArrayWithElementReference) break;
                    if (alias.methodMemberOrdinal != methodMemberOrdinal || !alias.isReference ||
                        !scopeContains(alias.scopeOrdinal, scopeOrdinal) || alias.syntax == nullptr) continue;
                    const ast::AstNode* root = alias.syntax->first();
                    bool throughElement = false;
                    for (std::size_t step = 0; step < 16U && root != nullptr &&
                         root->kind() != ast::AstNodeKind::IdentifierExpression; ++step)
                    {
                        if (root->kind() == ast::AstNodeKind::IndexExpression) throughElement = true;
                        root = root->first();
                    }
                    if (!throughElement || root == nullptr ||
                        root->kind() != ast::AstNodeKind::IdentifierExpression) continue;
                    const std::optional<ScopeName> aliasedArray = lookupScopeName(alias.scopeOrdinal, root->text());
                    const bool sameArray = aliasedArray.has_value() && aliasedArray->kind == deletedArray->kind &&
                        aliasedArray->semanticOrdinal == deletedArray->semanticOrdinal;
                    const bool sharedArray = aliasedArray.has_value() && aliasedArray->kind == ScopeNameKind::Local &&
                        sharedStorageLocals.contains(aliasedArray->semanticOrdinal);
                    if (!sameArray && !sharedArray) continue;
                    // EN: A reference whose last use precedes the delete is no longer alive.
                    // ES: Una referencia cuyo ultimo uso precede al delete ya no esta viva.
                    const ClassMemberSymbol* enclosing = methodSymbol(methodMemberOrdinal);
                    if (enclosing != nullptr && enclosing->syntax != nullptr &&
                        !identifierMayBeReadAfter(enclosing->syntax, alias.name, expression, alias.syntax)) continue;
                    deletesArrayWithElementReference = true;
                    diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error, expression->range(),
                        "cannot delete[] array '" + expression->first()->text() + "' while reference '" +
                            alias.name + "' to one of its elements is alive");
                }
            }
            // EN: LKF-337: a reference returned by a ref method of an object, or taken from an element of
            //     its array field, stays valid only while that field's buffer lives. `delete[] b.Data`
            //     with such a reference still read afterwards left it reading freed storage.
            // ES: LKF-337: una referencia devuelta por un método ref de un objeto, o tomada de un elemento
            //     de su campo arreglo, vale solo mientras vive el búfer de ese campo. `delete[] b.Data`
            //     con esa referencia leída después la dejaba leyendo memoria liberada.
            if (!deletesArrayWithElementReference && operand.valid && expression->flag() &&
                ProjectTypeConversion::isNativeArray(operand.typeKey) && expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::MemberExpression)
            {
                const auto pathOf = [](const ast::AstNode* node) -> std::optional<std::string>
                {
                    std::vector<std::string> parts;
                    while (node != nullptr && node->kind() == ast::AstNodeKind::MemberExpression)
                    {
                        parts.push_back(node->text());
                        node = node->first();
                    }
                    if (node == nullptr || node->kind() != ast::AstNodeKind::IdentifierExpression) return std::nullopt;
                    std::string path = node->text();
                    for (auto part = parts.rbegin(); part != parts.rend(); ++part) path += "." + *part;
                    return path;
                };
                const std::optional<std::string> deletedPath = pathOf(expression->first());
                const std::string owner = deletedPath.has_value()
                    ? deletedPath->substr(0, deletedPath->find('.')) : std::string{};
                for (const MethodLocalSymbol& alias : locals_)
                {
                    if (!deletedPath.has_value() || deletesArrayWithElementReference) break;
                    if (alias.methodMemberOrdinal != methodMemberOrdinal || !alias.isReference ||
                        !scopeContains(alias.scopeOrdinal, scopeOrdinal) || alias.syntax == nullptr) continue;
                    bool derived = false;
                    const ast::AstNode* node = alias.syntax->first();
                    for (std::size_t step = 0; step < 16U && node != nullptr && !derived; ++step)
                    {
                        if (node->kind() == ast::AstNodeKind::IndexExpression && pathOf(node->first()) == deletedPath)
                            derived = true;
                        else if (node->kind() == ast::AstNodeKind::CallExpression && node->first() != nullptr &&
                            node->first()->kind() == ast::AstNodeKind::MemberExpression &&
                            pathOf(node->first()->first()) == owner)
                            derived = true;
                        node = node->first();
                    }
                    if (!derived) continue;
                    const ClassMemberSymbol* enclosing = methodSymbol(methodMemberOrdinal);
                    if (enclosing != nullptr && enclosing->syntax != nullptr &&
                        !identifierMayBeReadAfter(enclosing->syntax, alias.name, expression, alias.syntax)) continue;
                    deletesArrayWithElementReference = true;
                    diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error, expression->range(),
                        "cannot delete[] '" + *deletedPath + "' while reference '" + alias.name +
                            "', which may point into it through '" + owner + "', is alive");
                }
            }
            bool deletesBorrowedDelegate = false;
            if (operand.valid && !expression->flag() && isDelegateTypeKey(operand.typeKey) &&
                !isExternCDelegateTypeKey(operand.typeKey) && expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const auto target = lookupScopeName(scopeOrdinal, expression->first()->text());
                if (target.has_value() && target->kind == ScopeNameKind::Local &&
                    target->semanticOrdinal < locals_.size())
                {
                    // Local delegate assignments may alias in either direction. Preserve all
                    // known source edges and inspect the connected component before destruction.
                    std::vector<std::vector<std::size_t>> aliases(locals_.size());
                    for (std::size_t ordinal = 0; ordinal < locals_.size(); ++ordinal)
                    {
                        const MethodLocalSymbol& local = locals_[ordinal];
                        if (local.methodMemberOrdinal != methodMemberOrdinal ||
                            !isDelegateTypeKey(local.typeKey)) continue;
                        std::vector<const ast::AstNode*> sources = local.possibleDelegateSources;
                        if (local.syntax != nullptr) sources.push_back(local.syntax->first());
                        while (!sources.empty())
                        {
                            const ast::AstNode* source = sources.back();
                            sources.pop_back();
                            if (source == nullptr) continue;
                            if (source->kind() == ast::AstNodeKind::ConditionalExpression)
                            {
                                sources.push_back(source->second());
                                sources.push_back(source->third());
                                continue;
                            }
                            const MethodNameBinding* binding = nameBinding(source, methodMemberOrdinal);
                            if (binding == nullptr || binding->kind != MethodNameBindingKind::Local ||
                                !binding->localOrdinal.has_value() || *binding->localOrdinal >= locals_.size() ||
                                !isDelegateTypeKey(locals_[*binding->localOrdinal].typeKey)) continue;
                            aliases[ordinal].push_back(*binding->localOrdinal);
                            aliases[*binding->localOrdinal].push_back(ordinal);
                        }
                    }
                    std::vector<std::size_t> pending{target->semanticOrdinal};
                    std::unordered_set<std::size_t> visited;
                    while (!pending.empty())
                    {
                        const std::size_t ordinal = pending.back();
                        pending.pop_back();
                        if (!visited.insert(ordinal).second) continue;
                        if (hasActiveBorrowOfOwnerLocal(ordinal, scopeOrdinal))
                        {
                            deletesBorrowedDelegate = true;
                            break;
                        }
                        pending.insert(pending.end(), aliases[ordinal].begin(), aliases[ordinal].end());
                    }
                }
            }
            if (deletesBorrowedDelegate)
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, expression->range(),
                    "cannot delete delegate while a borrowed local is alive");
            const bool parameterConsumable = !operand.valid ||
                consumeDeletedOwnedParameter(expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
            recordPlainArrayLocalDelete(expression, scopeOrdinal);
            bool consumesBorrowedOwner = false;
            if (consumedOwnedArrayLocal.has_value() && operand.valid)
            {
                const std::size_t ordinal = *consumedOwnedArrayLocal;
                consumesBorrowedOwner = hasActiveBorrowOfOwnerLocal(ordinal, scopeOrdinal);
                if (consumesBorrowedOwner)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "cannot delete[] owned local '" + locals_[ordinal].name +
                            "' while a borrowed local is alive");
                }
                else
                {
                    arrayDeleteClearsOwnedField_.insert(expression);
                    maybeMovedRaiiLocals_.erase(ordinal);
                    movedRaiiLocals_.insert(ordinal);
                    invalidateHistoricalBorrowsOfOwnerLocal(ordinal);
                }
            }
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
            // LKF-071: outside a destructor, `this` is borrowed by the call; the object belongs to the
            // owner that invoked the method (a raii local, an owned field or slot), which still
            // destroys it, so `delete this` would release it twice.
            const bool deletesBorrowedThis = !deletesCurrentThis && expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::ThisExpression;
            if (deletesBorrowedThis)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "delete this is forbidden; the method borrows its receiver and the object's owner "
                    "releases it");
            }
            // LKF-020/LKF-021: three deletes that can never be valid are rejected here.
            // - A borrowed parameter or local is owned by someone else; deleting it frees storage the
            //   owner still destroys.
            // - A pointer whose value is the address of a local or of `stackalloc` storage was never
            //   allocated on the heap; freeing it corrupts the heap.
            // - A dynamic array is released with `delete[]`; the scalar form does not run the array
            //   release protocol.
            bool deletesForeignStorage = false;
            if (operand.valid && expression->first() != nullptr)
            {
                std::string reason;
                if (expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
                {
                    const auto target = lookupScopeName(scopeOrdinal, expression->first()->text());
                    if (target.has_value() && target->kind == ScopeNameKind::Local &&
                        target->semanticOrdinal < locals_.size() &&
                        locals_[target->semanticOrdinal].ownership == sema::OwnershipKind::Borrowed)
                        reason = "borrowed local '" + locals_[target->semanticOrdinal].name +
                            "' is owned elsewhere and cannot be deleted";
                    if (target.has_value() && target->kind == ScopeNameKind::Parameter && context != nullptr &&
                        target->semanticOrdinal < context->parameters.size())
                    {
                        const MethodParameterSymbol& parameter = context->parameters[target->semanticOrdinal];
                        if (parameter.ownership == sema::OwnershipKind::Borrowed ||
                            parameter.passKind == ParameterPassKind::In)
                            reason = "borrowed parameter '" + parameter.name +
                                "' is owned by the caller and cannot be deleted";
                    }
                }
                if (reason.empty() && operand.typeKey.ends_with("*") &&
                    pointerExpressionAddressesCurrentFrame(expression->first(), scopeOrdinal))
                    reason = "a pointer to current-frame storage (a local or stackalloc) was not allocated "
                        "with new and cannot be deleted";
                if (reason.empty() && !expression->flag() && ProjectTypeConversion::isNativeArray(operand.typeKey))
                {
                    // EN: Generic code releases every element with one `delete` written over T (the Stage 52
                    //     contract of the collections); when T is a dynamic array that release is `delete[]`.
                    //     Only a scalar delete written outside a generic specialization is rejected.
                    // ES: El codigo generico libera cada elemento con un `delete` escrito sobre T (el contrato
                    //     Stage 52 de las colecciones); cuando T es un arreglo dinamico esa liberacion es
                    //     `delete[]`. Solo se rechaza un delete escalar escrito fuera de una especializacion.
                    const bool genericSpecialization = context != nullptr &&
                        (context->genericTemplateOrdinal.has_value() ||
                         callableBelongsToGenericTypeSpecialization(methodMemberOrdinal));
                    if (genericSpecialization)
                        scalarDeleteReleasesArray_.insert(expression);
                    else
                        reason = "a dynamic array is released with delete[], not with scalar delete";
                }
                // EN: LKF-020. A local initialized by `new (storage) T(...)` over raw storage lives in that
                //     storage; delete would hand it to the heap. An allocator placement keeps its own release.
                // ES: LKF-020. Un local inicializado con `new (storage) T(...)` sobre almacenamiento crudo
                //     vive en ese almacenamiento; delete lo entregaria al heap. La colocacion con allocator
                //     conserva su propia liberacion.
                if (reason.empty() && expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
                {
                    const auto target = lookupScopeName(scopeOrdinal, expression->first()->text());
                    const ast::AstNode* initializer = target.has_value() && target->kind == ScopeNameKind::Local &&
                        target->semanticOrdinal < locals_.size() && locals_[target->semanticOrdinal].syntax != nullptr
                        ? locals_[target->semanticOrdinal].syntax->first() : nullptr;
                    if (initializer != nullptr &&
                        initializer->kind() == ast::AstNodeKind::PlacementConstructionExpression &&
                        initializer->first() != nullptr &&
                        recordedExpressionTypeKey(initializer->first(), methodMemberOrdinal).ends_with("*"))
                        reason = "'" + expression->first()->text() + "' was constructed in place with new (storage) "
                            "and was not allocated with new; end its lifetime with destroy";
                }
                if (!reason.empty())
                {
                    deletesForeignStorage = true;
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        reason);
                }
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
                // EN: Freeze (owned arrays): an owned array field is released by its container
                //     now; `delete[]` on it stays the manual spelling, and it clears the field.
                // ES: Congelamiento (arreglos owned): el contenedor ahora libera un campo de
                //     arreglo owned; `delete[]` sobre el sigue siendo la forma manual, y vacia el campo.
                if (deletedMember.kind == ClassMemberKind::Field && !deletedMember.isStatic &&
                    deletedMember.declaredOwnership == sema::OwnershipKind::Owned &&
                    ProjectTypeConversion::isNativeArray(deletedMember.declaredTypeKey))
                    arrayDeleteClearsOwnedField_.insert(expression);
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
            result.valid = operand.valid && !deletesCurrentThis && !deletesBorrowedThis && !deletesRaiiLocal &&
                !deletesBorrowedDelegate &&
                !deletesOwnedField && !deletesBorrowedFieldObject && !deletesBorrowedArray &&
                !consumesBorrowedOwner && parameterConsumable && !deletesForeignStorage &&
                !deletesArrayWithElementReference;
            break;
        }
        default:
            break;
    }
    return result;
}

/// EN: Freeze (item 130, F2). Marks `delete[] a` when `a` is a local declared without owned/raii whose
///     elements are class or interface references: the array does not own them, so only the block is
///     released. Struct elements (DEC-11) and owned/raii locals keep their behavior.
/// ES: Congelamiento (elemento 130, F2). Marca `delete[] a` cuando `a` es un local declarado sin
///     owned/raii cuyos elementos son referencias de clase o interfaz: el arreglo no los posee, asi que
///     solo se libera el bloque. Los elementos struct (DEC-11) y los locales owned/raii no cambian.
void ProjectMethodBodyModel::recordPlainArrayLocalDelete(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal)
{
    const ast::AstNode* operand = expression->first();
    if (!expression->flag() || operand == nullptr ||
        operand->kind() != ast::AstNodeKind::IdentifierExpression) return;
    const std::optional<ScopeName> target = lookupScopeName(scopeOrdinal, operand->text());
    if (!target.has_value() || target->kind != ScopeNameKind::Local ||
        target->semanticOrdinal >= locals_.size()) return;
    const MethodLocalSymbol& local = locals_[target->semanticOrdinal];
    if (local.isRaii || local.isReference) return;
    const std::optional<std::string> element = ProjectTypeConversion::nativeArrayElementType(local.typeKey);
    if (!element.has_value() || !isReferenceTypeKey(*element)) return;
    arrayDeleteKeepsElements_.insert(expression);
}

/// EN: Freeze (item 121). `delete p` of an owned class or interface parameter consumes `p`, like
///     move(p): it is moved-from afterwards and the HIR clears its slot before destruction. Returns
///     false after reporting a delete of a moved parameter or of one a borrowed local still uses.
/// ES: Congelamiento (elemento 121). `delete p` de un parametro owned de clase o interfaz consume
///     `p`, como move(p): despues queda movido y el HIR vacia su slot antes de la destruccion. Devuelve
///     false despues de reportar el delete de un parametro movido o de uno que un local prestado usa.
bool ProjectMethodBodyModel::consumeDeletedOwnedParameter(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::AstNode* operand = expression->first();
    if (operand == nullptr || operand->kind() != ast::AstNodeKind::IdentifierExpression) return true;
    const std::optional<ScopeName> target = lookupScopeName(scopeOrdinal, operand->text());
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (!target.has_value() || target->kind != ScopeNameKind::Parameter || method == nullptr ||
        target->semanticOrdinal >= method->parameters.size()) return true;
    const MethodParameterSymbol& parameter = method->parameters[target->semanticOrdinal];
    // EN: `delete[] a` of an owned array parameter consumes `a` the same way; without it the
    //     parameter's exit drop released the block a second time.
    // ES: `delete[] a` de un parametro arreglo owned consume `a` igual; sin esto la liberacion de
    //     salida del parametro liberaba el bloque una segunda vez.
    const bool arrayDelete = expression->flag();
    if (parameter.ownership != sema::OwnershipKind::Owned) return true;
    if (arrayDelete ? !ProjectTypeConversion::isNativeArray(parameter.typeKey)
                    : (parameter.typeKey != "builtin:string" && !isReferenceTypeKey(parameter.typeKey)))
        return true;
    if (!validateParameterMoveState(
            parameter, expression, target->semanticOrdinal, scopeOrdinal, true, diagnostics))
        return false;
    if (arrayDelete)
        arrayDeleteClearsOwnedField_.insert(expression);
    else
        deleteClearsOwnedParameter_.insert(expression);
    maybeMovedOwnedParameters_.erase(target->semanticOrdinal);
    movedOwnedParameters_.insert(target->semanticOrdinal);
    invalidateHistoricalBorrowsOfOwnedParameter(target->semanticOrdinal);
    return true;
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
            const MethodNameBinding* destroyedName = expression->first() != nullptr
                ? nameBinding(expression->first(), methodMemberOrdinal) : nullptr;

            const bool destroysBorrowedFieldObject = operand.valid &&
                expression->first() != nullptr && isReferenceTypeKey(operand.typeKey) &&
                ((expression->first()->kind() == ast::AstNodeKind::MemberExpression &&
                  hasActiveViewOfStorage(expression->first(), methodMemberOrdinal, scopeOrdinal)) ||
                 (expression->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
                  hasActiveViewOfStorage(expression->first(), methodMemberOrdinal, scopeOrdinal, true)) ||
                 (expression->first()->kind() == ast::AstNodeKind::IndexExpression &&
                  hasActiveViewOfStorage(expression->first(), methodMemberOrdinal, scopeOrdinal, true)) ||
                 ((expression->first()->kind() == ast::AstNodeKind::CallExpression ||
                   (destroyedName != nullptr && (destroyedName->kind == MethodNameBindingKind::StaticField ||
                                                 destroyedName->kind == MethodNameBindingKind::NamespaceState))) &&
                  std::ranges::any_of(locals_, [&](const MethodLocalSymbol& local)
                  {
                      return local.methodMemberOrdinal == methodMemberOrdinal &&
                          scopeContains(local.scopeOrdinal, scopeOrdinal) && !local.viewSourceFields.empty();
                  })));
            if (destroysBorrowedFieldObject)
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "cannot destroy owner field object while a borrowed view is alive");

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

            // Field reads can be non-owning expressions even when their slot
            // retains ownership. Destruction through the slot would leave its
            // owner responsible for an object whose lifetime has already ended.
            bool ownedField = false;
            if (operand.valid && operand.assignmentMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *operand.assignmentMemberOrdinal < memberModel_->members().size())
            {
                const ClassMemberSymbol& member = memberModel_->members()[*operand.assignmentMemberOrdinal];
                ownedField = member.kind == ClassMemberKind::Field &&
                    member.declaredOwnership == sema::OwnershipKind::Owned;
            }
            const bool destroysOwnedValue = operand.valid && !destroysRaiiLocal &&
                (operand.ownership == sema::OwnershipKind::Owned || ownedField);
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
                !destroysCurrentThis && !destroysOwnedValue && !destroysBorrowedFieldObject;
            break;
        }
        default:
            break;
    }
    return result;
}

}
