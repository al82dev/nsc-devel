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

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::analyzeExpressionKindUnaryExpressionOrPostfixExpression(
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
        case ast::AstNodeKind::UnaryExpression:
        case ast::AstNodeKind::PostfixExpression:
            result = analyzeUnary(expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindBinaryExpression(
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
        case ast::AstNodeKind::BinaryExpression:
            result = analyzeBinary(expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindNullCoalescingExpression(
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
        case ast::AstNodeKind::NullCoalescingExpression:
        {
            // T01-F03. The left operand is unconditional; the right operand is a real branch.
            // Keeping the branch in the ownership/definite-assignment model matters just as much
            // as keeping it in LLVM: a move or assignment on the fallback edge did not happen on
            // the non-null edge.
            const ExpressionResult left = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            std::optional<MethodOptionalCarrierBinding> optionalCarrier;
            const auto nullCarrier = [&](std::string_view typeKey)
            {
                if (ProjectTypeConversion::isPointer(typeKey) ||
                    ProjectTypeConversion::isNativeArray(typeKey) ||
                    typeKey == "builtin:string" || isAnyDelegateTypeKey(typeKey))
                {
                    return true;
                }
                optionalCarrier = resolveOptionalCarrier(typeKey);
                if (optionalCarrier.has_value()) return true;
                const std::optional<std::size_t> ordinal = referenceTypeOrdinalForTypeKey(typeKey);
                return ordinal.has_value() && inheritanceModel_ != nullptr &&
                    (inheritanceModel_->isClass(*ordinal) ||
                     (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)));
            };
            const bool leftIsCarrier = left.valid && nullCarrier(left.typeKey);
            if (left.valid && !leftIsCarrier)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->first() != nullptr ? expression->first()->range() : expression->range(),
                    "left operand of '?' '?' must be a pointer, reference, delegate, native array, "
                    "native string, or a supported Optional<T> carrier");
            }

            const OwnershipFlowSnapshot branchEntry = captureOwnershipFlow();
            ExpressionResult right;
            const OwnershipFlowSnapshot rightState = analyzeBranchState(
                branchEntry,
                [&]
                {
                    right = analyzeExpression(
                        expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                });
            commitBranchJoin(branchEntry, rightState);
            if (!left.valid || !right.valid || !leftIsCarrier) break;

            const std::string resultType = optionalCarrier.has_value()
                ? optionalCarrier->valueTypeKey : left.typeKey;
            const ProjectImplicitConversion conversion = classifyImplicit(
                right.typeKey, resultType, right.integerConstant);
            if (!conversion.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->second() != nullptr ? expression->second()->range() : expression->range(),
                    "right operand of '?' '?' with type '" + right.typeKey +
                        "' is not implicitly convertible to '" + resultType + "'");
                break;
            }
            if (optionalCarrier.has_value())
            {
                optionalCarrier->syntax = expression;
                optionalCarrier->methodMemberOrdinal = methodMemberOrdinal;
                optionalCarrier->scopeOrdinal = scopeOrdinal;
                optionalCarrierBindings_.push_back(std::move(*optionalCarrier));
            }
            result.typeKey = resultType;
            result.valid = true;
            result.assignable = false;
            // EN: Freeze 3.0 (plan item 135): the result may be either operand, so it keeps a borrow of either.
            // ES: Congelamiento 3.0 (elemento 135 del plan): el resultado puede ser cualquier operando, asi que
            //     conserva el prestamo de cualquiera.
            result.borrowsOwnedTemporary = left.borrowsOwnedTemporary || right.borrowsOwnedTemporary;
            break;
        }
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindTupleExpression(
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
        case ast::AstNodeKind::TupleExpression:
        {
            std::vector<std::string> elementTypeKeys;
            elementTypeKeys.reserve(expression->children().size());
            MethodAggregateInitializerBinding binding;
            binding.syntax = expression;
            bool valid = true;
            for (std::size_t index = 0; index < expression->children().size(); ++index)
            {
                const ast::AstNode* element = expression->children()[index];
                const ExpressionResult value = analyzeExpression(
                    element, methodMemberOrdinal, scopeOrdinal, diagnostics);
                valid = valid && value.valid;
                elementTypeKeys.push_back(value.valid ? value.typeKey : std::string("<error>"));
                MethodAggregateInitializerEntryBinding entry;
                entry.syntax = element;
                entry.targetElementOrdinals.push_back(index);
                binding.entries.push_back(std::move(entry));
            }
            binding.targetTypeKey = ProjectTypeConversion::tupleTypeKey(elementTypeKeys);
            aggregateInitializerBindingBySyntax_[expression] = aggregateInitializerBindings_.size();
            aggregateInitializerBindings_.push_back(std::move(binding));
            result.typeKey = ProjectTypeConversion::tupleTypeKey(elementTypeKeys);
            result.valid = valid;
            result.assignable = false;
            break;
        }
        default:
            break;
    }
    return result;
}

const ClassMemberSymbol* ProjectMethodBodyModel::withUpdateField(
    std::size_t sourceTypeOrdinal,
    std::string_view fieldName) const
{
    if (memberModel_ == nullptr) return nullptr;
    for (const std::size_t fieldOrdinal : memberModel_->membersOfType(sourceTypeOrdinal))
    {
        if (fieldOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& candidate = memberModel_->members()[fieldOrdinal];
        if (candidate.kind == ClassMemberKind::Field && !candidate.isStatic &&
            !candidate.isConst && !candidate.isUnnamedBitField && candidate.name == fieldName)
        {
            return &candidate;
        }
    }
    return nullptr;
}

void ProjectMethodBodyModel::bindWithUpdate(
    MethodAggregateInitializerBinding& binding,
    std::vector<bool>& assigned,
    bool& valid,
    const ast::AstNode* expression,
    const ast::AstNode* update,
    const ExpressionResult& source,
    std::span<const std::string> elements,
    std::span<const std::string> names,
    std::optional<std::size_t> sourceTypeOrdinal,
    const ClassMemberSymbol* caller,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    MethodAggregateInitializerEntryBinding entry;
    entry.syntax = update;
    const auto found = update != nullptr
        ? std::ranges::find(names, update->argumentName()) : names.end();
    if (update == nullptr || found == names.end())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::UnknownMember,
            diagnostics::DiagnosticSeverity::Error,
            update != nullptr ? update->range() : expression->range(),
            "with source '" + source.typeKey + "' has no field named '" +
                (update != nullptr ? update->argumentName() : std::string{}) + "'");
        valid = false;
        binding.entries.push_back(std::move(entry));
        return;
    }
    const std::size_t target = static_cast<std::size_t>(std::distance(names.begin(), found));
    if (assigned[target])
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            update->range(),
            "with member '" + update->argumentName() + "' is assigned more than once");
        valid = false;
    }
    assigned[target] = true;
    entry.targetElementOrdinals.push_back(target);
    // A `with` update is construction of a new value, so a readonly field may be
    // initialized here. Accessibility is independent: copying the complete carrier
    // never exposes a private field, but naming that field as an update is a direct
    // source-level access and must obey the same rule as `receiver.Field = value`.
    const ClassMemberSymbol* targetField = sourceTypeOrdinal.has_value()
        ? withUpdateField(*sourceTypeOrdinal, names[target]) : nullptr;
    if (targetField == nullptr || caller == nullptr || nameLookup_ == nullptr ||
        inheritanceModel_ == nullptr || !sourceTypeOrdinal.has_value() ||
        !ProjectMemberAccessibility::isAccessible(
            *targetField,
            caller->declaringTypeOrdinal,
            *sourceTypeOrdinal,
            *nameLookup_,
            *inheritanceModel_))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InaccessibleMember,
            diagnostics::DiagnosticSeverity::Error,
            update->range(),
            "with member '" + update->argumentName() + "' is not accessible in this context");
        valid = false;
    }
    const ExpressionResult value = analyzeExpression(
        update, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (!value.valid || !classifyImplicit(
            value.typeKey, elements[target], value.integerConstant).exists())
    {
        if (value.valid)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                update->range(),
                "with member '" + update->argumentName() + "' of type '" +
                    elements[target] + "' cannot be updated from '" + value.typeKey + "'");
        }
        valid = false;
    }
    binding.entries.push_back(std::move(entry));
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindWithExpression(
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
        case ast::AstNodeKind::WithExpression:
        {
            // T01-F07. `source with { A = x }` is an explicit whole-value copy followed by
            // textual updates. Source is analyzed before every update and exactly once. Until
            // T06 supplies per-field drop flags, only trivially copyable/destructible structs are
            // admitted; accepting an owning aggregate here would turn `with` into an implicit
            // aliasing copy and make exceptional cleanup ambiguous.
            const ExpressionResult source = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!source.valid) break;
            const std::optional<std::vector<std::string>> elements =
                aggregateElementTypeKeys(source.typeKey);
            const std::optional<std::vector<std::string>> names =
                aggregateElementNames(source.typeKey);
            if (!elements.has_value() || !names.has_value() || names->size() != elements->size() ||
                names->empty())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "a with expression requires a named struct value, not '" + source.typeKey + "'");
                break;
            }
            bool valid = true;
            if (!elementTypeHasTrivialLifetime(source.typeKey) ||
                source.ownership == sema::OwnershipKind::Owned || source.ownershipTransfer ||
                source.ownershipFreshOwner)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "with expression for '" + source.typeKey +
                        "' requires a trivially copyable value; move-only or owning values need "
                        "an explicit clone/copy strategy");
                valid = false;
            }

            MethodAggregateInitializerBinding binding;
            binding.syntax = expression;
            binding.targetTypeKey = source.typeKey;
            binding.generalized = true;
            const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
            const std::optional<std::size_t> sourceTypeOrdinal = inheritanceModel_ != nullptr
                ? inheritanceModel_->typeOrdinalForTypeKey(source.typeKey) : std::nullopt;
            std::vector<bool> assigned(elements->size(), false);
            for (const ast::AstNode* update : expression->children())
            {
                bindWithUpdate(
                    binding, assigned, valid, expression, update, source,
                    *elements, *names, sourceTypeOrdinal, caller,
                    methodMemberOrdinal, scopeOrdinal, diagnostics);
            }
            aggregateInitializerBindingBySyntax_[expression] = aggregateInitializerBindings_.size();
            aggregateInitializerBindings_.push_back(std::move(binding));
            result.typeKey = source.typeKey;
            result.valid = valid;
            result.assignable = false;
            break;
        }
        default:
            break;
    }
    return result;
}

void ProjectMethodBodyModel::analyzeConditionalBranches(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    ExpressionResult& whenTrue,
    ExpressionResult& whenFalse)
{
    // `condition ? whenTrue : whenFalse`.
    //
    // EL TIPO. Las dos ramas deben tener el MISMO tipo, o ser ambas numericas -- en cuyo
    // caso el resultado es su tipo comun, la misma regla que usa un operador aritmetico.
    // C# ademas recorre una reticula de conversiones entre tipos referencia no
    // emparentados para encontrar una base comun; eso necesita el modelo de herencia y no
    // es lo que hace util al operador. Un par de ramas que esto no acepta recibe un
    // diagnostico que dice exactamente que dos tipos discreparon, nunca una eleccion
    // silenciosa de una de ellas.
    //
    // EL FLUJO (CC1-203). Las dos ramas se analizan CADA UNA desde el estado que hay tras
    // la condicion, y el estado posterior es la union de las dos. Antes se analizaban
    // secuencialmente, y eso hacia que `c ? (x = 1) : 0` diera `x` por definitivamente
    // asignada: definite assignment es un conjunto MUST cuyo join es una INTERSECCION, y
    // analizar en secuencia equivale a unir, que es la direccion insegura. El programa no
    // explotaba porque C7 pone a cero el almacenamiento del local -- devolvia 0. El backend
    // tapaba el bug del analisis.
    const ExpressionResult condition = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (condition.valid && condition.typeKey != "builtin:bool" &&
        condition.typeKey != "builtin:never")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCondition,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "the condition of a conditional expression must have type bool");
    }
    const OwnershipFlowSnapshot branchEntry = captureOwnershipFlow();
    OwnershipFlowSnapshot trueEntry = branchEntry;
    OwnershipFlowSnapshot falseEntry = branchEntry;
    if (const auto flow = conditionFlowBySyntax_.find(expression->first());
        flow != conditionFlowBySyntax_.end())
    {
        trueEntry = flow->second.whenTrue;
        falseEntry = flow->second.whenFalse;
    }
    const OwnershipFlowSnapshot trueState = analyzeBranchState(trueEntry, [&]
    {
        whenTrue = analyzeExpression(
            expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    });
    const OwnershipFlowSnapshot falseState = analyzeBranchState(falseEntry, [&]
    {
        whenFalse = analyzeExpression(
            expression->third(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    });
    commitBranchJoin(trueState, falseState);
}

void ProjectMethodBodyModel::adoptConditionalBranch(
    ExpressionResult& result,
    const ExpressionResult& branch)
{
    result = branch;
    result.assignable = false;
    result.addressable = false;
}

void ProjectMethodBodyModel::mergeConditionalOwnership(
    ExpressionResult& result,
    const ExpressionResult& whenTrue,
    const ExpressionResult& whenFalse)
{
    // CC1-204. OWNERSHIP DEL RESULTADO. Un ternario cuyas dos ramas devuelven `owned` ES
    // una expresion que transfiere propiedad, y hasta ahora se perdia: solo se copiaban
    // typeKey y valid, asi que `raii string r = c ? MakeA() : MakeB();` no podia
    // reconocerse como inicializacion valida de un RAII.
    //
    // La regla es la conservadora y la unica sana: el resultado solo conserva una propiedad
    // de ownership si AMBAS ramas coinciden en ella. Si difieren, el valor no tiene una
    // procedencia unica que un checker de lifetime pueda seguir, y decir que si la tiene
    // seria peor que no decir nada. El origen concreto (local o parametro) solo se propaga
    // cuando las dos ramas nombran exactamente el mismo, por la misma razon.
    if (whenTrue.ownership != whenFalse.ownership) return;
    result.ownership = whenTrue.ownership;
    result.ownershipTransfer =
        whenTrue.ownershipTransfer && whenFalse.ownershipTransfer;
    if (whenTrue.ownershipSourceLocalOrdinal == whenFalse.ownershipSourceLocalOrdinal)
        result.ownershipSourceLocalOrdinal = whenTrue.ownershipSourceLocalOrdinal;
    if (whenTrue.ownershipSourceParameterOrdinal ==
        whenFalse.ownershipSourceParameterOrdinal)
    {
        result.ownershipSourceParameterOrdinal =
            whenTrue.ownershipSourceParameterOrdinal;
    }
    if (whenTrue.borrowedLocalOrdinal == whenFalse.borrowedLocalOrdinal)
        result.borrowedLocalOrdinal = whenTrue.borrowedLocalOrdinal;
}

void ProjectMethodBodyModel::adoptConditionalNullableBranch(
    ExpressionResult& result,
    const ExpressionResult& nonNull)
{
    result.typeKey = nonNull.typeKey;
    result.valid = true;
    result.pointerPointeeReadonly = nonNull.pointerPointeeReadonly;
    result.pointerNullable = true;
    result.pointerHasProvenance = nonNull.pointerHasProvenance;
    result.pointerKnownAlignment = nonNull.pointerKnownAlignment;
    result.pointerAddressSpace = nonNull.pointerAddressSpace;
    result.onePastEndPointer = nonNull.onePastEndPointer;
    result.ownership = nonNull.ownership;
    result.ownershipTransfer = nonNull.ownershipTransfer;
    result.ownershipSourceLocalOrdinal = nonNull.ownershipSourceLocalOrdinal;
    result.ownershipSourceParameterOrdinal = nonNull.ownershipSourceParameterOrdinal;
    result.borrowedLocalOrdinal = nonNull.borrowedLocalOrdinal;
}

bool ProjectMethodBodyModel::resolveConditionalDivergence(
    ExpressionResult& result,
    const ExpressionResult& whenTrue,
    const ExpressionResult& whenFalse) const
{
    // T07-F04. A divergent arm contributes no value and no ownership state to the
    // join. The other arm therefore determines the conditional's complete result.
    // Two divergent arms retain the bottom type. This happens before equality and
    // numeric inference so `panic()` never becomes a fabricated bool/integer/reference.
    if (whenTrue.typeKey == "builtin:never" && whenFalse.typeKey == "builtin:never")
    {
        result.typeKey = "builtin:never";
        result.valid = true;
        result.assignable = false;
        result.addressable = false;
        return true;
    }
    if (whenTrue.typeKey == "builtin:never")
    {
        adoptConditionalBranch(result, whenFalse);
        return true;
    }
    if (whenFalse.typeKey == "builtin:never")
    {
        adoptConditionalBranch(result, whenTrue);
        return true;
    }
    return false;
}

bool ProjectMethodBodyModel::resolveConditionalMatchingType(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& whenTrue,
    const ExpressionResult& whenFalse,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    if (whenTrue.typeKey != whenFalse.typeKey) return false;
    if (ProjectTypeConversion::isPointer(whenTrue.typeKey) &&
        whenTrue.pointerAddressSpace != whenFalse.pointerAddressSpace)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "conditional pointer branches must use the same address space");
        return true;
    }
    result.typeKey = whenTrue.typeKey;
    result.valid = true;
    result.pointerPointeeReadonly =
        whenTrue.pointerPointeeReadonly || whenFalse.pointerPointeeReadonly;
    result.pointerNullable = whenTrue.pointerNullable || whenFalse.pointerNullable;
    result.pointerHasProvenance =
        whenTrue.pointerHasProvenance && whenFalse.pointerHasProvenance;
    result.pointerKnownAlignment = std::min(
        whenTrue.pointerKnownAlignment, whenFalse.pointerKnownAlignment);
    result.pointerAddressSpace = whenTrue.pointerAddressSpace;
    result.onePastEndPointer = whenTrue.onePastEndPointer || whenFalse.onePastEndPointer;
    mergeConditionalOwnership(result, whenTrue, whenFalse);
    return true;
}

void ProjectMethodBodyModel::resolveConditionalSpecialType(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& whenTrue,
    const ExpressionResult& whenFalse,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    // A string literal already converts implicitly to the native string value everywhere
    // else (assignment, arguments and returns).  The conditional join must use that same
    // conversion instead of treating `string-literal` as an unrelated final type.  This
    // is deliberately narrow: it does not add the C# reference-type best-common-type
    // lattice described above.
    if ((whenTrue.typeKey == "string-literal" && whenFalse.typeKey == "builtin:string") ||
        (whenTrue.typeKey == "builtin:string" && whenFalse.typeKey == "string-literal"))
    {
        result.typeKey = "builtin:string";
        result.valid = true;
        mergeConditionalOwnership(result, whenTrue, whenFalse);
        return;
    }
    // `null` has no standalone runtime representation: in a conditional expression it
    // is contextualized by the pointer/reference type of the opposite branch.  Reuse the
    // ordinary implicit-conversion classifier so this accepts exactly the destinations
    // to which null can already be assigned, rather than inventing a second type rule.
    if (whenTrue.typeKey == "builtin:null" &&
        classifyImplicit(
            whenTrue.typeKey, whenFalse.typeKey, whenTrue.integerConstant).exists())
    {
        adoptConditionalNullableBranch(result, whenFalse);
        return;
    }
    if (whenFalse.typeKey == "builtin:null" &&
        classifyImplicit(
            whenFalse.typeKey, whenTrue.typeKey, whenFalse.integerConstant).exists())
    {
        adoptConditionalNullableBranch(result, whenTrue);
        return;
    }
    const std::optional<std::string> common = ProjectTypeConversion::commonNumericType(
        whenTrue.typeKey, whenFalse.typeKey,
        whenTrue.integerConstant, whenFalse.integerConstant);
    if (common.has_value())
    {
        result.typeKey = *common;
        result.valid = true;
        // Un tipo numerico comun implica que las dos ramas son valores numericos, que no
        // llevan ownership. No hay nada que fusionar.
        return;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "the branches of a conditional expression have incompatible types '" +
            whenTrue.typeKey + "' and '" + whenFalse.typeKey + "'");
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::analyzeExpressionKindConditionalExpression(
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
    ExpressionResult whenTrue;
    ExpressionResult whenFalse;
    analyzeConditionalBranches(
        expression, methodMemberOrdinal, scopeOrdinal, diagnostics, whenTrue, whenFalse);
    if (!whenTrue.valid || !whenFalse.valid) return result;
    if (!resolveConditionalDivergence(result, whenTrue, whenFalse) &&
        !resolveConditionalMatchingType(result, expression, whenTrue, whenFalse, diagnostics))
        resolveConditionalSpecialType(result, expression, whenTrue, whenFalse, diagnostics);
    // EN: Freeze 3.0 (plan item 135): the result is one branch or the other, so it keeps a borrow of either.
    // ES: Congelamiento 3.0 (elemento 135 del plan): el resultado es una rama o la otra, asi que conserva el
    //     prestamo de cualquiera.
    result.borrowsOwnedTemporary =
        result.valid && (whenTrue.borrowsOwnedTemporary || whenFalse.borrowsOwnedTemporary);
    return result;
}
}
