#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "Support/OwnershipArgumentClassification.h"
#include "Support/OperatorClassification.h"
#include "Support/IntegralConstantArithmetic.h"
#include "Support/OwnershipFlowSnapshotQueries.h"


#include "MethodBodyInternals.h"
#include "CppVolatileAliasContract.h"
#include "nativesharp/Frontend/ReadonlyReferenceReturn.h"
#include "../Internal/Stage37ConstantValue.h"

#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/CppImportedEnumNumeric.h"

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
using lexer::TokenKind;

using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;

void ProjectMethodBodyModel::applyIdentifierFlowFacts(
    ExpressionResult& result,
    std::size_t storageKey) const
{
        result.flowProvenNonNull = nonNullStorages_.contains(storageKey);
        if (const auto range = integerRanges_.find(storageKey); range != integerRanges_.end())
        {
            result.flowIntegerMinimum = range->second.minimum;
            result.flowIntegerMaximum = range->second.maximum;
            result.flowIntegerExcludesZero = range->second.excludesZero ||
                (range->second.minimum.has_value() && *range->second.minimum > 0) ||
                (range->second.maximum.has_value() && *range->second.maximum < 0);
        }
        if (const auto activeCase = activeVariantCases_.find(storageKey);
            activeCase != activeVariantCases_.end())
            result.flowActiveVariantCase = activeCase->second;

}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeLocalIdentifier(
    const ast::AstNode* expression,
    const ScopeName& scopeName,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (scopeName.semanticOrdinal >= locals_.size()) return result;
    const MethodLocalSymbol& local = locals_[scopeName.semanticOrdinal];
    if (!validateLocalIdentifierRead(expression, scopeName, local, diagnostics)) return result;
    result.typeKey = local.typeKey;
    result.callableTrait = local.callableTrait;
    result.valid = true;
    applyIdentifierFlowFacts(result, scopeName.semanticOrdinal * 2U);
    result.objectLifetimeOwnership = local.objectLifetimeOwnership;
    result.storageOwnership = local.storageOwnership;
    result.borrowedElementSlots = local.borrowedElementSlots;
    if (!applyLocalIdentifierOwnership(result, expression, scopeName, local, diagnostics)) return {};
    completeLocalIdentifier(
        result, expression, scopeName, local, methodMemberOrdinal, scopeOrdinal);
    return result;
}

bool ProjectMethodBodyModel::validateLocalIdentifierRead(
    const ast::AstNode* expression,
    const ScopeName& scopeName,
    const MethodLocalSymbol& local,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!deferCaptureAllowsLocal(expression->text(), local.scopeOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "defer body uses outer local '" + expression->text() +
                "' without an explicit ref or move capture");
        return false;
    }
    // C7 "definite assignment": this is the one place a local name becomes a READ of
    // its value, so it is the one place the check belongs. It runs before the
    // ownership diagnostics below on purpose -- an unassigned local is the more
    // fundamental complaint, and the moved-from checks below would otherwise report
    // a confusing second opinion about the same name.
    checkDefiniteAssignment(expression, scopeName.semanticOrdinal, diagnostics);
    const std::string partialRoot = "L" + std::to_string(scopeName.semanticOrdinal);
    if (local.isMoveOnly && partialProjectionReceiverDepth_ == 0 &&
        (placeHasMovedDescendant(partialRoot) || placeHasMaybeMovedDescendant(partialRoot)))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "use of partially moved value '" + local.name +
                "'; reinitialize every moved field before using the complete value");
        return false;
    }
    // EN: Freeze DEC-10: a catch variable moved out with move(e) is moved-from too.
    // ES: Congelamiento DEC-10: una variable de catch sacada con move(e) tambien queda movida.
    const bool tracksMoves = local.isRaii || local.isMoveOnly || local.isCatchVariable;
    const std::string movedKind = local.isRaii ? "RAII local '"
        : (local.isCatchVariable ? "caught exception '" : "move-only local '");
    if (tracksMoves && movedRaiiLocals_.contains(scopeName.semanticOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "use of moved-from " + movedKind + local.name + "'");
        return false;
    }
    if (tracksMoves && maybeMovedRaiiLocals_.contains(scopeName.semanticOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "use of possibly moved " + movedKind + local.name +
                "': value is possibly moved after a control-flow join");
        return false;
    }
    return true;
}

bool ProjectMethodBodyModel::applyBorrowedLocalIdentifierOwnership(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ScopeName& scopeName,
    const MethodLocalSymbol& local,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!local.localBorrowBindingOrdinal.has_value() ||
        *local.localBorrowBindingOrdinal >= localBorrowBindings_.size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "borrowed local '" + local.name + "' has no valid lifetime binding");
        return false;
    }
    const MethodLocalBorrowBinding& borrow =
        localBorrowBindings_[*local.localBorrowBindingOrdinal];
    const MethodBorrowState borrowState = localBorrowFlowState(*local.localBorrowBindingOrdinal);
    const bool rootDefinitelyMoved =
        (borrow.sourceOwnerLocalOrdinal.has_value() &&
         movedRaiiLocals_.contains(*borrow.sourceOwnerLocalOrdinal)) ||
        (borrow.sourceOwnerParameterOrdinal.has_value() &&
         movedOwnedParameters_.contains(*borrow.sourceOwnerParameterOrdinal)) ||
        std::ranges::any_of(
            borrow.possibleSourceOwnerLocalOrdinals,
            [&](std::size_t ordinal) { return movedRaiiLocals_.contains(ordinal); }) ||
        std::ranges::any_of(
            borrow.possibleSourceOwnerParameterOrdinals,
            [&](std::size_t ordinal) { return movedOwnedParameters_.contains(ordinal); });
    const bool rootMaybeMoved =
        (borrow.sourceOwnerLocalOrdinal.has_value() &&
         maybeMovedRaiiLocals_.contains(*borrow.sourceOwnerLocalOrdinal)) ||
        (borrow.sourceOwnerParameterOrdinal.has_value() &&
         maybeMovedOwnedParameters_.contains(*borrow.sourceOwnerParameterOrdinal)) ||
        std::ranges::any_of(
            borrow.possibleSourceOwnerLocalOrdinals,
            [&](std::size_t ordinal) { return maybeMovedRaiiLocals_.contains(ordinal); }) ||
        std::ranges::any_of(
            borrow.possibleSourceOwnerParameterOrdinals,
            [&](std::size_t ordinal) { return maybeMovedOwnedParameters_.contains(ordinal); });
    if (borrowState == MethodBorrowState::Invalidated || rootDefinitelyMoved)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "use of invalidated borrowed local '" + local.name + "'");
        return false;
    }
    if (borrowState == MethodBorrowState::MaybeInvalid || rootMaybeMoved)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "use of possibly invalidated borrowed local '" + local.name +
                "' after a control-flow join");
        return false;
    }
    result.ownership = sema::OwnershipKind::Borrowed;
    result.ownershipSourceLocalOrdinal = borrow.sourceOwnerLocalOrdinal;
    result.ownershipSourceParameterOrdinal = borrow.sourceOwnerParameterOrdinal;
    result.borrowedLocalOrdinal = scopeName.semanticOrdinal;
    return true;
}

bool ProjectMethodBodyModel::applyLocalIdentifierOwnership(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ScopeName& scopeName,
    const MethodLocalSymbol& local,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const bool borrowedNativeArray =
        local.ownership == sema::OwnershipKind::Borrowed &&
        ProjectTypeConversion::isNativeArray(local.typeKey);
    if (local.isRaii && !borrowedNativeArray)
    {
        result.ownership = sema::OwnershipKind::Owned;
        result.ownershipSourceLocalOrdinal = scopeName.semanticOrdinal;
        return true;
    }
    if (local.isRaii && borrowedNativeArray)
    {
        // Buffer owner / borrowed-element array. The local itself still has a RAII
        // cleanup region, but indexing it must not turn aliases into owned slots.
        result.ownership = sema::OwnershipKind::Borrowed;
        return true;
    }
    if (local.ownership != sema::OwnershipKind::Borrowed) return true;
    return applyBorrowedLocalIdentifierOwnership(
        result, expression, scopeName, local, diagnostics);
}

void ProjectMethodBodyModel::completeLocalIdentifier(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ScopeName& scopeName,
    const MethodLocalSymbol& local,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // E11 heap-homes an outer local captured by a lambda in a shared cell.  It is an
    // lvalue on both sides of the closure boundary, exactly like the source local;
    // const and borrowed locals keep their existing restrictions.
    // T41: a borrowed local is an alias slot, not immutable storage.  Reassignment
    // is admitted here and its new owner roots are validated/accumulated in
    // analyzeAssignment; `const borrowed` remains immutable.
    result.assignable = !local.isConst;
    // T19-002: `&p` of a pointer to atomic storage would need a nested qualifier, like volatile. / `&p` de un
    //     puntero a almacenamiento atomico necesitaria un calificador anidado, como volatile.
    // C1-02: a pointer to volatile data has an address since levels are qualified (`T volatile**`). / Un puntero a
    //     dato volatile tiene direccion desde que los niveles se califican.
    result.addressable = !local.cppPointerPointeeAtomic;
    result.readonlyReceiver = local.isConst || local.readonlyBorrowedObject;
    result.readonlyBorrowedObject = local.readonlyBorrowedObject;
    result.pointerPointeeReadonly = local.pointerPointeeReadonly;
    result.pointerPointeeVolatile = local.pointerPointeeVolatile;
    result.pointerLevelQualifiers = local.pointerLevelQualifiers;
    result.cppVolatilePointeeContract = result.cppVolatilePointeeContract || local.cppVolatilePointeeContract;
    result.cppPointerPointeeAtomic = local.cppPointerPointeeAtomic;
    result.cppAtomicPointeeHoldsVolatile = local.cppAtomicPointeeHoldsVolatile;
    result.cppVolatilePointeeContract = local.cppVolatilePointeeContract;
    result.cppVolatilePointeeMicrosoft = local.cppVolatilePointeeMicrosoft;
    result.cppVolatilePointeeAlignment = local.cppVolatilePointeeAlignment;
    result.pointerNullable = local.pointerNullable;
    result.pointerHasProvenance = local.pointerHasProvenance;
    result.pointerKnownAlignment = local.pointerKnownAlignment;
    result.pointerAddressSpace = local.pointerAddressSpace;
    result.onePastEndPointer = local.onePastEndPointer;
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::Local,
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        expression->text(),
        result.typeKey,
        scopeName.semanticOrdinal,
        std::nullopt,
        {},
        nullptr,
        "",
        std::nullopt,
        false});
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeParameterIdentifier(
    const ast::AstNode* expression,
    const ScopeName& scopeName,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method == nullptr || scopeName.semanticOrdinal >= method->parameters.size()) return result;
    const MethodParameterSymbol& parameter = method->parameters[scopeName.semanticOrdinal];
    if (!validateParameterIdentifierRead(expression, scopeName, parameter, diagnostics)) return result;
    initializeParameterIdentifierResult(result, scopeName, parameter);
    if (method->isUserOperator && parameter.passKind == ParameterPassKind::In &&
        memberModel_ != nullptr && memberModel_->typeTransitivelyOwnsStorage(result.typeKey))
    {
        result.readonlyBorrowedObject = true;
        result.ownershipSourceParameterOrdinal = scopeName.semanticOrdinal;
    }
    if (parameter.ownership == sema::OwnershipKind::Borrowed &&
        !applyBorrowedParameterReassignment(
            result, expression, scopeName, methodMemberOrdinal, diagnostics))
    {
        return {};
    }
    completeParameterIdentifier(
        result, expression, scopeName, parameter, methodMemberOrdinal, scopeOrdinal);
    return result;
}

bool ProjectMethodBodyModel::validateParameterIdentifierRead(
    const ast::AstNode* expression,
    const ScopeName& scopeName,
    const MethodParameterSymbol& parameter,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!deferCaptureAllowsParameter(expression->text()))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "defer body uses parameter '" + expression->text() +
                "' without an explicit ref or move capture");
        return false;
    }
    const bool moveTrackedParameter =
        parameter.ownership == sema::OwnershipKind::Owned ||
        (parameter.passKind == ParameterPassKind::Value &&
         memberModel_->typeIsMoveOnly(parameter.typeKey));
    const std::string partialRoot = "P" + std::to_string(scopeName.semanticOrdinal);
    if (moveTrackedParameter && partialProjectionReceiverDepth_ == 0 &&
        (placeHasMovedDescendant(partialRoot) || placeHasMaybeMovedDescendant(partialRoot)))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "use of partially moved parameter '" + parameter.name +
                "'; reinitialize every moved field before using the complete value");
        return false;
    }
    if (moveTrackedParameter && movedOwnedParameters_.contains(scopeName.semanticOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "use of moved-from parameter '" + parameter.name + "'");
        return false;
    }
    if (moveTrackedParameter && maybeMovedOwnedParameters_.contains(scopeName.semanticOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "use of moved-from parameter '" + parameter.name +
                "': value is possibly moved after a control-flow join");
        return false;
    }
    return true;
}

void ProjectMethodBodyModel::initializeParameterIdentifierResult(
    ExpressionResult& result,
    const ScopeName& scopeName,
    const MethodParameterSymbol& parameter)
{
    // C4 "language references": a by-reference parameter dereferences implicitly. The
    // symbol's own typeKey is `T*` (that is what the callee physically receives), but the
    // programmer wrote `ref T x` and every use of `x` means the T on the other side of
    // the pointer -- reading it, assigning it, passing it on. That implicit dereference
    // is the entire ergonomic difference between `ref int x` and `int* x`, and it is the
    // reason a ref parameter needs its own rule here instead of being pointer sugar at
    // the call site only.
    const bool byReferenceParameter = parameter.passKind != ParameterPassKind::Value;
    result.typeKey = byReferenceParameter ? parameter.pointeeTypeKey : parameter.typeKey;
    result.valid = true;
    applyIdentifierFlowFacts(result, scopeName.semanticOrdinal * 2U + 1U);
    result.ownership = parameter.ownership;
    result.objectLifetimeOwnership = parameter.ownership;
    result.storageOwnership = parameter.ownership;
    result.pointerPointeeReadonly = !byReferenceParameter && parameter.pointerPointeeReadonly;
    result.pointerPointeeVolatile = !byReferenceParameter && parameter.pointerPointeeVolatile;
    result.pointerLevelQualifiers = byReferenceParameter ? 0U : parameter.pointerLevelQualifiers;
    result.cppPointerPointeeAtomic = !byReferenceParameter && parameter.cppPointerPointeeAtomic;
    if (!byReferenceParameter)
    {
        result.pointerNullable = parameter.pointerNullable;
        result.pointerHasProvenance = parameter.pointerHasProvenance;
        result.pointerKnownAlignment = parameter.pointerKnownAlignment;
        result.pointerAddressSpace = parameter.pointerAddressSpace;
    }
    if (parameter.ownership != sema::OwnershipKind::None)
    {
        result.ownershipSourceParameterOrdinal = scopeName.semanticOrdinal;
    }
}

bool ProjectMethodBodyModel::applyBorrowedParameterReassignment(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ScopeName& scopeName,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    for (const MethodBorrowedParameterReassignmentBinding& reassignment :
         borrowedParameterReassignmentBindings_)
    {
        if (reassignment.methodMemberOrdinal != methodMemberOrdinal ||
            reassignment.parameterOrdinal != scopeName.semanticOrdinal) continue;
        const bool rootDefinitelyMoved = std::ranges::any_of(
            reassignment.possibleSourceOwnerLocalOrdinals,
            [&](std::size_t ordinal) { return movedRaiiLocals_.contains(ordinal); }) ||
            std::ranges::any_of(
                reassignment.possibleSourceOwnerParameterOrdinals,
                [&](std::size_t ordinal) { return movedOwnedParameters_.contains(ordinal); });
        const bool rootMaybeMoved = std::ranges::any_of(
            reassignment.possibleSourceOwnerLocalOrdinals,
            [&](std::size_t ordinal) { return maybeMovedRaiiLocals_.contains(ordinal); }) ||
            std::ranges::any_of(
                reassignment.possibleSourceOwnerParameterOrdinals,
                [&](std::size_t ordinal) { return maybeMovedOwnedParameters_.contains(ordinal); });
        if (rootDefinitelyMoved || rootMaybeMoved)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                rootDefinitelyMoved
                    ? "use of borrowed parameter whose reassigned owner was moved"
                    : "use of borrowed parameter whose reassigned owner may have moved");
            return false;
        }
        if (!reassignment.possibleSourceOwnerLocalOrdinals.empty())
        {
            result.ownershipSourceLocalOrdinal =
                reassignment.possibleSourceOwnerLocalOrdinals.front();
        }
        else if (!reassignment.possibleSourceOwnerParameterOrdinals.empty())
        {
            result.ownershipSourceParameterOrdinal =
                reassignment.possibleSourceOwnerParameterOrdinals.front();
        }
        break;
    }
    return true;
}

void ProjectMethodBodyModel::completeParameterIdentifier(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ScopeName& scopeName,
    const MethodParameterSymbol& parameter,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // `in` is read-only by definition: it exists to avoid a copy, not to give the callee
    // a channel back to the caller. `ref`/`out` are assignable, and for `out` that is not
    // a permission but an obligation -- see the definite-assignment check on returns.
    // E11 captured parameters are shared cells too.  `in` remains read-only; value/ref/
    // out parameters preserve the same assignability inside a closure as in the method.
    result.assignable = parameter.passKind != ParameterPassKind::In;
    result.addressable = !result.cppPointerPointeeAtomic;
    result.readonlyReceiver = parameter.passKind == ParameterPassKind::In;
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::Parameter,
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        expression->text(),
        result.typeKey,
        std::nullopt,
        scopeName.semanticOrdinal,
        {},
        nullptr,
        "",
        std::nullopt,
        false});
}

void ProjectMethodBodyModel::collectIdentifierMemberCandidates(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t accessingTypeOrdinal,
    bool staticContext,
    std::vector<std::size_t>& fields,
    std::vector<std::size_t>& events,
    std::vector<std::size_t>& methods,
    bool& inaccessibleFound,
    bool& uncapturedReceiverFound) const
{
    for (const std::size_t candidate : memberCandidates(methodMemberOrdinal, expression->text()))
    {
        if (candidate >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[candidate];
        if (member.kind == ClassMemberKind::Constructor ||
            member.kind == ClassMemberKind::Destructor) continue;
        if (staticContext && !member.isStatic) continue;
        if (!member.isStatic && !deferCaptureAllowsThis())
        {
            uncapturedReceiverFound = true;
            continue;
        }
        if (!ProjectMemberAccessibility::isAccessible(
                member,
                accessingTypeOrdinal,
                accessingTypeOrdinal,
                *nameLookup_,
                *inheritanceModel_))
        {
            inaccessibleFound = true;
            continue;
        }
        if (member.kind == ClassMemberKind::Field) fields.push_back(candidate);
        else if (member.kind == ClassMemberKind::Event) events.push_back(candidate);
        else if (member.kind == ClassMemberKind::Method) methods.push_back(candidate);
    }
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeImplicitEventIdentifier(
    const ast::AstNode* expression,
    const std::vector<std::size_t>& events,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    std::size_t accessingTypeOrdinal,
    const std::string& thisTypeKey,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowEvent)
{
    ExpressionResult result;
    const std::size_t eventOrdinal = events.front();
    const ClassMemberSymbol& event = memberModel_->members()[eventOrdinal];
    result.typeKey = event.declaredTypeKey;
    result.valid = allowEvent;
    result.assignable = false;
    result.eventMemberOrdinal = eventOrdinal;
    result.readonlyReceiver = !event.isStatic && contextHasReadonlyReceiver(methodMemberOrdinal);
    if (!event.isStatic)
    {
        result.receiverTypeKey = thisTypeKey;
        result.receiverTypeOrdinal = accessingTypeOrdinal;
    }
    recordNameBinding(MethodNameBinding{
        event.isStatic ? MethodNameBindingKind::StaticEvent : MethodNameBindingKind::ImplicitEvent,
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        expression->text(),
        result.typeKey,
        std::nullopt,
        std::nullopt,
        events,
        nullptr,
        event.isStatic ? "" : thisTypeKey,
        event.isStatic ? std::nullopt : std::optional<std::size_t>(accessingTypeOrdinal),
        false});
    if (!allowEvent)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "event '" + expression->text() +
                "' may only be used with +=, -=, or invocation from within its declaring type in Stage 42");
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeImplicitFieldIdentifier(
    const ast::AstNode* expression,
    const std::vector<std::size_t>& fields,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    std::size_t accessingTypeOrdinal,
    const std::string& thisTypeKey)
{
    ExpressionResult result;
    const ClassMemberSymbol& field = memberModel_->members()[fields.front()];
    const bool readonlyThis = !field.isStatic && contextHasReadonlyReceiver(methodMemberOrdinal);
    result.typeKey = field.declaredTypeKey;
    result.valid = true;
    result.assignable = !readonlyThis && !field.isConst &&
        (!field.isReadonly || readonlyFieldAssignable(field, methodMemberOrdinal));
    result.addressable = !field.isBitField && field.cppArrayElementStrides.empty();
    result.readonlyReceiver = readonlyThis || field.isReadonly;
    result.pointerPointeeReadonly = field.declaredPointerPointeeReadonly;
    result.pointerPointeeVolatile = field.declaredPointerPointeeVolatile;
    result.pointerLevelQualifiers = field.declaredPointerLevelQualifiers;
    // C3-04: a C `_Atomic T` field is atomic storage, without ref/in/out. / Un campo C `_Atomic T` es
    //     almacenamiento atomico, sin ref/in/out.
    if (field.cppAtomicStorage) { result.cppAtomicStorage = true; result.addressable = false; }
    result.pointerNullable = field.declaredPointerNullable;
    result.pointerHasProvenance = field.declaredPointerHasProvenance;
    result.pointerKnownAlignment = field.declaredPointerKnownAlignment;
    result.pointerAddressSpace = field.declaredPointerAddressSpace;
    result.assignmentMemberOrdinal = fields.front();
    if (!field.isStatic)
    {
        result.receiverTypeKey = thisTypeKey;
        result.receiverTypeOrdinal = accessingTypeOrdinal;
    }
    recordNameBinding(MethodNameBinding{
        field.isStatic ? MethodNameBindingKind::StaticField : MethodNameBindingKind::ImplicitField,
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        expression->text(),
        result.typeKey,
        std::nullopt,
        std::nullopt,
        fields,
        nullptr,
        field.isStatic ? "" : thisTypeKey,
        field.isStatic ? std::nullopt : std::optional<std::size_t>(accessingTypeOrdinal),
        false});
    return result;
}

void ProjectMethodBodyModel::collectPropertyAccessorCandidates(
    std::string_view accessorName,
    bool getter,
    std::size_t methodMemberOrdinal,
    std::size_t accessingTypeOrdinal,
    bool staticContext,
    std::vector<std::size_t>& output) const
{
    for (const std::size_t ordinal : memberCandidates(methodMemberOrdinal, accessorName))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (!member.isPropertyAccessor || getter != member.isGetter) continue;
        if (staticContext && !member.isStatic) continue;
        if (!staticContext && member.isStatic && !output.empty()) continue;
        if (!ProjectMemberAccessibility::isAccessible(
                member,
                accessingTypeOrdinal,
                accessingTypeOrdinal,
                *nameLookup_,
                *inheritanceModel_)) continue;
        output.push_back(ordinal);
    }
    if (staticContext) return;
    const bool hasInstance = std::any_of(output.begin(), output.end(), [&](std::size_t ordinal)
    {
        return ordinal < memberModel_->members().size() &&
            !memberModel_->members()[ordinal].isStatic;
    });
    if (hasInstance)
    {
        std::erase_if(output, [&](std::size_t ordinal)
        {
            return ordinal < memberModel_->members().size() &&
                memberModel_->members()[ordinal].isStatic;
        });
    }
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzePropertyIdentifier(
    const ast::AstNode* expression,
    const std::vector<std::size_t>& propertyGetters,
    const std::vector<std::size_t>& propertySetters,
    const ClassMemberSymbol& caller,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    std::size_t accessingTypeOrdinal,
    const std::string& thisTypeKey,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    const bool staticProperty = memberModel_->members()[propertyGetters.front()].isStatic;
    const std::optional<std::size_t> backing =
        allowConstructorPropertyWrite && propertySetters.empty() && !staticProperty &&
            caller.kind == ClassMemberKind::Constructor
        ? automaticPropertyBackingField(accessingTypeOrdinal, expression->text(), false)
        : std::nullopt;
    if (backing.has_value())
    {
        const ClassMemberSymbol& field = memberModel_->members()[*backing];
        result.typeKey = field.declaredTypeKey;
        result.valid = true;
        result.assignable = true;
        result.addressable = true;
        result.pointerPointeeReadonly = field.declaredPointerPointeeReadonly;
        result.pointerPointeeVolatile = field.declaredPointerPointeeVolatile;
        result.pointerLevelQualifiers = field.declaredPointerLevelQualifiers;
        // C3-04: a C `_Atomic T` field is atomic storage, without ref/in/out. / Un campo C `_Atomic T` es
        //     almacenamiento atomico, sin ref/in/out.
        if (field.cppAtomicStorage) { result.cppAtomicStorage = true; result.addressable = false; }
        result.pointerNullable = field.declaredPointerNullable;
        result.pointerHasProvenance = field.declaredPointerHasProvenance;
        result.pointerKnownAlignment = field.declaredPointerKnownAlignment;
        result.pointerAddressSpace = field.declaredPointerAddressSpace;
        result.assignmentMemberOrdinal = *backing;
        result.receiverTypeKey = thisTypeKey;
        result.receiverTypeOrdinal = accessingTypeOrdinal;
        recordNameBinding(MethodNameBinding{
            MethodNameBindingKind::ImplicitField,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            expression->text(),
            result.typeKey,
            std::nullopt,
            std::nullopt,
            {*backing},
            nullptr,
            thisTypeKey,
            accessingTypeOrdinal,
            false});
        return result;
    }
    result = resolveSyntheticCall(
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        "get_" + expression->text(),
        propertyGetters,
        {},
        nullptr,
        staticProperty ? "" : thisTypeKey,
        staticProperty ? std::nullopt : std::optional<std::size_t>(accessingTypeOrdinal),
        false,
        false,
        diagnostics,
        true,
        !staticProperty && contextHasReadonlyReceiver(methodMemberOrdinal));
    // NS304-T08-F02: a ref-returning getter is assignable even without a setter.
    // EN: Freeze P3: a `ref readonly` getter is not.
    // ES: Congelamiento P3: un getter `ref readonly` no.
    const bool refGetter = std::any_of(
        propertyGetters.begin(), propertyGetters.end(), [&](std::size_t ordinal)
        {
            return ordinal < memberModel_->members().size() &&
                memberModel_->members()[ordinal].isReferenceReturn &&
                !isReadonlyReferenceReturn(memberModel_->members()[ordinal]);
        });
    result.assignable = !contextHasReadonlyReceiver(methodMemberOrdinal) &&
        (!propertySetters.empty() || refGetter);
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeMethodGroupIdentifier(
    const ast::AstNode* expression,
    const std::vector<std::size_t>& methods,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    std::size_t accessingTypeOrdinal,
    const std::string& thisTypeKey,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
    ExpressionResult result;
    result.methodGroup = true;
    result.methodCandidates = methods;
    result.valid = allowMethodGroup;
    const bool allStatic = std::all_of(
        methods.begin(), methods.end(), [&](std::size_t ordinal)
        {
            return ordinal < memberModel_->members().size() &&
                memberModel_->members()[ordinal].isStatic;
        });
    if (!allStatic)
    {
        result.receiverTypeKey = thisTypeKey;
        result.receiverTypeOrdinal = accessingTypeOrdinal;
        result.readonlyReceiver = contextHasReadonlyReceiver(methodMemberOrdinal);
    }
    recordNameBinding(MethodNameBinding{
        allStatic ? MethodNameBindingKind::StaticMethodGroup : MethodNameBindingKind::ImplicitMethodGroup,
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        expression->text(),
        "<method-group>",
        std::nullopt,
        std::nullopt,
        methods,
        nullptr,
        allStatic ? "" : thisTypeKey,
        allStatic ? std::nullopt : std::optional<std::size_t>(accessingTypeOrdinal),
        false});
    if (!allowMethodGroup)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "method group '" + expression->text() + "' can only be used as a call target");
    }
    return result;
}

std::optional<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::analyzeNamespaceIdentifierFallback(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
    // Members deliberately win. If no member claimed the identifier, a namespace function
    // overload set is a first-class method group and can be converted contextually to a delegate.
    if (namespaceFunctionModel_ != nullptr)
    {
        if (const NamespaceFunctionReferenceBinding* functionReference =
                namespaceFunctionModel_->referenceBinding(expression);
            functionReference != nullptr && !functionReference->qualified)
        {
            return analyzeNamespaceFunctionGroup(
                expression, *functionReference, methodMemberOrdinal, scopeOrdinal,
                diagnostics, allowMethodGroup);
        }
    }
    // Class members deliberately win over namespace state, but an unqualified namespace
    // constant/variable remains visible from a method after locals, parameters, fields,
    // properties, events and methods have all had their normal precedence.
    if (namespaceFunctionModel_ != nullptr)
    {
        if (const NamespaceStateReferenceBinding* stateReference =
                namespaceFunctionModel_->stateReferenceBinding(expression);
            stateReference != nullptr && !stateReference->qualified)
        {
            return analyzeNamespaceState(
                expression, *stateReference, methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
    }
    return std::nullopt;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeMemberOrNamespaceIdentifier(
    const ast::AstNode* expression,
    const ClassMemberSymbol& caller,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    const std::size_t accessingTypeOrdinal = caller.declaringTypeOrdinal;
    const std::string thisTypeKey = implicitThisTypeKey(methodMemberOrdinal);
    const bool staticContext = !contextHasImplicitThis(methodMemberOrdinal);
    std::vector<std::size_t> fields;
    std::vector<std::size_t> events;
    std::vector<std::size_t> methods;
    bool inaccessibleFound = false;
    bool uncapturedReceiverFound = false;
    collectIdentifierMemberCandidates(
        expression, methodMemberOrdinal, accessingTypeOrdinal, staticContext,
        fields, events, methods, inaccessibleFound, uncapturedReceiverFound);
    if (fields.empty() && events.empty() && methods.empty() && uncapturedReceiverFound)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "defer body uses an instance member through this without an explicit 'ref this' capture");
        return result;
    }
    if (!events.empty())
    {
        return analyzeImplicitEventIdentifier(
            expression, events, methodMemberOrdinal, scopeOrdinal, accessingTypeOrdinal,
            thisTypeKey, diagnostics, allowEvent);
    }
    if (!fields.empty())
    {
        return analyzeImplicitFieldIdentifier(
            expression, fields, methodMemberOrdinal, scopeOrdinal, accessingTypeOrdinal, thisTypeKey);
    }
    std::vector<std::size_t> propertyGetters;
    std::vector<std::size_t> propertySetters;
    collectPropertyAccessorCandidates(
        "get_" + expression->text(), true, methodMemberOrdinal,
        accessingTypeOrdinal, staticContext, propertyGetters);
    collectPropertyAccessorCandidates(
        "set_" + expression->text(), false, methodMemberOrdinal,
        accessingTypeOrdinal, staticContext, propertySetters);
    if (!propertyGetters.empty())
    {
        return analyzePropertyIdentifier(
            expression, propertyGetters, propertySetters, caller, methodMemberOrdinal,
            scopeOrdinal, accessingTypeOrdinal, thisTypeKey, diagnostics,
            allowConstructorPropertyWrite);
    }
    if (!methods.empty())
    {
        ExpressionResult group = analyzeMethodGroupIdentifier(
            expression, methods, methodMemberOrdinal, scopeOrdinal,
            accessingTypeOrdinal, thisTypeKey, diagnostics, allowMethodGroup);
        group.inaccessibleNamedMember = inaccessibleFound;
        return group;
    }
    if (const std::optional<ExpressionResult> namespaceResult =
            analyzeNamespaceIdentifierFallback(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup);
        namespaceResult.has_value())
    {
        return *namespaceResult;
    }
    diagnostics.report(
        inaccessibleFound ? diagnostics::DiagnosticId::InaccessibleMember
                          : diagnostics::DiagnosticId::UnknownIdentifier,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        inaccessibleFound
            ? "member '" + expression->text() + "' is not accessible in this context"
            : "unknown identifier '" + expression->text() + "' in callable/initializer context");
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeIdentifier(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    const std::optional<ScopeName> scopeName = lookupScopeName(scopeOrdinal, expression->text());
    if (scopeName.has_value())
    {
        if (scopeName->kind == ScopeNameKind::Local)
        {
            return analyzeLocalIdentifier(
                expression, *scopeName, methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        return analyzeParameterIdentifier(
            expression, *scopeName, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    const bool namespaceCallable = methodMemberOrdinal >= namespaceCallableBase_ &&
        methodMemberOrdinal - namespaceCallableBase_ < namespaceCallableAdapters().size();
    if (namespaceCallable && namespaceFunctionModel_ != nullptr)
    {
        if (const NamespaceStateReferenceBinding* stateReference =
                namespaceFunctionModel_->stateReferenceBinding(expression);
            stateReference != nullptr && !stateReference->qualified)
        {
            return analyzeNamespaceState(
                expression, *stateReference, methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
    }
    const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
    if (caller == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr ||
        inheritanceModel_ == nullptr) return result;
    return analyzeMemberOrNamespaceIdentifier(
        expression, *caller, methodMemberOrdinal, scopeOrdinal, diagnostics,
        allowMethodGroup, allowEvent, allowConstructorPropertyWrite);
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeNamespaceState(
    const ast::AstNode* expression,
    const NamespaceStateReferenceBinding& reference,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (namespaceFunctionModel_ == nullptr || reference.candidateStateOrdinals.empty())
        return result;
    std::vector<std::size_t> candidates = reference.candidateStateOrdinals;
    const auto& states = namespaceFunctionModel_->states();
    std::erase_if(candidates, [&](const auto ordinal) {
        if (ordinal >= states.size() || !states[ordinal].cppReconciledStateKey) return false;
        return std::any_of(reference.candidateStateOrdinals.begin(), reference.candidateStateOrdinals.end(),
            [&](const auto other) { return other < states.size() && other != ordinal &&
                states[other].identityKey == *states[ordinal].cppReconciledStateKey; });
    });
    // Imported TLS declarations with the same C name can belong to different
    // DLLs. A transported template resolves names in its producer's scope.
    if (candidates.size() > 1 &&
        std::all_of(candidates.begin(), candidates.end(), [&](const auto ordinal) {
            return ordinal < states.size() && states[ordinal].cppCGlobal &&
                states[ordinal].cppThreadLocal && states[ordinal].cppDllImport &&
                !states[ordinal].cppTlsRuntimeIdentity.empty();
        }))
    {
        const bool hasLocal = std::any_of(candidates.begin(), candidates.end(), [&](const auto ordinal) {
            return states[ordinal].projectOrdinal == reference.projectOrdinal;
        });
        if (hasLocal)
            std::erase_if(candidates, [&](const auto ordinal) {
                return states[ordinal].projectOrdinal != reference.projectOrdinal;
            });
    }
    // Each transported template retains the C enum constants of its producer.
    // A consumer without such an owner must still diagnose ambiguous constants, and so must the consumer's
    // own code even when its headers declare one of them: only a producer's catalogue identifies the owner.
    // Cada plantilla transportada conserva las constantes enum C de su productor. El código propio del
    // consumidor sigue diagnosticando la ambigüedad aunque sus encabezados declaren una de ellas.
    const auto producerOwned = [&](std::size_t ordinal) {
        return states[ordinal].projectOrdinal == reference.projectOrdinal;
    };
    if (candidates.size() > 1 &&
        std::all_of(candidates.begin(), candidates.end(), [&](const auto ordinal) {
            return ordinal < states.size() && states[ordinal].isCppEnumConstant;
        }) &&
        std::any_of(candidates.begin(), candidates.end(), producerOwned) &&
        std::all_of(candidates.begin(), candidates.end(), [&](const auto ordinal) {
            return !producerOwned(ordinal) || states[ordinal].cppFromPrecompiledCatalog;
        }))
        std::erase_if(candidates, [&](const auto ordinal) {
            return states[ordinal].projectOrdinal != reference.projectOrdinal;
        });
    if (candidates.size() > 1 && candidates.front() < states.size())
    {
        // Same C object except, possibly, its bound: `extern T a[];` (T[*]) and `extern T a[N];` (T[N]).
        // Mismo objeto C salvo, quizá, su límite: `extern T a[];` (T[*]) y `extern T a[N];` (T[N]).
        const auto sameElementAndBound = [](const NamespaceStateSymbol& left, const NamespaceStateSymbol& right,
                                            bool allowUnknownBound) {
            if (left.declaredTypeKey == right.declaredTypeKey)
                return !left.cppCanonicalCType.empty() && left.cppCanonicalCType == right.cppCanonicalCType &&
                    left.cppStorageSizeBits == right.cppStorageSizeBits;
            if (!allowUnknownBound) return false;
            const auto unknown = [](const NamespaceStateSymbol& state) {
                return ProjectMemberModel::flexibleArrayElementTypeKey(state.declaredTypeKey);
            };
            const auto known = [](const NamespaceStateSymbol& state) {
                return ProjectMemberModel::fixedArrayElementTypeKey(state.declaredTypeKey);
            };
            const auto leftUnknown = unknown(left);
            const auto rightUnknown = unknown(right);
            return (leftUnknown && !rightUnknown && known(right) == leftUnknown) ||
                (rightUnknown && !leftUnknown && known(left) == rightUnknown);
        };
        // Clang reports no declaration alignment for `extern T a[];`; an explicit one must still match.
        // Clang no informa alineación de declaración para `extern T a[];`; una explícita debe coincidir.
        const auto unboundedWithoutAlignment = [](const NamespaceStateSymbol& left, const NamespaceStateSymbol& right) {
            const auto unbounded = [](const NamespaceStateSymbol& state) {
                return ProjectMemberModel::flexibleArrayElementTypeKey(state.declaredTypeKey).has_value() &&
                    !state.cppDeclarationAlignmentBits;
            };
            return unbounded(left) != unbounded(right);
        };
        const auto compatibleExternalC = [&](std::size_t ordinal, bool allowUnknownBound,
                                             bool anyVolatileMode = false) {
            if (ordinal >= states.size()) return false;
            const auto& other = states[ordinal];
            const auto& first = states[candidates.front()];
            return first.cppCGlobal && other.cppCGlobal && first.isCppExternal && other.isCppExternal &&
                !first.cppCInternalLinkage &&
                !other.cppCInternalLinkage && first.cppImportError.empty() && other.cppImportError.empty() &&
                first.cppClangUsr && !first.cppClangUsr->empty() && first.cppClangUsr == other.cppClangUsr &&
                !first.storageName.empty() && first.storageName == other.storageName &&
                sameElementAndBound(first, other, allowUnknownBound) && first.isVolatile == other.isVolatile &&
                first.isCppReadonly == other.isCppReadonly &&
                first.cppPointerPointeeReadonly == other.cppPointerPointeeReadonly &&
                first.cppPointerPointeeVolatile == other.cppPointerPointeeVolatile &&
                (anyVolatileMode || first.cppMicrosoftVolatileSemantics == other.cppMicrosoftVolatileSemantics) &&
                (first.cppDeclarationAlignmentBits == other.cppDeclarationAlignmentBits ||
                 (allowUnknownBound && unboundedWithoutAlignment(first, other))) &&
                first.cppDllImport == other.cppDllImport && first.cppThreadLocal == other.cppThreadLocal &&
                (!(first.cppDllImport && first.cppThreadLocal) ||
                    (!first.cppTlsRuntimeIdentity.empty() &&
                     first.cppTlsRuntimeIdentity == other.cppTlsRuntimeIdentity)) &&
                first.cppAtomicStorage == other.cppAtomicStorage;
        };
        // EN: T08-D13. The same C object declared by the referencing project and by a referenced one with another
        //     volatile mode (VolatileMode of each CppLibraryReference): the code of a project reads it through its
        //     own header reference, whose mode governs the access and its aliases. Without a declaration of its
        //     own, the candidates stay ambiguous.
        // ES: T08-D13. El mismo objeto C declarado por el proyecto que lo usa y por uno referenciado con otro modo
        //     volatile (VolatileMode de cada CppLibraryReference): el codigo de un proyecto lo lee por su propia
        //     referencia del encabezado, cuyo modo rige el acceso y sus alias. Sin una declaracion propia, los
        //     candidatos siguen siendo ambiguos.
        const auto ownProject = [&](std::size_t ordinal) {
            return states[ordinal].projectOrdinal == reference.projectOrdinal;
        };
        if (std::any_of(candidates.begin(), candidates.end(), ownProject) &&
            !std::all_of(candidates.begin(), candidates.end(), ownProject) &&
            std::all_of(candidates.begin(), candidates.end(),
                        [&](std::size_t ordinal) { return compatibleExternalC(ordinal, true, true); }))
            std::erase_if(candidates, [&](std::size_t ordinal) { return !ownProject(ordinal); });
        if (std::all_of(candidates.begin(), candidates.end(),
                        [&](std::size_t ordinal) { return compatibleExternalC(ordinal, false); }))
            candidates.resize(1);
        else if (std::all_of(candidates.begin(), candidates.end(),
                             [&](std::size_t ordinal) { return compatibleExternalC(ordinal, true); }))
        {
            // C forms the composite type: the declaration with a bound completes the one without it. All bounded
            // declarations must agree with each other; they keep a single storage declaration in the consumer.
            // C forma el tipo compuesto: la declaración con límite completa la que no lo tiene. Todas las que
            // tienen límite deben coincidir entre sí; el consumidor conserva una sola declaración de almacenamiento.
            std::optional<std::size_t> bounded;
            bool consistent = true;
            for (const auto ordinal : candidates)
            {
                if (ProjectMemberModel::flexibleArrayElementTypeKey(states[ordinal].declaredTypeKey)) continue;
                if (!bounded) bounded = ordinal;
                else if (!sameElementAndBound(states[*bounded], states[ordinal], false)) consistent = false;
            }
            if (consistent && bounded) candidates.assign(1, *bounded);
        }
    }
    if (candidates.size() != 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidMemberAccess,
            diagnostics::DiagnosticSeverity::Error,
            expression != nullptr ? expression->range() : SourceRange{},
            "ambiguous namespace state reference '" + reference.writtenName + "'");
        return result;
    }

    const std::size_t stateOrdinal = candidates.front();
    if (stateOrdinal >= namespaceFunctionModel_->states().size()) return result;
    const NamespaceStateSymbol& state = namespaceFunctionModel_->states()[stateOrdinal];
    if (!state.cppImportError.empty())
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidMemberAccess,
            diagnostics::DiagnosticSeverity::Error,
            expression != nullptr ? expression->range() : SourceRange{},
            "cannot import C/C++ global '" + reference.writtenName + "': " + state.cppImportError);
        return result;
    }
    const bool pointeeVolatile = state.cppPointerPointeeVolatile ||
        (state.declaredTypeSyntax != nullptr && state.declaredTypeSyntax->isPointeeVolatile());
    // EN: T08-D06. As in C, `&` of a `const` C global yields a pointer to constant data (`T const*`): the
    //     readonly receiver marks the pointee, so writes through it and conversions that drop `const` are
    //     rejected. `volatile`, atomic and pointer-to-qualified globals still need a qualified address type.
    // ES: T08-D06. Como en C, `&` de una global C `const` produce un puntero a dato constante (`T const*`): el
    //     receptor de solo lectura marca el dato apuntado, asi que se rechazan la escritura y las conversiones
    //     que pierden `const`. Las globales `volatile`, atomicas y punteros a dato calificado siguen sin admitirlo.
    // T19-038: `&gp` of a C `_Atomic T*` global would be a nested qualified pointer. / `&gp` de una global C
    //     `_Atomic T*` seria un puntero calificado anidado.
    // C1-02: a non-atomic pointer global has an address whose levels keep every qualifier. / Una global puntero
    //     no atomica tiene direccion cuyos niveles conservan cada calificador.
    const bool levelAddressablePointer = ProjectTypeConversion::isPointer(state.declaredTypeKey) &&
        !state.cppAtomicStorage && !state.cppPointerPointeeAtomic;
    const bool qualifiedAddress = !levelAddressablePointer && ((state.isCppExternal &&
        (state.isVolatile || state.cppPointerPointeeReadonly || state.cppAtomicStorage ||
         state.cppPointerPointeeAtomic)) || pointeeVolatile);
    // EN: T19-002. As in C, `&a` of an `_Atomic` C global (scalar or record, `volatile` or `const` too, also
    //     `_Thread_local`) is a pointer to atomic storage: every access through it stays atomic. The storage itself
    //     stays non-addressable, so `ref`, `in` and `out` cannot reach it without the atomic access. An atomic
    //     global whose own value is a pointer to qualified data still needs a nested qualified pointer.
    // ES: T19-002. Como en C, `&a` de una global C `_Atomic` (escalar o record, tambien `volatile` o `const`, y
    //     `_Thread_local`) es un puntero a almacenamiento atomico: todo acceso por el sigue siendo atomico. El
    //     almacenamiento sigue sin ser direccionable, asi que `ref`, `in` y `out` no lo alcanzan sin el acceso
    //     atomico. Una global atomica cuyo valor es un puntero a dato calificado sigue necesitando un puntero
    //     calificado anidado.
    // EN: T19-038. An atomic global whose value is a pointer to `const` or `volatile` data is allowed too: the
    //     address is a pointer to atomic storage holding that qualified pointer (T08-D17 for `const`,
    //     `cppAtomicPointeeHoldsVolatile` for `volatile`).
    // ES: T19-038. Tambien una global atomica cuyo valor es un puntero a dato `const` o `volatile`: la direccion es
    //     un puntero a almacenamiento atomico que guarda ese puntero calificado (T08-D17 para `const`,
    //     `cppAtomicPointeeHoldsVolatile` para `volatile`).
    const bool atomicAddress = state.isCppExternal && state.cppAtomicStorage;
    // EN: T19-038. As in C, `&v` of a non-atomic `volatile` C global (scalar or record, `const` too) is a pointer to
    //     volatile data. An array global, or one whose value is a pointer to qualified data, is not covered here.
    // ES: T19-038. Como en C, `&v` de una global C `volatile` no atomica (escalar o record, tambien `const`) es un
    //     puntero a dato volatile. Una global arreglo, o una cuyo valor es un puntero a dato calificado, no entra.
    const bool volatileAddress = !levelAddressablePointer && state.isCppExternal && state.isVolatile &&
        !state.cppAtomicStorage &&
        !state.cppPointerPointeeReadonly && !pointeeVolatile &&
        !ProjectMemberModel::flexibleArrayElementTypeKey(state.declaredTypeKey).has_value() &&
        !ProjectMemberModel::fixedArrayElementTypeKey(state.declaredTypeKey).has_value();
    // EN: T19-041. As in C, the value of a macro whose expansion is not an lvalue has no address.
    // ES: T19-041. Como en C, el valor de una macro cuya expansion no es un lvalue no tiene direccion.
    if (state.cppMacroValue && formingAddressOperand_ && expression == formingAddressTarget_)
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression != nullptr ? expression->range() : SourceRange{},
            "cannot take the address of C macro '" + reference.writtenName +
                "': its expansion is a value, not an lvalue");
        return result;
    }
    if (qualifiedAddress && !atomicAddress && !volatileAddress && formingAddressOperand_ &&
        expression == formingAddressTarget_)
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression != nullptr ? expression->range() : SourceRange{},
            state.cppAtomicStorage
                ? "cannot take the address of C atomic storage holding a pointer to qualified data until nested "
                  "qualified pointers are supported"
                : state.isCppExternal
                ? "cannot take the address of a qualified C/C++ global until qualified pointers are supported"
                : "cannot take the address of this global until nested pointer qualifiers are supported");
        return result;
    }
    const bool unknownBoundArray = state.isCppExternal &&
        ProjectMemberModel::flexibleArrayElementTypeKey(state.declaredTypeKey).has_value();
    if (unknownBoundArray && formingAddressOperand_ && expression == formingAddressTarget_)
    {
        // C gives `&name` the type `T (*)[]`; NativeSharp has no pointer to an array of unknown size yet.
        // C da a `&name` el tipo `T (*)[]`; NativeSharp aún no tiene puntero a arreglo de tamaño desconocido.
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression != nullptr ? expression->range() : SourceRange{},
            "cannot take the address of C array '" + reference.writtenName +
                "' of unknown size; use the array itself, which decays to a pointer to its first element");
        return result;
    }
    result.typeKey = state.declaredTypeKey;
    result.valid = true;
    // EN: T08-D11/D12. A C/C++ global or enumerator is a C operand for the integer divergence diagnostic.
    // ES: T08-D11/D12. Una global o enumerador C/C++ es operando C para el diagnostico de divergencia entera.
    result.cppCOrigin = state.isCppExternal;
    result.assignable = !state.isConst && !state.isCppReadonly &&
        !(state.isCppExternal && ProjectMemberModel::fixedArrayElementTypeKey(state.declaredTypeKey).has_value());
    result.addressable = !state.isCppEnumConstant && !qualifiedAddress;
    result.cppAtomicStorage = atomicAddress;
    result.cppVolatileGlobalAddress = volatileAddress;
    // T19-038: the address keeps the global's volatile mode and C's natural alignment. / La direccion conserva el
    //     modo volatile de la global y la alineacion natural de C.
    if (volatileAddress && state.cppCGlobal)
    {
        result.cppVolatilePointeeMicrosoft = state.cppMicrosoftVolatileSemantics;
        result.cppVolatilePointeeAlignment =
            method_body_detail::cVolatilePointeeScalarWidth(state.declaredTypeKey + "*");
    }
    result.cppAtomicRecordStorage = state.isCppExternal && state.cppAtomicStorage &&
        state.declaredTypeKey.starts_with("type:") && !state.declaredTypeKey.ends_with('*');
    result.readonlyReceiver = state.isConst || state.isCppReadonly;
    // EN: An integer constant (a `const` namespace state, a C macro such as `GL_TRUE`) is a constant expression, as an
    //     integer literal is: it converts implicitly to a narrower integer type when its value fits (C# constant
    //     expression conversion; C++ converts it too), so `Take(ONE)` and `Take(GL_TRUE)` reach a `byte` parameter
    //     like `Take(1)`. The stored value is the bit pattern of the declared type; the constant carries a magnitude.
    // ES: Una constante entera (estado `const` de namespace, macro C como `GL_TRUE`) es una expresion constante, como un
    //     literal entero: se convierte implicitamente a un entero mas estrecho cuando su valor cabe, asi `Take(ONE)` y
    //     `Take(GL_TRUE)` llegan a un parametro `byte` como `Take(1)`. Se guarda el patron de bits del tipo declarado;
    //     la constante lleva una magnitud.
    if ((state.isConst || state.cppMacroValue) && !result.assignable &&
        state.initializerKind == NamespaceStateInitializerKind::Integer &&
        ProjectTypeConversion::isIntegral(state.declaredTypeKey) && state.declaredTypeKey != "builtin:bool")
    {
        struct Width { std::string_view key; unsigned bits; bool isSigned; };
        static constexpr Width widths[] = {
            {"builtin:sbyte", 8, true}, {"builtin:byte", 8, false}, {"builtin:char", 8, false},
            {"builtin:short", 16, true}, {"builtin:ushort", 16, false}, {"builtin:int", 32, true},
            {"builtin:uint", 32, false}, {"builtin:long", 64, true}, {"builtin:ulong", 64, false},
            {"builtin:nint", 64, true}, {"builtin:nuint", 64, false}};
        for (const Width& width : widths)
        {
            if (width.key != state.declaredTypeKey) continue;
            const std::uint64_t mask = width.bits == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << width.bits) - 1U;
            const std::uint64_t bits = state.integerValue & mask;
            const bool negative = width.isSigned && ((bits >> (width.bits - 1U)) & 1U) != 0;
            ProjectIntegerConstant constant;
            constant.isUnsigned = !width.isSigned;
            constant.isNegative = negative;
            constant.value = negative ? ((~bits + 1U) & mask) : bits;
            result.integerConstant = constant;
            break;
        }
    }
    result.volatileStorage = state.isCppExternal && state.isVolatile;
    result.pointerPointeeReadonly = state.cppPointerPointeeReadonly;
    result.pointerPointeeVolatile = pointeeVolatile;
    result.pointerLevelQualifiers = state.isCppExternal ? state.cppPointerLevelQualifiers
        : state.declaredTypeSyntax != nullptr ? state.declaredTypeSyntax->pointerLevelQualifiers() : 0U;
    // T19-038: a C `_Atomic T*` global points to atomic data. / Una global C `_Atomic T*` apunta a dato atomico.
    result.cppPointerPointeeAtomic = state.isCppExternal && state.cppPointerPointeeAtomic;
    // EN: T08-D13. A C global pointer to volatile scalars (or array of them) starts the C contract: aliases keep
    //     the global's volatile mode and C's natural alignment of the pointee.
    // ES: T08-D13. Una global C puntero a escalares volatile (o arreglo de ellos) inicia el contrato C: los alias
    //     conservan el modo volatile de la global y la alineacion natural del apuntado que supone C.
    if (state.isCppExternal && state.cppCGlobal && state.cppPointerPointeeVolatile)
        if (const std::size_t width = method_body_detail::cVolatilePointeeScalarWidth(state.declaredTypeKey))
        {
            result.cppVolatilePointeeContract = true;
            result.cppVolatilePointeeMicrosoft = state.cppMicrosoftVolatileSemantics;
            result.cppVolatilePointeeAlignment = width;
        }
    // `extern T name[];` is projected as T[*]: like a C flexible array member it cannot be assigned or copied
    // as a whole, and decays to T* carrying the element's const/volatile qualifiers.
    // `extern T name[];` se proyecta como T[*]: igual que un miembro flexible C no se asigna ni copia entero y
    // decae a T* con los calificadores const/volatile de su elemento.
    if (unknownBoundArray)
    {
        result.assignable = false;
        result.cppFlexibleArrayMember = true;
        result.cppFlexibleArrayDecaySafe = !state.cppPointerPointeeReadonly && !pointeeVolatile;
        result.pointerPointeeReadonly = result.readonlyReceiver;
        result.pointerPointeeVolatile = result.volatileStorage;
        // EN: T08-D13. Its decay keeps the global's volatile mode; a C global is naturally aligned.
        // ES: T08-D13. Su decaimiento conserva el modo volatile de la global; una global C esta alineada naturalmente.
        result.cppVolatilePointeeMicrosoft = state.cppMicrosoftVolatileSemantics;
        if (const auto element = ProjectMemberModel::flexibleArrayElementTypeKey(state.declaredTypeKey))
            result.cppVolatilePointeeAlignment = method_body_detail::cVolatilePointeeScalarWidth(*element + "*");
    }
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::NamespaceState,
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        reference.writtenName,
        result.typeKey,
        std::nullopt,
        std::nullopt,
        {stateOrdinal},
        nullptr,
        "",
        std::nullopt,
        false});
    return result;
}

std::optional<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::analyzeUnaryOperand(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
// C7 "definite assignment": `f(out x)` is the one argument form that ASSIGNS its operand
// instead of reading it -- the callee is obliged to write it (verifyOutParameterAssignments
// enforces that half) -- so `int x; f(out x); use(x);` must be accepted, and the `out x`
// itself must not be reported as a read. `ref x` and `in x` are ordinary reads and get no
// exemption: passing an unassigned local by reference is the same bug as reading it.
const ast::AstNode* const previousWriteTarget = definiteAssignmentWriteTarget_;
const bool assignsOperand = expression->tokenKind() == TokenKind::Amp &&
    expression->auxiliaryText() == "pass:out";
std::optional<std::size_t> rawUnassignedAddressLocal;
if (assignsOperand)
{
    markAssignmentTarget(expression->first(), scopeOrdinal);
}
else if (expression->tokenKind() == TokenKind::Amp &&
         !expression->auxiliaryText().starts_with("pass:") &&
         expression->first() != nullptr &&
         expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
{
    const std::optional<ScopeName> target =
        lookupScopeName(scopeOrdinal, expression->first()->text());
    if (target.has_value() && target->kind == ScopeNameKind::Local &&
        definiteAssignmentTracksLocal(target->semanticOrdinal) &&
        !assignedLocals_.contains(target->semanticOrdinal))
    {
        rawUnassignedAddressLocal = target->semanticOrdinal;
        // Forming an address is not a value read. Suppress the generic read diagnostic so the
        // source receives the actionable raw-pointer contract below; unlike `out`, this does
        // not mark the local assigned.
        definiteAssignmentWriteTarget_ = expression->first();
    }
}
const bool previousAddressFormation = formingAddressOperand_;
const ast::AstNode* const previousAddressTarget = formingAddressTarget_;
if (expression->tokenKind() == TokenKind::Amp)
{
    formingAddressOperand_ = true;
    formingAddressTarget_ = expression->first();
}
ExpressionResult operand = analyzeExpression(
    expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
    allowMethodGroup && expression->tokenKind() == TokenKind::Amp);
formingAddressOperand_ = previousAddressFormation;
formingAddressTarget_ = previousAddressTarget;
definiteAssignmentWriteTarget_ = previousWriteTarget;
if (rawUnassignedAddressLocal.has_value())
{
    const MethodLocalSymbol& local = locals_[*rawUnassignedAddressLocal];
    const std::string writtenType = local.typeKey.starts_with("builtin:")
        ? local.typeKey.substr(std::string("builtin:").size()) : local.typeKey;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "taking the address of unassigned local '" + local.name +
            "' through a raw pointer does not establish definite assignment; "
            "declare the callee parameter as 'out " + writtenType +
            "' or initialize the local explicitly");
    // The program is rejected. Avoid a second, less useful error at every later read, matching
    // checkDefiniteAssignment's existing diagnostic-recovery policy.
    assignedLocals_.insert(*rawUnassignedAddressLocal);
}
if (!operand.valid)
{
    return std::nullopt;
}

// Passing a delegate by mutable reference can replace its captured owner.
if (expression->tokenKind() == TokenKind::Amp &&
    (expression->auxiliaryText() == "pass:ref" || expression->auxiliaryText() == "pass:out") &&
    expression->first() != nullptr &&
    expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
{
    const auto local = lookupScopeName(scopeOrdinal, expression->first()->text());
    if (local.has_value() && local->kind == ScopeNameKind::Local && local->semanticOrdinal < locals_.size() &&
        isDelegateTypeKey(locals_[local->semanticOrdinal].typeKey) &&
        hasActiveBorrowOfOwnerLocal(local->semanticOrdinal, scopeOrdinal))
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error, expression->range(),
            "cannot pass delegate for replacement while a borrowed local is alive");
        return std::nullopt;
    }
}

// EL MENOS UNARIO SOBRE UN LITERAL SE TIPA JUNTO CON EL, y es la unica forma de escribir el
// minimo de un ancho: `int.MinValue` vale 2147483648 en magnitud, uno mas que el maximo de
// `int`, asi que tipar el literal por su cuenta lo hace `long` y arrastra a toda la expresion.
// Antes habia que escribir `-2147483647 - 1`; ese rodeo estaba en la fixture c30 y en
// NumericLimits.ns.
//
// Es el caso especial de C#, y con su mismo limite: se aplica al literal que esta
// DIRECTAMENTE debajo del menos. `-(2147483648)` con parentesis no es este caso y sigue
// siendo `long`.
//
// Se re-tipa despues de analizar en vez de pasar la bandera hacia abajo porque el literal es
// una hoja: analizarlo no tiene efectos que rehacer, y una bandera de contexto tendria que
// cruzar todo analyzeExpression para servir a un solo nodo.
if (expression->kind() == ast::AstNodeKind::UnaryExpression &&
    expression->tokenKind() == TokenKind::Minus && expression->first() != nullptr &&
    expression->first()->kind() == ast::AstNodeKind::IntegerLiteralExpression)
{
    operand.typeKey = integerLiteralType(expression->first(), true);
    // Y HAY QUE RE-REGISTRARLO, no solo corregir la copia local. El backend no lee este
    // `ExpressionResult`: lee `expressionTypes_`, donde el literal quedo anotado como `long`
    // cuando se analizo por su cuenta. Sin esta linea el modelo semantico dice `int` y el HIR
    // dice `long`, y lo que sale es un `store i64` sobre un `alloca i32` -- ocho bytes en una
    // ranura de cuatro. Se vio en el IR, no en un rojo.
    //
    // El mapa del HIR se construye con `expressionTypeBySyntax_[syntax] = &binding`, asi que
    // la ANOTACION MAS RECIENTE gana. Por eso basta con volver a anotar.
    recordExpressionType(expression->first(), methodMemberOrdinal, scopeOrdinal, operand);
}
    return operand;
}

std::optional<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::analyzeOverloadedUnary(
    const ast::AstNode* expression,
    const ExpressionResult& operand,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const TokenKind operation = expression->tokenKind();
    if (operation == TokenKind::PlusPlus || operation == TokenKind::MinusMinus)
    {
        const std::string name = stage38OperatorMethodName(operation, 1);
        const std::string trait = stage38OperatorTraitKey(operation, 1);
        std::vector<std::size_t> candidates =
            stage38OperatorCandidates(trait, name, operand.typeKey, std::nullopt);
        if (candidates.empty()) return std::nullopt;
        if (!operand.assignable)
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidAssignmentTarget,
                diagnostics::DiagnosticSeverity::Error, expression->range(),
                "increment/decrement requires a modifiable operand");
            return ExpressionResult{};
        }
        if (memberModel_ != nullptr && operand.assignmentMemberOrdinal.has_value() &&
            *operand.assignmentMemberOrdinal < memberModel_->members().size())
        {
            const ClassMemberSymbol& field = memberModel_->members()[*operand.assignmentMemberOrdinal];
            bool activeArgumentField = false;
            const ast::AstNode* target = expression->first();
            if (target != nullptr && target->kind() == ast::AstNodeKind::MemberExpression &&
                target->first() != nullptr &&
                target->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const auto root = lookupScopeName(scopeOrdinal, target->first()->text());
                if (root.has_value() && root->kind == ScopeNameKind::Local)
                {
                    const auto key = std::pair<std::size_t, std::size_t>{
                        root->semanticOrdinal, *operand.assignmentMemberOrdinal};
                    activeArgumentField = std::ranges::find(activeArgumentOwnedFields_, key) !=
                        activeArgumentOwnedFields_.end();
                }
            }
            if (field.kind == ClassMemberKind::Field && !field.isStatic &&
                field.declaredOwnership == sema::OwnershipKind::Owned &&
                isReferenceTypeKey(operand.typeKey) &&
                (activeArgumentField ||
                    hasActiveViewOfStorage(expression->first(), methodMemberOrdinal, scopeOrdinal)))
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, expression->first()->range(),
                    "cannot overwrite owner field while a borrowed view is alive");
                return ExpressionResult{};
            }
        }
        if (isReferenceTypeKey(operand.typeKey) && expression->first() != nullptr &&
            expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const auto target = lookupScopeName(scopeOrdinal, expression->first()->text());
            if (target.has_value() && target->kind == ScopeNameKind::Local &&
                target->semanticOrdinal < locals_.size() && locals_[target->semanticOrdinal].isRaii &&
                rejectUnavailableAssignmentOwner(expression, target->semanticOrdinal, scopeOrdinal, diagnostics))
                return ExpressionResult{};
        }
        const std::vector<ExpressionResult> arguments = {operand};
        ExpressionResult selected = resolveSyntheticCall(
            expression, methodMemberOrdinal, scopeOrdinal, name, std::move(candidates),
            arguments, nullptr, {}, std::nullopt, false, false, diagnostics, true);
        if (!selected.valid) return ExpressionResult{};
        if (!classifyImplicit(selected.typeKey, operand.typeKey, selected.integerConstant).exists())
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error, expression->range(),
                "increment/decrement operator result must be assignable to its operand");
            return ExpressionResult{};
        }
        const MethodCallBinding* binding = callBinding(expression, methodMemberOrdinal);
        const ClassMemberSymbol* implementation = binding != nullptr &&
            binding->selectedMemberOrdinal.has_value()
            ? methodSymbol(*binding->selectedMemberOrdinal) : nullptr;
        const auto localTarget = expression->first() != nullptr &&
            expression->first()->kind() == ast::AstNodeKind::IdentifierExpression
            ? lookupScopeName(scopeOrdinal, expression->first()->text()) : std::nullopt;
        if (implementation != nullptr && binding->selectedMemberOrdinal.has_value() &&
            memberModel_ != nullptr && *binding->selectedMemberOrdinal < memberModel_->members().size() &&
            implementation->isUserOperator &&
            implementation->parameters.size() == 1 &&
            implementation->parameters[0].passKind == ParameterPassKind::Value &&
            implementation->parameters[0].ownership != sema::OwnershipKind::Owned &&
            implementation->parameters[0].typeKey == operand.typeKey &&
            implementation->declaredOwnership == sema::OwnershipKind::Owned &&
            selected.typeKey == operand.typeKey && isReferenceTypeKey(operand.typeKey) &&
            operand.addressable && localTarget.has_value() && localTarget->kind == ScopeNameKind::Local &&
            localTarget->semanticOrdinal < locals_.size() && locals_[localTarget->semanticOrdinal].isRaii &&
            operand.ownershipSourceLocalOrdinal == localTarget->semanticOrdinal)
        {
            selected.assignable = false;
            selected.addressable = false;
            if (expression->kind() != ast::AstNodeKind::PostfixExpression)
            {
                selected.ownership = sema::OwnershipKind::Borrowed;
                selected.ownershipTransfer = false;
                selected.ownershipFreshOwner = false;
                selected.objectLifetimeOwnership = sema::OwnershipKind::Borrowed;
                selected.storageOwnership = sema::OwnershipKind::Borrowed;
                selected.ownershipSourceLocalOrdinal = localTarget->semanticOrdinal;
            }
            else
                selected.ownedPostfixTemporary = true;
            return selected;
        }
        // A computed receiver can use a local owner only when its source body proves identity.
        const ast::AstNode* fieldReceiver = expression->first() != nullptr &&
            expression->first()->kind() == ast::AstNodeKind::MemberExpression
            ? expression->first()->first() : nullptr;
        const ast::AstNode* fieldRoot = fieldReceiver;
        if (fieldReceiver != nullptr && fieldReceiver->kind() == ast::AstNodeKind::CallExpression &&
            fieldReceiver->children().size() == 1 && memberModel_ != nullptr)
        {
            const MethodCallBinding* receiverCall = callBinding(fieldReceiver, methodMemberOrdinal);
            const ClassMemberSymbol* receiverMethod = receiverCall != nullptr && receiverCall->staticCall &&
                receiverCall->resolution == MethodCallResolutionKind::Resolved &&
                receiverCall->selectedMemberOrdinal.has_value()
                ? methodSymbol(*receiverCall->selectedMemberOrdinal) : nullptr;
            const ast::AstNode* body = receiverMethod != nullptr && receiverMethod->syntax != nullptr
                ? receiverMethod->syntax->first() : nullptr;
            bool proven = receiverMethod != nullptr && !receiverMethod->isPrecompiled &&
                !receiverMethod->isVirtual && !receiverMethod->isExternC &&
                !receiverMethod->isCompilerGenerated && receiverMethod->precompiledMangledName.empty() &&
                receiverMethod->declaredOwnership == sema::OwnershipKind::Borrowed &&
                receiverMethod->parameters.size() == 1 &&
                receiverMethod->parameters[0].ownership == sema::OwnershipKind::Borrowed &&
                receiverMethod->parameters[0].passKind == ParameterPassKind::Value && body != nullptr &&
                body->kind() == ast::AstNodeKind::BlockStatement && !body->children().empty();
            const ast::AstNode* returned = proven ? body->children().back() : nullptr;
            proven = proven && returned != nullptr && returned->kind() == ast::AstNodeKind::ReturnStatement &&
                returned->first() != nullptr && returned->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
                returned->first()->text() == receiverMethod->parameters[0].name;
            const auto isStaticNumeric = [&](const ast::AstNode* node) {
                if (node == nullptr || node->kind() != ast::AstNodeKind::IdentifierExpression ||
                    node->text() == receiverMethod->parameters[0].name) return false;
                for (const std::size_t member : memberModel_->membersOfType(receiverMethod->declaringTypeOrdinal))
                {
                    const ClassMemberSymbol& field = memberModel_->members()[member];
                    if (field.name == node->text() && field.kind == ClassMemberKind::Field && field.isStatic &&
                        ProjectTypeConversion::isNumeric(field.declaredTypeKey)) return true;
                }
                return false;
            };
            if (proven)
                for (std::size_t index = 0; index + 1 < body->children().size(); ++index)
                {
                    const ast::AstNode* statement = body->children()[index];
                    const ast::AstNode* assignment = statement != nullptr &&
                        statement->kind() == ast::AstNodeKind::ExpressionStatement ? statement->first() : nullptr;
                    const ast::AstNode* value = assignment != nullptr ? assignment->second() : nullptr;
                    if (assignment == nullptr || assignment->kind() != ast::AstNodeKind::AssignmentExpression ||
                        assignment->tokenKind() != TokenKind::Equal || !isStaticNumeric(assignment->first()) ||
                        value == nullptr || value->kind() != ast::AstNodeKind::BinaryExpression ||
                        value->tokenKind() != TokenKind::Plus || !isStaticNumeric(value->first()) ||
                        value->second() == nullptr ||
                        value->second()->kind() != ast::AstNodeKind::IntegerLiteralExpression)
                    {
                        proven = false;
                        break;
                    }
                }
            const ast::AstNode* argument = fieldReceiver->children().front();
            const auto argumentName = argument != nullptr && argument->kind() == ast::AstNodeKind::IdentifierExpression
                ? lookupScopeName(scopeOrdinal, argument->text()) : std::nullopt;
            if (proven && argumentName.has_value() && argumentName->kind == ScopeNameKind::Local &&
                argumentName->semanticOrdinal < locals_.size() && locals_[argumentName->semanticOrdinal].isRaii &&
                scopeContains(locals_[argumentName->semanticOrdinal].scopeOrdinal, scopeOrdinal))
                fieldRoot = argument;
        }
        if (implementation != nullptr && memberModel_ != nullptr &&
            implementation->isUserOperator && implementation->parameters.size() == 1 &&
            implementation->parameters[0].passKind == ParameterPassKind::Value &&
            implementation->parameters[0].ownership != sema::OwnershipKind::Owned &&
            implementation->parameters[0].typeKey == operand.typeKey &&
            implementation->declaredOwnership == sema::OwnershipKind::Owned &&
            selected.typeKey == operand.typeKey && isReferenceTypeKey(operand.typeKey) &&
            operand.addressable && operand.assignmentMemberOrdinal.has_value() &&
            *operand.assignmentMemberOrdinal < memberModel_->members().size() &&
            expression->first() != nullptr &&
            expression->first()->kind() == ast::AstNodeKind::MemberExpression &&
            fieldRoot != nullptr && fieldRoot->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const ClassMemberSymbol& field = memberModel_->members()[*operand.assignmentMemberOrdinal];
            const auto root = lookupScopeName(scopeOrdinal, fieldRoot->text());
            std::optional<std::size_t> owner;
            std::optional<std::size_t> parameterOwner;
            const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
            if (root.has_value() && root->kind == ScopeNameKind::Parameter && caller != nullptr &&
                root->semanticOrdinal < caller->parameters.size() &&
                caller->parameters[root->semanticOrdinal].ownership == sema::OwnershipKind::Borrowed)
                parameterOwner = root->semanticOrdinal;
            if (root.has_value() && root->kind == ScopeNameKind::Local && root->semanticOrdinal < locals_.size())
            {
                const MethodLocalSymbol& local = locals_[root->semanticOrdinal];
                if (local.isRaii)
                    owner = root->semanticOrdinal;
                else if (local.localBorrowBindingOrdinal.has_value() &&
                         *local.localBorrowBindingOrdinal < localBorrowBindings_.size())
                {
                    const MethodLocalBorrowBinding& borrow = localBorrowBindings_[*local.localBorrowBindingOrdinal];
                    const auto localCandidate = borrow.sourceOwnerLocalOrdinal.has_value()
                        ? borrow.sourceOwnerLocalOrdinal
                        : (borrow.possibleSourceOwnerLocalOrdinals.empty() ? std::nullopt
                            : std::optional<std::size_t>(borrow.possibleSourceOwnerLocalOrdinals.front()));
                    const auto parameterCandidate = borrow.sourceOwnerParameterOrdinal.has_value()
                        ? borrow.sourceOwnerParameterOrdinal
                        : (borrow.possibleSourceOwnerParameterOrdinals.empty() ? std::nullopt
                            : std::optional<std::size_t>(borrow.possibleSourceOwnerParameterOrdinals.front()));
                    const bool validLocals = (!localCandidate.has_value() ||
                        (*localCandidate < locals_.size() && locals_[*localCandidate].isRaii)) &&
                        std::ranges::all_of(borrow.possibleSourceOwnerLocalOrdinals,
                            [&](std::size_t candidate)
                            { return candidate < locals_.size() && locals_[candidate].isRaii; });
                    const bool validParameters = !parameterCandidate.has_value() || (caller != nullptr &&
                        *parameterCandidate < caller->parameters.size() &&
                        caller->parameters[*parameterCandidate].ownership == sema::OwnershipKind::Borrowed &&
                        std::ranges::all_of(borrow.possibleSourceOwnerParameterOrdinals,
                            [&](std::size_t candidate)
                            { return candidate < caller->parameters.size() &&
                                caller->parameters[candidate].ownership == sema::OwnershipKind::Borrowed; }));
                    if (validLocals && validParameters)
                    {
                        owner = localCandidate;
                        parameterOwner = parameterCandidate;
                    }
                }
            }
            if (field.kind == ClassMemberKind::Field && !field.isStatic &&
                field.declaredOwnership == sema::OwnershipKind::Owned &&
                (parameterOwner.has_value() ||
                 (owner.has_value() && *owner < locals_.size() && locals_[*owner].isRaii)))
            {
                selected.assignable = false;
                selected.addressable = false;
                if (expression->kind() == ast::AstNodeKind::PostfixExpression)
                    selected.ownedPostfixTemporary = true;
                else
                {
                    selected.ownership = sema::OwnershipKind::Borrowed;
                    selected.ownershipTransfer = false;
                    selected.ownershipFreshOwner = false;
                    selected.objectLifetimeOwnership = sema::OwnershipKind::Borrowed;
                    selected.storageOwnership = sema::OwnershipKind::Borrowed;
                    selected.ownershipSourceLocalOrdinal = owner;
                    selected.ownershipSourceParameterOrdinal = parameterOwner;
                    selected.ownsStorageDestination = true;
                }
                return selected;
            }
        }
        if (implementation != nullptr && binding->selectedMemberOrdinal.has_value() &&
            memberModel_ != nullptr && *binding->selectedMemberOrdinal < memberModel_->members().size() &&
            implementation->isUserOperator && implementation->parameters.size() == 1 &&
            ((implementation->parameters[0].passKind == ParameterPassKind::Value &&
              implementation->parameters[0].typeKey == operand.typeKey) ||
             ((implementation->parameters[0].passKind == ParameterPassKind::In ||
               implementation->parameters[0].passKind == ParameterPassKind::Ref) &&
              implementation->parameters[0].pointeeTypeKey == operand.typeKey)) &&
            selected.typeKey == operand.typeKey && operand.addressable &&
            typeKeyIsStruct(operand.typeKey) &&
            !memberModel_->typeIsMoveOnly(operand.typeKey))
        {
            selected.assignable = false;
            selected.addressable = false;
            return selected;
        }
        // Do not lower this as an ordinary call: updating the destination must evaluate it once.
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error, expression->range(),
            "user-defined increment/decrement update is not implemented");
        return ExpressionResult{};
    }
if (operation != TokenKind::Amp && operation != TokenKind::Star &&
    operation != TokenKind::PlusPlus && operation != TokenKind::MinusMinus)
{
    const std::string operatorName = stage38OperatorMethodName(operation, 1);
    const std::string operatorTraitKey = stage38OperatorTraitKey(operation, 1);
    if (!operatorName.empty() && !operatorTraitKey.empty())
    {
        std::vector<std::size_t> candidates = stage38OperatorCandidates(
            operatorTraitKey, operatorName, operand.typeKey, std::nullopt);
        if (!candidates.empty())
        {
            const std::vector<ExpressionResult> arguments = {operand};
            ExpressionResult overloaded = resolveSyntheticCall(
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                operatorName,
                std::move(candidates),
                arguments,
                nullptr,
                {},
                std::nullopt,
                false,
                false,
                diagnostics,
                true);
            if (overloaded.valid)
            {
                overloaded.assignable = false;
                overloaded.addressable = false;
                return overloaded;
            }
            return ExpressionResult{};
        }
    }
}
    return std::nullopt;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeLogicalNot(
    const ast::AstNode* expression,
    const ExpressionResult& operand,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    const TokenKind operation = expression->tokenKind();
if (operation == TokenKind::Bang)
{
    if (operand.typeKey != "builtin:bool" &&
        !classifyImportedConditionConversion(operand).exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "operator ! requires bool operand");
        return result;
    }
    result.typeKey = "builtin:bool";
    result.valid = true;
    if (const auto flow = conditionFlowBySyntax_.find(expression->first());
        flow != conditionFlowBySyntax_.end())
    {
        ConditionFlowStates negated;
        negated.whenTrue = flow->second.whenFalse;
        negated.whenFalse = flow->second.whenTrue;
        conditionFlowBySyntax_.insert_or_assign(expression, std::move(negated));
    }
    return result;
}
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeAddressOf(
    const ast::AstNode* expression,
    const ExpressionResult& operand,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    const TokenKind operation = expression->tokenKind();
if (operation == TokenKind::Amp)
{
    // T19-002: `&a` of C atomic scalar storage, never `ref`/`in`/`out`. / `&a` de almacenamiento C atomico
    //     escalar, nunca `ref`/`in`/`out`.
    const bool atomicAddress = operand.cppAtomicStorage && expression != nullptr &&
        !expression->auxiliaryText().starts_with("pass:");
    // T19-038: `&v` of a volatile C global, never `ref`/`in`/`out`. / `&v` de una global C volatile, nunca
    //     `ref`/`in`/`out`.
    const bool volatileAddress = operand.cppVolatileGlobalAddress && expression != nullptr &&
        !expression->auxiliaryText().starts_with("pass:");
    if (!operand.addressable && !atomicAddress && !volatileAddress)
    {
        // EN: An indexed read gets its own explanation: the generic text named bit fields for a
        //     string character or a by-value indexer result.
        // ES: Una lectura indexada tiene su propia explicacion: el texto generico nombraba campos de
        //     bits para un caracter de cadena o el resultado de un indexador por valor.
        const ast::AstNode* target = expression->first();
        std::string message = "bit fields have no address; operand of & must name addressable storage, "
            "not a bit field or temporary value";
        if (target != nullptr && target->kind() == ast::AstNodeKind::IndexExpression)
            message = operand.typeKey == "builtin:char"
                ? "an indexed character is a value, not addressable storage; "
                  "String.CharacterPointer(text, index) returns the address of a string character"
                : "an indexer that returns by value yields a temporary, which has no address; "
                  "an indexer whose getter returns ref can be addressed";
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAddressOf,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            operand.cppAtomicStorage
                ? "C atomic storage cannot be passed by ref, in or out; pass its address (&) to a C parameter of "
                  "type '_Atomic T*'"
                : operand.cppPointerPointeeAtomic
                ? "cannot take the address of a pointer to C atomic storage until nested qualified pointers are "
                  "supported"
                : operand.volatileStorage
                ? "cannot take the address of volatile storage until qualified pointers are supported"
                : operand.pointerPointeeVolatile
                ? "cannot take the address of a pointer to volatile storage until nested qualified pointers are supported"
                : operand.readonlyReceiver
                ? "cannot take the address of readonly storage until qualified pointers are supported"
                : message);
        return result;
    }
    // EN: T08-D17: the address of a pointer is a pointer to pointer, whose qualifier belongs to the base type
    //     (`&p` with `p` a `T const*` is a `T const**`). A readonly pointer would need `T* const*`, a qualifier on
    //     an intermediate pointer that NativeSharp does not represent.
    // ES: T08-D17: la direccion de un puntero es un puntero a puntero, cuyo calificador es del tipo base (`&p` con
    //     `p` de tipo `T const*` es un `T const**`). Un puntero de solo lectura exigiria `T* const*`, un calificador
    //     en un puntero intermedio que NativeSharp no representa.
    const bool pointerOperand = ProjectTypeConversion::isPointer(operand.typeKey);
    // EN: C1-02: the address of a pointer carries the pointer's own qualifiers as level 0 of the mask (`&p` of a
    //     readonly `int*` is an `int* const*`, of a volatile one an `int* volatile*`).
    // ES: C1-02: la direccion de un puntero lleva los calificadores del propio puntero como nivel 0 de la mascara.
    std::uint8_t addressLevels = 0;
    if (pointerOperand && !atomicAddress &&
        !language::pointerLevelsAfterAddressOf(operand.pointerLevelQualifiers,
            !operand.assignable || operand.readonlyReceiver, operand.volatileStorage, addressLevels))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAddressOf,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot take the address of this pointer: the result would need more than three qualified "
            "intermediate pointer levels");
        return result;
    }
    result.typeKey = operand.typeKey + '*';
    result.valid = true;
    result.cppPointerPointeeAtomic = atomicAddress;
    result.cppAtomicPointeeHoldsVolatile = atomicAddress && pointerOperand && operand.pointerPointeeVolatile;
    // T19-002: `&a` of a `volatile _Atomic` global points to volatile atomic storage. / `&a` de una global
    //     `volatile _Atomic` apunta a almacenamiento atomico volatile.
    if (atomicAddress) result.pointerPointeeVolatile = operand.volatileStorage;
    // EN: T19-038. `&v` of a volatile C global points to volatile data and starts the C contract (T08-D13) when the
    //     pointee is a scalar, so aliases keep its volatile mode and alignment.
    // ES: T19-038. `&v` de una global C volatile apunta a dato volatile e inicia el contrato C (T08-D13) si el
    //     apuntado es escalar, asi que los alias conservan su modo volatile y su alineacion.
    if (pointerOperand && !atomicAddress)
    {
        result.pointerLevelQualifiers = addressLevels;
        result.pointerPointeeVolatile = operand.pointerPointeeVolatile;
        result.cppVolatilePointeeContract = operand.cppVolatilePointeeContract;
        result.cppVolatilePointeeMicrosoft = operand.cppVolatilePointeeMicrosoft;
        result.cppVolatilePointeeAlignment = operand.cppVolatilePointeeAlignment;
    }
    else if (volatileAddress)
    {
        result.pointerPointeeVolatile = true;
        result.cppCOrigin = operand.cppCOrigin;
        if (operand.cppVolatilePointeeAlignment != 0)
        {
            result.cppVolatilePointeeContract = true;
            result.cppVolatilePointeeMicrosoft = operand.cppVolatilePointeeMicrosoft;
            result.cppVolatilePointeeAlignment = operand.cppVolatilePointeeAlignment;
        }
    }
    result.pointerPointeeReadonly = pointerOperand
        ? operand.pointerPointeeReadonly
        : !operand.assignable || operand.readonlyReceiver;
    result.pointerNullable = false;
    result.pointerHasProvenance = true;
    result.pointerKnownAlignment = 1;
    result.pointerAddressSpace = 0;
    result.onePastEndPointer = operand.onePastEndAddress;
    // C4: `f(ref x)` parses as the address-of expression the callee's pointer parameter
    // expects, carrying the keyword the programmer wrote so the call can be checked against
    // the signature. An `out` argument is the one case that does NOT require the operand to
    // hold a value first -- the callee is the one that assigns it -- but it still has to be
    // assignable, which the check above already established.
    if (expression != nullptr && expression->auxiliaryText().starts_with("pass:"))
    {
        const std::string spelling = expression->auxiliaryText().substr(std::string("pass:").size());
        result.argumentPassKind = spelling == "ref"
            ? ParameterPassKind::Ref
            : (spelling == "out" ? ParameterPassKind::Out : ParameterPassKind::In);
    }
    return result;
}
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeDereference(
    const ast::AstNode* expression,
    const ExpressionResult& operand,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    const TokenKind operation = expression->tokenKind();
if (operation == TokenKind::Star)
{
    if (!ProjectTypeConversion::isPointer(operand.typeKey) || operand.typeKey == "builtin:void*")
    {
        // IDs 80/83. Desde que el parser convierte `p->x` en `(*p).x`, este `*` puede ser uno
        // que la persona NO escribio. Un mensaje que hable de `*` ahi es desorientador -- lo
        // descubrio `nsc.project-member-model`, que comprueba que `this->Value` se rechaza CON
        // DiagnosticId::InvalidMemberAccess, y dejo de pasar. El nodo sintetizado se marca en
        // el parser, asi que aqui se puede nombrar lo que de verdad se escribio, y salir con
        // el mismo id con el que salia cuando `->` tenia nodo propio.
        const bool writtenAsArrow = expression != nullptr && expression->auxiliaryText() == "arrow";
        diagnostics.report(
            writtenAsArrow
                ? diagnostics::DiagnosticId::InvalidMemberAccess
                : diagnostics::DiagnosticId::InvalidDereference,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            writtenAsArrow
                ? "left side of '->' must be a non-void pointer, but '" + operand.typeKey +
                    "' is not one; use '.' for a value or a class reference"
                : "operand of * must be a non-void pointer");
        return result;
    }
    if (operand.onePastEndPointer)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidDereference,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "a one-past-end pointer is a comparison/iteration sentinel and cannot be dereferenced");
        return result;
    }
    result.typeKey = operand.typeKey.substr(0, operand.typeKey.size() - 1);
    result.valid = true;
    // EN: T08-D17: `*q` with `q` a `T const**` is a mutable `T const*`: the qualifier passes to the result.
    // ES: T08-D17: `*q` con `q` de tipo `T const**` es un `T const*` modificable: el calificador pasa al resultado.
    const bool nestedPointee = method_body_detail::nestedPointerTypeKey(operand.typeKey);
    // EN: C1-02/C1-04: in a chain, `*q` selects the level-0 pointer object, qualified by level 0 of the mask; the base
    //     marks and the rest of the mask pass to the result (`*q` of an `int volatile* const*` is a const slot
    //     holding an `int volatile*`).
    // ES: C1-02/C1-04: en una cadena, `*q` selecciona el objeto puntero del nivel 0, calificado por el nivel 0 de la
    //     mascara; las marcas de la base y el resto de la mascara pasan al resultado.
    const bool levelConst = nestedPointee && language::pointerLevelConst(operand.pointerLevelQualifiers, 0);
    const bool levelVolatile = nestedPointee && language::pointerLevelVolatile(operand.pointerLevelQualifiers, 0);
    result.assignable = nestedPointee ? !levelConst : !operand.pointerPointeeReadonly;
    result.volatileStorage = nestedPointee ? levelVolatile : operand.pointerPointeeVolatile;
    result.pointerPointeeVolatile = nestedPointee && operand.pointerPointeeVolatile;
    result.pointerLevelQualifiers =
        nestedPointee ? language::pointerLevelsAfterDereference(operand.pointerLevelQualifiers) : 0U;
    if (nestedPointee && operand.cppVolatilePointeeContract)
    {
        // The C contract describes the base access; it follows the chain. / El contrato C describe el acceso a la
        //     base; sigue a la cadena.
        result.cppVolatilePointeeContract = true;
        result.cppVolatilePointeeMicrosoft = operand.cppVolatilePointeeMicrosoft;
        result.cppVolatilePointeeAlignment = operand.cppVolatilePointeeAlignment;
    }
    // T19-002: `*p` of a pointer to atomic storage is atomic storage. / `*p` de un puntero a almacenamiento
    //     atomico es almacenamiento atomico.
    result.cppAtomicStorage = operand.cppPointerPointeeAtomic;
    // T19-038: the atomic storage holds a pointer to volatile data. / El almacenamiento atomico guarda un puntero
    //     a dato volatile.
    if (operand.cppAtomicPointeeHoldsVolatile) result.pointerPointeeVolatile = true;
    // T19-002: an atomic record is accessed as a whole value, as a global one (T08-D07). / Un record atomico se
    //     accede como valor entero, como uno global (T08-D07).
    result.cppAtomicRecordStorage = operand.cppPointerPointeeAtomic && nameLookup_ != nullptr &&
        result.typeKey.starts_with("type:") && [&] {
            const auto ordinal = nameLookup_->typeOrdinalForIdentity(result.typeKey.substr(5));
            const auto* imported = ordinal ? nameLookup_->cppImportedType(*ordinal) : nullptr;
            return imported && (imported->origin == CppImportedTypeOrigin::CStruct ||
                                imported->origin == CppImportedTypeOrigin::CUnion);
        }();
    // C1-02: a qualified pointer slot has an address, which carries its qualifiers as level 0. / Una posicion
    //     puntero calificada tiene direccion, que lleva sus calificadores como nivel 0.
    result.addressable = (nestedPointee || !result.volatileStorage) && !operand.cppPointerPointeeAtomic;
    result.readonlyReceiver = nestedPointee ? levelConst : operand.pointerPointeeReadonly;
    result.pointerPointeeReadonly = nestedPointee && operand.pointerPointeeReadonly;
    // EN: T08-D11/D12. The pointee of a C pointer value is C storage.
    // ES: T08-D11/D12. El dato apuntado por un puntero C es almacenamiento C.
    result.cppCOrigin = operand.cppCOrigin;
    return result;
}
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeIncrementOrDecrement(
    const ast::AstNode* expression,
    const ExpressionResult& operand,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    const TokenKind operation = expression->tokenKind();
if (operation == TokenKind::PlusPlus || operation == TokenKind::MinusMinus)
{
    // ID 79. Un puntero NO VOID tambien se incrementa, y avanza un ELEMENTO -- la misma regla
    // que `p + 1`, porque es la misma operacion. Dejarlo fuera repetiria al reves el error de
    // la tarea 62: `++i` andando y `++p` no, cuando lo unico que cambia es el paso.
    // `void*` queda fuera por la misma razon por la que `*p` lo rechaza: no hay tamano que
    // avanzar.
    const bool pointerOperand = ProjectTypeConversion::isPointer(operand.typeKey) &&
        operand.typeKey != "builtin:void*";
    if (!operand.assignable ||
        !(ProjectTypeConversion::isNumeric(operand.typeKey) || pointerOperand ||
          cppImportedCEnumUnderlying(nameLookup_, operand.typeKey)))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "increment/decrement requires a modifiable numeric or non-void pointer operand");
        return result;
    }
    if (pointerOperand && nameLookup_)
    {
        const std::string element = operand.typeKey.substr(0, operand.typeKey.size() - 1);
        if (element.starts_with("type:") && !element.ends_with("*"))
        {
            const auto ordinal = nameLookup_->typeOrdinalForIdentity(std::string_view(element).substr(5));
            const auto* imported = ordinal ? nameLookup_->cppImportedType(*ordinal) : nullptr;
            if (imported && imported->isIncomplete &&
                (imported->origin == CppImportedTypeOrigin::CStruct ||
                 imported->origin == CppImportedTypeOrigin::CUnion))
            {
                diagnostics.report(diagnostics::DiagnosticId::IncompleteType,
                    diagnostics::DiagnosticSeverity::Error, expression->range(),
                    "pointer arithmetic requires a complete C type; '" +
                        imported->identity.fullyQualifiedName() + "' has no element size");
                return result;
            }
        }
    }
    result.typeKey = operand.typeKey;
    result.valid = true;
    return result;
}
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeUnaryNumeric(
    const ast::AstNode* expression,
    const ExpressionResult& operand,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    const TokenKind operation = expression->tokenKind();
if (operation == TokenKind::Plus || operation == TokenKind::Minus || operation == TokenKind::Tilde)
{
    // NSC-LIMIT-012. `~enum` preserves the enum identity; unary + and - remain numeric-only.
    const auto cEnum = cppImportedCEnumUnderlying(nameLookup_, operand.typeKey);
    const bool enumComplement = !cEnum && operation == TokenKind::Tilde && isEnumTypeKey(operand.typeKey);
    const std::string numericType = cEnum.value_or(operand.typeKey);
    if ((!ProjectTypeConversion::isNumeric(numericType) || numericType == "builtin:bool") &&
        !enumComplement)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "unary numeric operator requires numeric operand");
        return result;
    }
    result.typeKey = cEnum ? shiftResultType(numericType) : operand.typeKey;
    result.valid = true;
    if (operation == TokenKind::Plus)
    {
        result.integerConstant = operand.integerConstant;
    }
    else if (operation == TokenKind::Minus && operand.integerConstant.has_value() &&
             !operand.integerConstant->isUnsigned && !operand.integerConstant->isNegative)
    {
        // ID 119. `-3` no conservaba su constancia: unario `+` la propagaba y unario `-` no,
        // asi que `short s = 3;` compilaba y `short s = -3;` NO -- el mismo literal, con signo,
        // dejaba de caber. La descubrio la fixture c13 al declarar `short smallS = -3;`.
        //
        // Se guarda la MAGNITUD mas la bandera, no el valor negado: `value` se compara contra
        // cotas superiores en constantFits, y 0xFFFF...FD no pasaria ninguna. Ver
        // ProjectIntegerConstant::isNegative.
        //
        // No se dobla `-(-3)` ni `-(3u)`: en el primero la constancia ya venia marcada
        // negativa y en el segundo el literal es sin signo. Los dos son raros y ninguno vale
        // el riesgo de una regla de signo escrita a medias.
        ProjectIntegerConstant negated = *operand.integerConstant;
        negated.isNegative = negated.value != 0;
        result.integerConstant = negated;
    }
    return result;
}
    result = operand;
    result.assignable = false;
    return result;
}

}
