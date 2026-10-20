#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "MethodBodyInternals.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
using lexer::TokenKind;
using method_body_detail::pointerContract;

namespace
{
bool isGenericValueTypeKey(std::string_view typeKey) noexcept
{
    return typeKey.starts_with("generic:");
}

bool isErrorType(std::string_view key) noexcept
{
    return key.empty() || key == "<error>";
}

bool isShift(TokenKind kind) noexcept
{
    return kind == TokenKind::LessLess || kind == TokenKind::GreaterGreater;
}
}

void ProjectMethodBodyModel::recordReferenceFieldAssignmentOrigin(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NS304-T11-F07: record the storage origin of a `field = ref storage` assignment keyed by the
    // ref field's member ordinal, so the escape evaluator propagates the storage origin instead of
    // the receiver struct value (which is copied on return and would hide the dangling storage).
    if (expression->isReference() && expression->tokenKind() == TokenKind::Equal &&
        expression->first() != nullptr && expression->second() != nullptr &&
        memberModel_ != nullptr && inheritanceModel_ != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::MemberExpression &&
        expression->first()->first() != nullptr)
    {
        const ExpressionResult receiver = analyzeExpression(
            expression->first()->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (receiver.valid)
        {
            const std::optional<std::size_t> receiverType =
                inheritanceModel_->typeOrdinalForTypeKey(receiver.typeKey);
            if (receiverType.has_value())
            {
                for (const std::size_t fieldOrdinal :
                     memberModel_->findMembers(*receiverType, expression->first()->text()))
                {
                    if (fieldOrdinal >= memberModel_->members().size()) continue;
                    if (!memberModel_->members()[fieldOrdinal].isReferenceReturn) continue;
                    refFieldOrigins_[fieldOrdinal] =
                        referenceEscapeOriginOfExpression(expression->second(), scopeOrdinal);
                }
            }
        }
    }
}

void ProjectMethodBodyModel::reportScopeAssignmentEscape(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // NS304-T13-F03: a `scope` object cannot be stored in a field/static (it would outlive its frame).
    if (expression->first() != nullptr && expression->second() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::MemberExpression)
    {
        std::vector<const ast::AstNode*> pending;
        pending.push_back(expression->second());
        while (!pending.empty())
        {
            const ast::AstNode* node = pending.back();
            pending.pop_back();
            if (node == nullptr) continue;
            if (node->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, node->text());
                if (name.has_value() && name->kind == ScopeNameKind::Local &&
                    name->semanticOrdinal < locals_.size() && locals_[name->semanticOrdinal].isScope)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->second()->range(),
                        "scope object '" + locals_[name->semanticOrdinal].name +
                            "' cannot be stored in a field or static because it would outlive its frame");
                    break;
                }
            }
            if (node->first() != nullptr) pending.push_back(node->first());
            for (const ast::AstNode* child : node->children()) pending.push_back(child);
        }
    }
}

bool ProjectMethodBodyModel::assignmentConflictsWithIterationBorrow(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (expression->first() != nullptr && !activeIterationBorrows_.empty())
    {
        const std::optional<std::string> targetPlace =
            storagePlaceKey(expression->first(), scopeOrdinal);
        if (targetPlace.has_value())
        {
            for (auto iterator = activeIterationBorrows_.rbegin();
                 iterator != activeIterationBorrows_.rend(); ++iterator)
            {
                if (!placesOverlap(iterator->sourcePlace, *targetPlace)) continue;
                bool directElementWrite = false;
                if (iterator->mutableElements &&
                    expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
                {
                    const std::optional<ScopeName> targetName =
                        lookupScopeName(scopeOrdinal, expression->first()->text());
                    directElementWrite = targetName.has_value() &&
                        targetName->kind == ScopeNameKind::Local &&
                        targetName->semanticOrdinal < locals_.size() &&
                        locals_[targetName->semanticOrdinal].syntax != nullptr &&
                        locals_[targetName->semanticOrdinal].syntax->auxiliaryText() ==
                            "local:foreach-ref-mut";
                }
                if (iterator->mutableElements &&
                    !directElementWrite &&
                    expression->first()->kind() == ast::AstNodeKind::IndexExpression &&
                    expression->first()->first() != nullptr)
                {
                    const std::optional<std::string> indexedSource =
                        storagePlaceKey(expression->first()->first(), scopeOrdinal);
                    directElementWrite = indexedSource.has_value() &&
                        *indexedSource == iterator->sourcePlace;
                }
                if (directElementWrite) break;
                diagnostics.report(
                    diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->first()->range(),
                    "cannot mutate foreach source storage while a lending iteration is active; "
                    "use the ref mut element binding for compatible element writes");
                return true;
            }
        }
    }
    return false;
}

void ProjectMethodBodyModel::invalidateAssignedStorage(
    const ast::AstNode* target,
    std::size_t scopeOrdinal)
{
    const ast::AstNode* root = definiteAssignmentTargetRoot(target);
    if (root == nullptr || root->kind() != ast::AstNodeKind::IdentifierExpression) return;
    const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, root->text());
    if (!name.has_value() ||
        (name->kind != ScopeNameKind::Local && name->kind != ScopeNameKind::Parameter)) return;
    const std::size_t storage = name->semanticOrdinal * 2U +
        (name->kind == ScopeNameKind::Parameter ? 1U : 0U);
    nonNullStorages_.erase(storage);
    integerRanges_.erase(storage);
    activeVariantCases_.erase(storage);
}

bool ProjectMethodBodyModel::assignmentNullCarrier(std::string_view typeKey) const
{
    if (ProjectTypeConversion::isPointer(typeKey) ||
        ProjectTypeConversion::isNativeArray(typeKey) ||
        typeKey == "builtin:string" || isAnyDelegateTypeKey(typeKey))
    {
        return true;
    }
    const std::optional<std::size_t> ordinal = referenceTypeOrdinalForTypeKey(typeKey);
    return ordinal.has_value() && inheritanceModel_ != nullptr &&
        (inheritanceModel_->isClass(*ordinal) ||
         (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)));
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeNullCoalescingAssignment(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // T01-F03 `place ??= fallback` is not a numeric compound assignment. Analyze it here before
    // the generic read/modify/write machinery asks compoundAssignmentBaseOperator for a numeric
    // operator. This deliberately accepts only real addressable storage: a property/indexer would
    // require one captured receiver shared by getter and setter, which this HIR cannot represent
    // safely yet, so it receives a source diagnostic instead of being evaluated twice.
    ExpressionResult target = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
        false, false, false, false);
        bool valid = target.valid;
        if (valid && (!target.assignable || !target.addressable))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                diagnostics::DiagnosticSeverity::Error,
                expression->first() != nullptr ? expression->first()->range() : expression->range(),
                "left operand of '?' '?=' must be addressable storage; cache a property/indexer "
                "receiver explicitly before using null-coalescing assignment");
            valid = false;
        }
        if (valid && !assignmentNullCarrier(target.typeKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->first() != nullptr ? expression->first()->range() : expression->range(),
                "left operand of '?' '?=' must have a nullable pointer/reference carrier");
            valid = false;
        }

        const OwnershipFlowSnapshot branchEntry = captureOwnershipFlow();
        ExpressionResult fallback;
        const OwnershipFlowSnapshot fallbackState = analyzeBranchState(
            branchEntry,
            [&]
            {
                fallback = analyzeExpression(
                    expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            });
        commitBranchJoin(branchEntry, fallbackState);
        if (!fallback.valid) valid = false;
        if (valid)
        {
            const ProjectImplicitConversion conversion = classifyImplicit(
                fallback.typeKey, target.typeKey, fallback.integerConstant);
            if (!conversion.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->second() != nullptr ? expression->second()->range() : expression->range(),
                    "right operand of '?' '?=' with type '" + fallback.typeKey +
                        "' is not implicitly convertible to '" + target.typeKey + "'");
                valid = false;
            }
        }
        target.valid = valid;
        target.assignable = false;
        if (valid) invalidateAssignedStorage(expression->first(), scopeOrdinal);
    return target;
}

void ProjectMethodBodyModel::bindAssignmentDeconstructionTarget(
    bool& valid,
    const ast::AstNode* expression,
    const ast::AstNode* target,
    const std::string& sourceTypeKey,
    const std::string& path,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (target == nullptr)
    {
        valid = false;
        return;
    }
    if (target->kind() == ast::AstNodeKind::TupleExpression)
    {
        const std::optional<std::vector<std::string>> nested =
            ProjectTypeConversion::tupleElementTypeKeys(sourceTypeKey);
        if (!nested.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                target->range(),
                "nested deconstruction target at " + path +
                    " requires a tuple source element");
            valid = false;
            return;
        }
        if (nested->size() != target->children().size())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                target->range(),
                "nested deconstruction target at " + path + " has " +
                    std::to_string(target->children().size()) +
                    " element(s), but its source tuple has " +
                    std::to_string(nested->size()));
            valid = false;
            return;
        }
        for (std::size_t index = 0; index < nested->size(); ++index)
        {
            bindAssignmentDeconstructionTarget(
                valid, expression, target->children()[index], (*nested)[index],
                path + "." + std::to_string(index), methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        return;
    }
    if (target->kind() == ast::AstNodeKind::IdentifierExpression && target->text() == "_") return;
    const ast::AstNode* previousWriteTarget = definiteAssignmentWriteTarget_;
    markAssignmentTarget(target, scopeOrdinal);
    const ExpressionResult targetResult = analyzeExpression(
        target, methodMemberOrdinal, scopeOrdinal, diagnostics, false, true, false, true);
    definiteAssignmentWriteTarget_ = previousWriteTarget;
    if (!targetResult.valid || !targetResult.assignable || !targetResult.addressable)
    {
        if (targetResult.valid)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                diagnostics::DiagnosticSeverity::Error,
                target != nullptr ? target->range() : expression->first()->range(),
                "tuple deconstruction target must be addressable storage or '_'");
        }
        valid = false;
        return;
    }
    const ProjectImplicitConversion conversion = classifyImplicit(
        sourceTypeKey, targetResult.typeKey, std::nullopt);
    if (!conversion.exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            target->range(),
            "deconstruction element " + path + " of type '" + sourceTypeKey +
                "' cannot convert to target type '" + targetResult.typeKey + "'");
        valid = false;
    }
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeTupleDeconstructionAssignment(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{

    // NativeSharp 3.0 T02-F03. Evaluate the complete source first, resolve a native tuple or the
    // nominal Deconstruct() protocol exactly once, then validate every target leaf left-to-right.
    // HIR materializes the resulting tuple before taking any destination address, which makes
    // `(a, b) = (b, a)` alias-safe and fixes the observable write order for nested patterns too.
    const ExpressionResult source = analyzeExpression(
        expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        const std::optional<MethodDeconstructionBinding> deconstruction = source.valid
            ? bindDeconstructionSource(
                expression, expression->second(), source, methodMemberOrdinal, diagnostics)
            : std::nullopt;
        const std::optional<std::vector<std::string>> sourceElements =
            deconstruction.has_value()
                ? ProjectTypeConversion::tupleElementTypeKeys(deconstruction->tupleTypeKey)
                : std::nullopt;
        bool valid = source.valid && deconstruction.has_value() && sourceElements.has_value();
        if (sourceElements.has_value() &&
            sourceElements->size() != expression->first()->children().size())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->first()->range(),
                "deconstruction target has " +
                    std::to_string(expression->first()->children().size()) +
                    " element(s), but the source tuple has " +
                    std::to_string(sourceElements->size()));
            valid = false;
        }
        if (deconstruction.has_value() &&
            !elementTypeHasTrivialLifetime(deconstruction->tupleTypeKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "tuple deconstruction of owning or move-only elements requires explicit move "
                "bindings and per-element drop flags");
            valid = false;
        }
        if (deconstruction.has_value())
        {
            deconstructionBindingBySyntax_[expression] = deconstructionBindings_.size();
            deconstructionBindings_.push_back(*deconstruction);
        }

        if (sourceElements.has_value() &&
            sourceElements->size() == expression->first()->children().size())
        {
            for (std::size_t index = 0; index < sourceElements->size(); ++index)
            {
                bindAssignmentDeconstructionTarget(
                    valid, expression, expression->first()->children()[index],
                    (*sourceElements)[index], std::to_string(index), methodMemberOrdinal,
                    scopeOrdinal, diagnostics);
            }
        }
        ExpressionResult result = source;
        if (deconstruction.has_value()) result.typeKey = deconstruction->tupleTypeKey;
        result.valid = valid;
        result.assignable = false;
        result.addressable = false;
    return result;
}

void ProjectMethodBodyModel::recordAssignedOutParameter(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // C4 "language references": record that an `out` parameter was written to. The obligation an
    // `out` parameter carries -- the callee MUST assign it -- is checked once the whole body has
    // been analyzed (see verifyOutParameterAssignments), and this is where the evidence is
    // collected.
    // Writing THROUGH an out parameter counts as writing it, whichever way the write is
    // spelled: `rect = other;`, `rect.left = 0;`, `values[0] = 1;`, `*p = v;`. The
    // GetMessage/GetClientRect shape -- the callee filling a record the caller owns, field by
    // field -- is the whole reason `out` exists for a struct, so the check walks the
    // assignment target down to the name it is rooted at.
    const ast::AstNode* rootTarget = expression->first();
    while (rootTarget != nullptr && rootTarget->kind() != ast::AstNodeKind::IdentifierExpression)
    {
        switch (rootTarget->kind())
        {
            case ast::AstNodeKind::MemberExpression:
            case ast::AstNodeKind::IndexExpression:
            case ast::AstNodeKind::UnaryExpression:
                rootTarget = rootTarget->first();
                continue;
            default:
                rootTarget = nullptr;
                continue;
        }
    }
    if (rootTarget == nullptr) return;
    const std::optional<ScopeName> outTarget = lookupScopeName(scopeOrdinal, rootTarget->text());
    if (outTarget.has_value() && outTarget->kind == ScopeNameKind::Parameter)
    {
        assignedOutParameters_.insert({methodMemberOrdinal, outTarget->semanticOrdinal});
    }
}

void ProjectMethodBodyModel::classifyDirectAssignmentTarget(
    ExpressionResult& left,
    std::optional<std::size_t>& raiiTargetLocal,
    std::optional<std::size_t>& moveOnlyTargetLocal,
    std::optional<std::size_t>& moveOnlyTargetParameter,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    if (expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IdentifierExpression) return;
    const std::optional<ScopeName> target =
        lookupScopeName(scopeOrdinal, expression->first()->text());
    if (target.has_value() && target->kind == ScopeNameKind::Local &&
        target->semanticOrdinal < locals_.size() &&
        (locals_[target->semanticOrdinal].isRaii || locals_[target->semanticOrdinal].isMoveOnly))
    {
        const MethodLocalSymbol& local = locals_[target->semanticOrdinal];
        if (local.isRaii) raiiTargetLocal = target->semanticOrdinal;
        if (local.isMoveOnly) moveOnlyTargetLocal = target->semanticOrdinal;
        left.typeKey = local.typeKey;
        left.valid = true;
        left.assignable = true;
        recordNameBinding(MethodNameBinding{
            MethodNameBindingKind::Local, expression->first(), methodMemberOrdinal, scopeOrdinal,
            local.name, local.typeKey, target->semanticOrdinal, std::nullopt, {}, nullptr, "",
            std::nullopt, false});
        return;
    }
    if (!target.has_value() || target->kind != ScopeNameKind::Parameter) return;
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method == nullptr || target->semanticOrdinal >= method->parameters.size()) return;
    const MethodParameterSymbol& parameter = method->parameters[target->semanticOrdinal];
    if (parameter.passKind != ParameterPassKind::Value ||
        !memberModel_->typeIsMoveOnly(parameter.typeKey)) return;
    moveOnlyTargetParameter = target->semanticOrdinal;
    left.typeKey = parameter.typeKey;
    left.valid = true;
    left.assignable = true;
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::Parameter, expression->first(), methodMemberOrdinal, scopeOrdinal,
        parameter.name, parameter.typeKey, std::nullopt, target->semanticOrdinal, {}, nullptr, "",
        std::nullopt, false});
}

bool ProjectMethodBodyModel::analyzeAssignmentLeft(
    ExpressionResult& left,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // C7 "definite assignment": a plain `=` assigns its target; a compound one (`x += 1`) reads it
    // first and therefore requires it to be assigned already, so only `=` marks and suppresses.
    // markAssignmentTarget must run BEFORE the left-hand side is analyzed, because analyzing it is
    // what would otherwise report the target as a read of an unassigned local.
    const ast::AstNode* const previousWriteTarget = definiteAssignmentWriteTarget_;
    const ast::AstNode* const previousUnionWriteTarget = activeUnionWriteTarget_;
    if (expression->tokenKind() == TokenKind::Equal)
    {
        markAssignmentTarget(expression->first(), scopeOrdinal);
        activeUnionWriteTarget_ = expression->first();
    }
    // NS304-T10-F05: un null-conditional write admite campos de tipo valor (el READ nullable lo
    // rechazaria); el flag suprime el chequeo de resultado nullable durante el analisis del LHS.
    const bool nullConditionalWrite = expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression;
    const bool previousNullConditionalWrite = activeNullConditionalWrite_;
    activeNullConditionalWrite_ = nullConditionalWrite;
    if (!left.valid)
    {
        left = analyzeExpression(
            expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
            false, true, false, expression->tokenKind() == TokenKind::Equal);
    }
    activeNullConditionalWrite_ = previousNullConditionalWrite;
    definiteAssignmentWriteTarget_ = previousWriteTarget;
    activeUnionWriteTarget_ = previousUnionWriteTarget;
    return left.valid;
}

void ProjectMethodBodyModel::trackRequiredFieldAssignment(
    const ExpressionResult& left,
    std::size_t methodMemberOrdinal)
{
    // NS304-T09-F01: track assignments on the active constructor path. Branch joins intersect
    // this set, and only the final normal-exit intersection is published for verification.
    if (!left.assignable || !left.assignmentMemberOrdinal.has_value() || memberModel_ == nullptr ||
        *left.assignmentMemberOrdinal >= memberModel_->members().size()) return;
    const ClassMemberSymbol& targetMember =
        memberModel_->members()[*left.assignmentMemberOrdinal];
    if (targetMember.kind != ClassMemberKind::Field || !targetMember.isRequired ||
        targetMember.isStatic) return;
    const ClassMemberSymbol* callable = methodSymbol(methodMemberOrdinal);
    if (callable != nullptr && callable->kind == ClassMemberKind::Constructor && !callable->isStatic)
    {
        activeRequiredFieldAssignments_.insert(*left.assignmentMemberOrdinal);
    }
}

void ProjectMethodBodyModel::classifyAssignedUnion(
    std::optional<std::size_t>& assignedUnionStorage,
    std::optional<std::size_t>& assignedUnionMember,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal) const
{
    if (expression->tokenKind() != TokenKind::Equal || expression->first() == nullptr) return;
    const ast::AstNode* storageSyntax = expression->first();
    if (storageSyntax->kind() == ast::AstNodeKind::MemberExpression)
    {
        if (left.assignmentMemberOrdinal.has_value())
            assignedUnionMember = *left.assignmentMemberOrdinal;
        storageSyntax = storageSyntax->first();
    }
    const std::optional<ScopeName> storage = storageSyntax != nullptr &&
            storageSyntax->kind() == ast::AstNodeKind::IdentifierExpression
        ? lookupScopeName(scopeOrdinal, storageSyntax->text()) : std::nullopt;
    if (storage.has_value() &&
        (storage->kind == ScopeNameKind::Local || storage->kind == ScopeNameKind::Parameter))
    {
        const std::size_t storageKey = storage->semanticOrdinal * 2U +
            (storage->kind == ScopeNameKind::Parameter ? 1U : 0U);
        std::string storageTypeKey;
        if (storage->kind == ScopeNameKind::Local && storage->semanticOrdinal < locals_.size())
            storageTypeKey = locals_[storage->semanticOrdinal].typeKey;
        else if (storage->kind == ScopeNameKind::Parameter)
        {
            const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
            if (method != nullptr && storage->semanticOrdinal < method->parameters.size())
                storageTypeKey = method->parameters[storage->semanticOrdinal].typeKey;
        }
        if (aggregateRequiresActiveMember(storageTypeKey)) assignedUnionStorage = storageKey;
    }
    if (!assignedUnionStorage.has_value() || assignedUnionMember.has_value() ||
        expression->second() == nullptr ||
        expression->second()->kind() != ast::AstNodeKind::IdentifierExpression) return;
    const std::optional<ScopeName> source =
        lookupScopeName(scopeOrdinal, expression->second()->text());
    if (!source.has_value() ||
        (source->kind != ScopeNameKind::Local && source->kind != ScopeNameKind::Parameter)) return;
    const std::size_t sourceKey = source->semanticOrdinal * 2U +
        (source->kind == ScopeNameKind::Parameter ? 1U : 0U);
    const auto active = activeUnionMembers_.find(sourceKey);
    if (active != activeUnionMembers_.end()) assignedUnionMember = active->second;
}

void ProjectMethodBodyModel::classifyArrayElementAssignmentTarget(
    bool& arrayElementMoveTarget,
    bool& ownedArrayElementOwnsStorage,
    bool& borrowedArrayElementTarget,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const std::optional<std::size_t>& raiiTargetLocal,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (raiiTargetLocal.has_value() || expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IndexExpression ||
        !left.valid || !left.assignable) return;
    ExpressionResult receiver;
    if (expression->first()->first() != nullptr)
    {
        receiver = analyzeExpression(
            expression->first()->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    const std::string& receiverTypeKey = receiver.typeKey;
    arrayElementMoveTarget = ProjectTypeConversion::isNativeArray(receiverTypeKey) ||
        ProjectMemberModel::fixedArrayElementTypeKey(receiverTypeKey).has_value() ||
        ProjectTypeConversion::isPointer(receiverTypeKey);
    bool receiverOwnsStorage = receiver.ownership == sema::OwnershipKind::Owned;
    if (!receiverOwnsStorage && receiver.assignmentMemberOrdinal.has_value() &&
        memberModel_ != nullptr &&
        *receiver.assignmentMemberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& receiverMember =
            memberModel_->members()[*receiver.assignmentMemberOrdinal];
        receiverOwnsStorage = receiverMember.kind == ClassMemberKind::Field &&
            !receiverMember.isStatic &&
            receiverMember.declaredOwnership == sema::OwnershipKind::Owned;
    }
    const bool elementOwnsSeparateStorage =
        isReferenceTypeKey(left.typeKey) || left.typeKey == "builtin:string";
    ownedArrayElementOwnsStorage =
        arrayElementMoveTarget && receiverOwnsStorage && elementOwnsSeparateStorage;
    borrowedArrayElementTarget = arrayElementMoveTarget &&
        receiver.ownership == sema::OwnershipKind::Borrowed;
}

void ProjectMethodBodyModel::classifyOwnedFieldAssignmentTarget(
    bool& ownedFieldMoveTarget,
    bool& genericFieldMoveTarget,
    bool& ownedFieldOwnsStorage,
    std::string& ownedFieldName,
    const ExpressionResult& left,
    const std::optional<std::size_t>& raiiTargetLocal,
    bool arrayElementMoveTarget,
    std::size_t methodMemberOrdinal) const
{
    if (raiiTargetLocal.has_value() || arrayElementMoveTarget || !left.valid || !left.assignable ||
        !left.assignmentMemberOrdinal.has_value() || memberModel_ == nullptr ||
        *left.assignmentMemberOrdinal >= memberModel_->members().size()) return;
    const ClassMemberSymbol& targetMember =
        memberModel_->members()[*left.assignmentMemberOrdinal];
    ownedFieldMoveTarget = targetMember.kind == ClassMemberKind::Field &&
        !targetMember.isStatic &&
        (targetMember.declaredOwnership == sema::OwnershipKind::Owned ||
         memberModel_->typeIsMoveOnly(targetMember.declaredTypeKey) ||
         isGenericValueTypeKey(targetMember.declaredTypeKey));
    genericFieldMoveTarget = targetMember.kind == ClassMemberKind::Field &&
        !targetMember.isStatic && callableBelongsToGenericTypeSpecialization(methodMemberOrdinal);
    ownedFieldOwnsStorage = ownedFieldMoveTarget &&
        (isReferenceTypeKey(targetMember.declaredTypeKey) ||
         targetMember.declaredTypeKey == "builtin:string");
    if (ownedFieldMoveTarget) ownedFieldName = targetMember.name;
}

std::optional<std::size_t> ProjectMethodBodyModel::assignmentGetterBindingIndex(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal) const
{
    const MethodCallBinding* getterCandidate = callBinding(expression->first(), methodMemberOrdinal);
    if (getterCandidate == nullptr ||
        getterCandidate->resolution != MethodCallResolutionKind::Resolved ||
        !getterCandidate->selectedMemberOrdinal.has_value()) return std::nullopt;
    const std::size_t selected = *getterCandidate->selectedMemberOrdinal;
    if (memberModel_ == nullptr || selected >= memberModel_->members().size()) return std::nullopt;
    const ClassMemberSymbol& member = memberModel_->members()[selected];
    if ((!member.isPropertyAccessor && !member.isIndexerAccessor) || !member.isGetter)
        return std::nullopt;
    return static_cast<std::size_t>(getterCandidate - calls_.data());
}

bool ProjectMethodBodyModel::validateEventAssignment(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (left.readonlyReceiver)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "readonly receiver cannot add or remove an event handler");
        return false;
    }
    if (expression->tokenKind() != TokenKind::PlusEqual &&
        expression->tokenKind() != TokenKind::MinusEqual)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "Stage 42 events support only += and -= subscription operations");
        return false;
    }
    return memberModel_ != nullptr &&
        *left.eventMemberOrdinal < memberModel_->members().size();
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeEventHandler(
    const ast::AstNode* expression,
    const ClassMemberSymbol& event,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult right;
    if (expression->second()->kind() == ast::AstNodeKind::LambdaExpression)
    {
        right = bindLambdaToDelegate(
            expression->second(), event.declaredTypeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    else
    {
        right = analyzeExpression(
            expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics, true);
    }
    if (right.methodGroup)
    {
        right = bindMethodGroupToDelegate(
            expression->second(), right, event.declaredTypeKey,
            methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    return right;
}

void ProjectMethodBodyModel::recordEventAssignmentBinding(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ClassMemberSymbol& event,
    std::size_t backingMemberOrdinal,
    const ProjectImplicitConversion& conversion,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    MethodEventBinding binding;
    binding.kind = expression->tokenKind() == TokenKind::PlusEqual
        ? MethodEventOperationKind::Add : MethodEventOperationKind::Remove;
    binding.syntax = expression;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.eventMemberOrdinal = *left.eventMemberOrdinal;
    binding.backingMemberOrdinal = backingMemberOrdinal;
    binding.delegateTypeKey = event.declaredTypeKey;
    binding.delegateTypeOrdinal = delegateTypeOrdinalForTypeKey(event.declaredTypeKey).value_or(0);
    binding.receiverSyntax = left.receiverSyntax;
    binding.receiverTypeKey = left.receiverTypeKey;
    binding.receiverTypeOrdinal = left.receiverTypeOrdinal;
    binding.staticEvent = event.isStatic;
    binding.resultTypeKey = "builtin:void";
    binding.argumentConversions.push_back(conversion);
    // NS304-T09-F08: un event con accessors custom invoca add_/remove_ en vez de la storage.
    const std::string accessorName =
        (binding.kind == MethodEventOperationKind::Add ? "add_" : "remove_") + event.name;
    for (std::size_t ordinal = 0; ordinal < memberModel_->members().size(); ++ordinal)
    {
        const ClassMemberSymbol& candidate = memberModel_->members()[ordinal];
        if (candidate.name == accessorName && candidate.kind == ClassMemberKind::Method)
        {
            binding.customAccessorMemberOrdinal = ordinal;
            break;
        }
    }
    eventBindings_.push_back(std::move(binding));
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeEventAssignment(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!validateEventAssignment(expression, left, diagnostics)) return {};
    const ClassMemberSymbol& event = memberModel_->members()[*left.eventMemberOrdinal];
    const std::optional<std::size_t> backing = eventBackingMember(*left.eventMemberOrdinal);
    if (!backing.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "Stage 42 event is missing its compiler-generated backing storage");
        return {};
    }
    const ExpressionResult right = analyzeEventHandler(
        expression, event, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (!right.valid) return {};
    if (right.typeKey == "builtin:null")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->second()->range(),
            "Stage 42 event subscription/removal requires a non-null delegate value");
        return {};
    }
    const ProjectImplicitConversion conversion = classifyImplicit(
        right.typeKey, event.declaredTypeKey, right.integerConstant);
    if (!conversion.exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->second()->range(),
            "event handler type '" + right.typeKey +
                "' is not compatible with event delegate type '" + event.declaredTypeKey + "'");
        return {};
    }
    recordEventAssignmentBinding(
        expression, left, event, *backing, conversion, methodMemberOrdinal, scopeOrdinal);
    ExpressionResult result;
    result.typeKey = "builtin:void";
    result.valid = true;
    return result;
}

bool ProjectMethodBodyModel::rejectUnavailableAssignmentOwner(
    const ast::AstNode* expression,
    const std::optional<std::size_t>& raiiTargetLocal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (raiiTargetLocal.has_value() && locals_[*raiiTargetLocal].isPinned)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "cannot replace pinned owner '" + locals_[*raiiTargetLocal].name +
                "'; it must remain at a stable address until lexical destruction");
        return true;
    }
    if (raiiTargetLocal.has_value() &&
        hasActiveBorrowOfOwnerLocal(*raiiTargetLocal, scopeOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "cannot overwrite owner '" + locals_[*raiiTargetLocal].name +
                "' while a borrowed local is alive");
        return true;
    }
    return false;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeAssignmentRight(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const std::optional<std::size_t>& raiiTargetLocal,
    const std::optional<std::size_t>& moveOnlyTargetLocal,
    const std::optional<std::size_t>& moveOnlyTargetParameter,
    bool arrayElementMoveTarget,
    bool ownedFieldMoveTarget,
    bool genericFieldMoveTarget,
    bool ownedSetterMoveTarget,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const bool delegateTarget = isDelegateTypeKey(left.typeKey);
    // NS304-T09-F07: expone el tipo objetivo al `new()` target-typed del lado derecho.
    const std::string previousTargetTypeKey = activeTargetTypeKey_;
    activeTargetTypeKey_ = left.typeKey;
    ExpressionResult right;
    if (delegateTarget && expression->second()->kind() == ast::AstNodeKind::LambdaExpression)
    {
        right = bindLambdaToDelegate(
            expression->second(), left.typeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    else
    {
        right = analyzeExpression(
            expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics,
            delegateTarget, false,
            raiiTargetLocal.has_value() || moveOnlyTargetLocal.has_value() ||
                moveOnlyTargetParameter.has_value() || arrayElementMoveTarget ||
                ownedFieldMoveTarget || genericFieldMoveTarget || ownedSetterMoveTarget);
    }
    activeTargetTypeKey_ = previousTargetTypeKey;
    if (right.methodGroup && delegateTarget)
    {
        right = bindMethodGroupToDelegate(
            expression->second(), right, left.typeKey,
            methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    return right;
}

bool ProjectMethodBodyModel::rejectMoveOnlyAssignmentCopy(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (expression->tokenKind() != TokenKind::Equal ||
        !memberModel_->typeIsMoveOnly(left.typeKey) || !right.addressable ||
        expression->second()->kind() == ast::AstNodeKind::MoveExpression) return false;
    diagnostics.report(
        diagnostics::DiagnosticId::NS3020InvalidValueCategory,
        diagnostics::DiagnosticSeverity::Error,
        expression->second()->range(),
        "move-only value of type '" + left.typeKey +
            "' cannot be copied by assignment; use move(source)");
    return true;
}

void ProjectMethodBodyModel::classifyNamedAssignmentTarget(
    std::optional<std::size_t>& assignmentTargetLocal,
    std::optional<std::size_t>& assignmentTargetParameter,
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const
{
    if (expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IdentifierExpression) return;
    const std::optional<ScopeName> target =
        lookupScopeName(scopeOrdinal, expression->first()->text());
    if (target.has_value() && target->kind == ScopeNameKind::Local &&
        target->semanticOrdinal < locals_.size())
    {
        assignmentTargetLocal = target->semanticOrdinal;
    }
    else if (target.has_value() && target->kind == ScopeNameKind::Parameter)
    {
        assignmentTargetParameter = target->semanticOrdinal;
    }
}

bool ProjectMethodBodyModel::validateAssignmentTargetStorage(
    const ast::AstNode* expression,
    const ExpressionResult& right,
    const std::optional<std::size_t>& assignmentTargetLocal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (assignmentTargetLocal.has_value() &&
        locals_[*assignmentTargetLocal].isVariableLengthArray)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "a variable-length array is storage, not a reassignable pointer");
        return false;
    }
    if (assignmentTargetLocal.has_value() &&
        ProjectTypeConversion::isPointer(locals_[*assignmentTargetLocal].typeKey))
    {
        const MethodLocalSymbol& target = locals_[*assignmentTargetLocal];
        if (!language::pointerContractImplicitlyAssignable(
                pointerContract(
                    right.pointerNullable, right.pointerPointeeReadonly,
                    right.pointerHasProvenance, right.pointerKnownAlignment,
                    right.pointerAddressSpace),
                pointerContract(
                    target.pointerNullable, target.pointerPointeeReadonly,
                    target.pointerHasProvenance, target.pointerKnownAlignment,
                    target.pointerAddressSpace)))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->second()->range(),
                "assignment does not satisfy the destination pointer nullability, mutability, "
                "provenance, alignment, or address-space contract");
            return false;
        }
        // Qualifier/provenance joins are MAY properties. Once any reachable assignment can make
        // a pointer readonly or one-past, later dereference must remain conservative.
        locals_[*assignmentTargetLocal].onePastEndPointer =
            locals_[*assignmentTargetLocal].onePastEndPointer || right.onePastEndPointer;
    }
    const std::optional<std::size_t> assignedVlaRoot =
        variableArrayRootLocalOfExpression(expression->second(), scopeOrdinal);
    if (assignedVlaRoot.has_value() && !assignmentTargetLocal.has_value())
    {
        const std::string name = *assignedVlaRoot < locals_.size()
            ? locals_[*assignedVlaRoot].name : std::string("<variable-length-array>");
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->second()->range(),
            "pointer into variable-length array '" + name +
                "' cannot be stored outside a local alias because the stack frame owns it");
        return false;
    }
    if (assignedVlaRoot.has_value())
        locals_[*assignmentTargetLocal].variableLengthArrayRootLocalOrdinal = assignedVlaRoot;
    else if (assignmentTargetLocal.has_value() &&
        locals_[*assignmentTargetLocal].typeKey.ends_with("*"))
        locals_[*assignmentTargetLocal].variableLengthArrayRootLocalOrdinal.reset();
    return true;
}

void ProjectMethodBodyModel::recordViewAssignmentRoot(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal)
{
    const ast::AstNode* viewAssignmentRoot = expression->first();
    while (viewAssignmentRoot != nullptr &&
        viewAssignmentRoot->kind() == ast::AstNodeKind::IndexExpression)
    {
        viewAssignmentRoot = viewAssignmentRoot->first();
    }
    if (viewAssignmentRoot == nullptr ||
        viewAssignmentRoot->kind() != ast::AstNodeKind::IdentifierExpression) return;
    const std::optional<ScopeName> viewTarget =
        lookupScopeName(scopeOrdinal, viewAssignmentRoot->text());
    if (!viewTarget.has_value() || viewTarget->kind != ScopeNameKind::Local ||
        viewTarget->semanticOrdinal >= locals_.size() ||
        !typeKeyIsViewLike(locals_[viewTarget->semanticOrdinal].typeKey) ||
        !locals_[viewTarget->semanticOrdinal].viewRootLocalName.empty()) return;
    const std::optional<std::string> root =
        viewRootLocalOfExpression(expression->second(), scopeOrdinal);
    if (root.has_value()) locals_[viewTarget->semanticOrdinal].viewRootLocalName = *root;
}

void ProjectMethodBodyModel::appendUniqueOrdinal(
    std::vector<std::size_t>& values,
    std::size_t value)
{
    if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

bool ProjectMethodBodyModel::classifyBorrowedAssignmentDestination(
    bool& borrowedReassignment,
    MethodLocalBorrowBinding*& targetLocalBorrow,
    const ClassMemberSymbol*& enclosingMethod,
    const ast::AstNode* expression,
    const std::optional<std::size_t>& assignmentTargetLocal,
    const std::optional<std::size_t>& assignmentTargetParameter,
    std::size_t methodMemberOrdinal)
{
    enclosingMethod = methodSymbol(methodMemberOrdinal);
    if (expression->tokenKind() == TokenKind::Equal && assignmentTargetLocal.has_value() &&
        *assignmentTargetLocal < locals_.size())
    {
        MethodLocalSymbol& targetLocal = locals_[*assignmentTargetLocal];
        if (!targetLocal.isRaii && targetLocal.ownership == sema::OwnershipKind::Borrowed &&
            targetLocal.localBorrowBindingOrdinal.has_value() &&
            *targetLocal.localBorrowBindingOrdinal < localBorrowBindings_.size())
        {
            borrowedReassignment = true;
            targetLocalBorrow = &localBorrowBindings_[*targetLocal.localBorrowBindingOrdinal];
        }
    }
    const bool borrowedParameterReassignment =
        expression->tokenKind() == TokenKind::Equal && assignmentTargetParameter.has_value() &&
        enclosingMethod != nullptr &&
        *assignmentTargetParameter < enclosingMethod->parameters.size() &&
        enclosingMethod->parameters[*assignmentTargetParameter].ownership ==
            sema::OwnershipKind::Borrowed;
    borrowedReassignment = borrowedReassignment || borrowedParameterReassignment;
    return borrowedReassignment;
}

void ProjectMethodBodyModel::collectBorrowedAssignmentOwners(
    std::vector<std::size_t>& ownerLocals,
    std::vector<std::size_t>& ownerParameters,
    const ast::AstNode* expression,
    const ClassMemberSymbol* enclosingMethod,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal) const
{
    if (expression->second() == nullptr ||
        expression->second()->kind() != ast::AstNodeKind::IdentifierExpression) return;
    const std::optional<ScopeName> source =
        lookupScopeName(scopeOrdinal, expression->second()->text());
    if (source.has_value() && source->kind == ScopeNameKind::Local &&
        source->semanticOrdinal < locals_.size())
    {
        const MethodLocalSymbol& sourceLocal = locals_[source->semanticOrdinal];
        if (sourceLocal.isRaii) appendUniqueOrdinal(ownerLocals, source->semanticOrdinal);
        else if (sourceLocal.ownership == sema::OwnershipKind::Borrowed &&
            sourceLocal.localBorrowBindingOrdinal.has_value() &&
            *sourceLocal.localBorrowBindingOrdinal < localBorrowBindings_.size())
        {
            const MethodLocalBorrowBinding& sourceBorrow =
                localBorrowBindings_[*sourceLocal.localBorrowBindingOrdinal];
            if (sourceBorrow.sourceOwnerLocalOrdinal.has_value())
                appendUniqueOrdinal(ownerLocals, *sourceBorrow.sourceOwnerLocalOrdinal);
            if (sourceBorrow.sourceOwnerParameterOrdinal.has_value())
                appendUniqueOrdinal(ownerParameters, *sourceBorrow.sourceOwnerParameterOrdinal);
            for (const std::size_t ordinal : sourceBorrow.possibleSourceOwnerLocalOrdinals)
                appendUniqueOrdinal(ownerLocals, ordinal);
            for (const std::size_t ordinal : sourceBorrow.possibleSourceOwnerParameterOrdinals)
                appendUniqueOrdinal(ownerParameters, ordinal);
        }
        return;
    }
    if (!source.has_value() || source->kind != ScopeNameKind::Parameter ||
        enclosingMethod == nullptr || source->semanticOrdinal >= enclosingMethod->parameters.size()) return;
    const MethodParameterSymbol& sourceParameter = enclosingMethod->parameters[source->semanticOrdinal];
    if (sourceParameter.ownership != sema::OwnershipKind::Owned &&
        sourceParameter.ownership != sema::OwnershipKind::Borrowed) return;
    appendUniqueOrdinal(ownerParameters, source->semanticOrdinal);
    for (const MethodBorrowedParameterReassignmentBinding& prior :
         borrowedParameterReassignmentBindings_)
    {
        if (prior.methodMemberOrdinal != methodMemberOrdinal ||
            prior.parameterOrdinal != source->semanticOrdinal) continue;
        for (const std::size_t ordinal : prior.possibleSourceOwnerLocalOrdinals)
            appendUniqueOrdinal(ownerLocals, ordinal);
        for (const std::size_t ordinal : prior.possibleSourceOwnerParameterOrdinals)
            appendUniqueOrdinal(ownerParameters, ordinal);
    }
}

bool ProjectMethodBodyModel::validateBorrowedAssignmentOwnerScopes(
    const std::vector<std::size_t>& ownerLocals,
    const ast::AstNode* expression,
    const MethodLocalBorrowBinding* targetLocalBorrow,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    std::size_t destinationScope = scopeOrdinal;
    if (targetLocalBorrow != nullptr)
        destinationScope = targetLocalBorrow->creationScopeOrdinal;
    else
        while (destinationScope < scopes_.size() &&
               scopes_[destinationScope].parentScopeOrdinal.has_value())
            destinationScope = *scopes_[destinationScope].parentScopeOrdinal;
    for (const std::size_t ownerLocal : ownerLocals)
    {
        if (ownerLocal < locals_.size() &&
            scopeContains(locals_[ownerLocal].scopeOrdinal, destinationScope)) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->second()->range(),
            "borrowed reassignment would let the destination outlive local owner '" +
                (ownerLocal < locals_.size() ? locals_[ownerLocal].name
                                             : std::string("<local-owner>")) + "'");
        return false;
    }
    return true;
}

void ProjectMethodBodyModel::recordBorrowedAssignmentOwners(
    const std::vector<std::size_t>& ownerLocals,
    const std::vector<std::size_t>& ownerParameters,
    const ast::AstNode* expression,
    MethodLocalBorrowBinding* targetLocalBorrow,
    const std::optional<std::size_t>& assignmentTargetParameter,
    std::size_t methodMemberOrdinal)
{
    if (targetLocalBorrow != nullptr)
    {
        for (const std::size_t ordinal : ownerLocals)
            appendUniqueOrdinal(targetLocalBorrow->possibleSourceOwnerLocalOrdinals, ordinal);
        for (const std::size_t ordinal : ownerParameters)
            appendUniqueOrdinal(targetLocalBorrow->possibleSourceOwnerParameterOrdinals, ordinal);
        return;
    }
    MethodBorrowedParameterReassignmentBinding* targetBinding = nullptr;
    for (MethodBorrowedParameterReassignmentBinding& candidate :
         borrowedParameterReassignmentBindings_)
    {
        if (candidate.methodMemberOrdinal == methodMemberOrdinal &&
            candidate.parameterOrdinal == *assignmentTargetParameter)
        {
            targetBinding = &candidate;
            break;
        }
    }
    if (targetBinding == nullptr)
    {
        MethodBorrowedParameterReassignmentBinding binding;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.parameterOrdinal = *assignmentTargetParameter;
        borrowedParameterReassignmentBindings_.push_back(std::move(binding));
        targetBinding = &borrowedParameterReassignmentBindings_.back();
    }
    for (const std::size_t ordinal : ownerLocals)
        appendUniqueOrdinal(targetBinding->possibleSourceOwnerLocalOrdinals, ordinal);
    for (const std::size_t ordinal : ownerParameters)
        appendUniqueOrdinal(targetBinding->possibleSourceOwnerParameterOrdinals, ordinal);
}

bool ProjectMethodBodyModel::analyzeBorrowedReassignment(
    bool& borrowedReassignment,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    const std::optional<std::size_t>& assignmentTargetLocal,
    const std::optional<std::size_t>& assignmentTargetParameter,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    MethodLocalBorrowBinding* targetLocalBorrow = nullptr;
    const ClassMemberSymbol* enclosingMethod = nullptr;
    if (!classifyBorrowedAssignmentDestination(
            borrowedReassignment, targetLocalBorrow, enclosingMethod, expression,
            assignmentTargetLocal, assignmentTargetParameter, methodMemberOrdinal)) return true;
    const ProjectImplicitConversion conversion =
        classifyImplicit(right.typeKey, left.typeKey, right.integerConstant);
    if (!conversion.exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot rebind borrowed destination from '" + right.typeKey + "' to '" +
                left.typeKey + "'");
        return false;
    }
    std::vector<std::size_t> ownerLocals;
    std::vector<std::size_t> ownerParameters;
    collectBorrowedAssignmentOwners(
        ownerLocals, ownerParameters, expression, enclosingMethod,
        methodMemberOrdinal, scopeOrdinal);
    if (ownerLocals.empty() && ownerParameters.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->second()->range(),
            "borrowed reassignment requires a named RAII owner, owned/borrowed parameter, "
            "or borrowed local with tracked provenance");
        return false;
    }
    if (!validateBorrowedAssignmentOwnerScopes(
            ownerLocals, expression, targetLocalBorrow, scopeOrdinal, diagnostics)) return false;
    recordBorrowedAssignmentOwners(
        ownerLocals, ownerParameters, expression, targetLocalBorrow,
        assignmentTargetParameter, methodMemberOrdinal);
    return true;
}

bool ProjectMethodBodyModel::rejectBorrowedAssignmentEscape(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    bool borrowedReassignment,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const bool assignmentTargetOwnable =
        isReferenceTypeKey(left.typeKey) || left.typeKey == "builtin:string";
    if (borrowedReassignment || right.ownership != sema::OwnershipKind::Borrowed ||
        !assignmentTargetOwnable) return false;
    if (left.assignmentMemberOrdinal.has_value() && memberModel_ != nullptr &&
        *left.assignmentMemberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& field = memberModel_->members()[*left.assignmentMemberOrdinal];
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->second()->range(),
            "borrowed value cannot be stored in field '" + field.name +
                "' because the field may outlive its source owner");
        return true;
    }
    if (expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IdentifierExpression) return false;
    const std::optional<ScopeName> target =
        lookupScopeName(scopeOrdinal, expression->first()->text());
    if (!target.has_value()) return false;
    if (target->kind == ScopeNameKind::Local && target->semanticOrdinal < locals_.size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->second()->range(),
            "borrowed value cannot be stored in unqualified local '" +
                locals_[target->semanticOrdinal].name +
                "'; declare a borrowed local at initialization so provenance remains checked");
        return true;
    }
    if (target->kind != ScopeNameKind::Parameter) return false;
    const ClassMemberSymbol* owner = methodSymbol(methodMemberOrdinal);
    const std::string targetName = owner != nullptr &&
        target->semanticOrdinal < owner->parameters.size()
        ? owner->parameters[target->semanticOrdinal].name : expression->first()->text();
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->second()->range(),
        "borrowed value cannot be assigned to parameter '" + targetName +
            "' because replacement lifetime provenance is not tracked in Stage 50");
    return true;
}

bool ProjectMethodBodyModel::rejectOwnerCopyAssignment(
    const ast::AstNode* expression,
    bool borrowedReassignment,
    bool borrowedArrayElementTarget,
    const std::optional<std::size_t>& raiiTargetLocal,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (borrowedReassignment || borrowedArrayElementTarget || raiiTargetLocal.has_value() ||
        expression->second() == nullptr ||
        expression->second()->kind() != ast::AstNodeKind::IdentifierExpression) return false;
    const std::optional<ScopeName> sourceName =
        lookupScopeName(scopeOrdinal, expression->second()->text());
    if (sourceName.has_value() && sourceName->kind == ScopeNameKind::Local &&
        sourceName->semanticOrdinal < locals_.size() && locals_[sourceName->semanticOrdinal].isRaii)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->second()->range(),
            "RAII owner '" + locals_[sourceName->semanticOrdinal].name +
                "' cannot be copied by assignment; Stage 49 requires ownership transfer to "
                "another RAII local with move(...)");
        return true;
    }
    if (!sourceName.has_value() || sourceName->kind != ScopeNameKind::Parameter) return false;
    const ClassMemberSymbol* enclosing = methodSymbol(methodMemberOrdinal);
    if (enclosing == nullptr || sourceName->semanticOrdinal >= enclosing->parameters.size()) return false;
    const MethodParameterSymbol& source = enclosing->parameters[sourceName->semanticOrdinal];
    const bool sourceOwnsStorage =
        isReferenceTypeKey(source.typeKey) || source.typeKey == "builtin:string";
    if (source.ownership != sema::OwnershipKind::Owned || !sourceOwnsStorage) return false;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->second()->range(),
        "cannot copy owned value '" + source.name +
            "' by assignment; use move(" + source.name + ") to transfer ownership");
    return true;
}

bool ProjectMethodBodyModel::validateOwningStorageAssignment(
    const ast::AstNode* expression,
    const ExpressionResult& right,
    bool ownedFieldOwnsStorage,
    bool ownedArrayElementOwnsStorage,
    const std::string& ownedFieldName,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!ownedFieldOwnsStorage && !ownedArrayElementOwnsStorage) return true;
    const bool freshObject = expression->second() != nullptr &&
        expression->second()->kind() == ast::AstNodeKind::ObjectCreationExpression;
    const bool nullValue = expression->second() != nullptr &&
        expression->second()->kind() == ast::AstNodeKind::NullLiteralExpression;
    const bool ownershipTransferred =
        right.ownership == sema::OwnershipKind::Owned && right.ownershipTransfer;
    if (freshObject || nullValue || ownershipTransferred) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->second()->range(),
        ownedFieldOwnsStorage
            ? "assignment to owned field '" + ownedFieldName +
                  "' requires an ownership-transfer expression such as move(owner), new T(...), "
                  "null, or an owned-returning call"
            : "assignment to an element of an owned array requires an ownership-transfer "
                  "expression such as move(owner), new T(...), null, or an owned-returning call");
    return false;
}

bool ProjectMethodBodyModel::validateRaiiAssignmentTarget(
    const ast::AstNode* expression,
    const std::optional<std::size_t>& raiiTargetLocal,
    bool raiiStringAppend,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!raiiTargetLocal.has_value()) return true;
    if (!raiiStringAppend && (expression->tokenKind() != TokenKind::Equal ||
        expression->second() == nullptr ||
        expression->second()->kind() != ast::AstNodeKind::MoveExpression))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "RAII locals are move-only; assignment requires target = move(source)");
        return false;
    }
    const MethodMoveBinding* move = nullptr;
    for (auto iterator = moveBindings_.rbegin(); iterator != moveBindings_.rend(); ++iterator)
    {
        if (iterator->syntax != expression->second()) continue;
        move = &*iterator;
        break;
    }
    if (move == nullptr || move->sourceLocalOrdinal != *raiiTargetLocal) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "self move-assignment of a RAII local is not allowed");
    return false;
}

bool ProjectMethodBodyModel::validateMoveOnlyAssignmentTarget(
    const ast::AstNode* expression,
    const std::optional<std::size_t>& moveOnlyTargetLocal,
    const std::optional<std::size_t>& moveOnlyTargetParameter,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!moveOnlyTargetLocal.has_value() && !moveOnlyTargetParameter.has_value()) return true;
    if (expression->tokenKind() != TokenKind::Equal)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "move-only values support replacement with '=' but not compound assignment");
        return false;
    }
    const MethodMoveBinding* move = nullptr;
    for (auto iterator = moveBindings_.rbegin(); iterator != moveBindings_.rend(); ++iterator)
    {
        if (iterator->syntax != expression->second()) continue;
        move = &*iterator;
        break;
    }
    const bool selfMove =
        (moveOnlyTargetLocal.has_value() && move != nullptr &&
         move->sourceLocalOrdinal == *moveOnlyTargetLocal) ||
        (moveOnlyTargetParameter.has_value() && move != nullptr &&
         move->sourceParameterOrdinal == *moveOnlyTargetParameter);
    if (!selfMove) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "self move-assignment of a move-only value is not allowed");
    return false;
}

const MethodCallBinding* ProjectMethodBodyModel::assignmentSetterGetter(
    const std::optional<std::size_t>& getterBindingIndex) const
{
    const MethodCallBinding* getterBinding = getterBindingIndex.has_value()
        ? &calls_[*getterBindingIndex] : nullptr;
    if (getterBinding == nullptr || memberModel_ == nullptr ||
        !getterBinding->selectedMemberOrdinal.has_value() ||
        *getterBinding->selectedMemberOrdinal >= memberModel_->members().size()) return getterBinding;
    if (memberModel_->members()[*getterBinding->selectedMemberOrdinal].isReferenceReturn)
    {
        return nullptr;
    }
    return getterBinding;
}

bool ProjectMethodBodyModel::validatePropertySetterAssignment(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (right.ownership == sema::OwnershipKind::Borrowed)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->second()->range(),
            "borrowed value cannot be stored through a property/indexer setter because "
            "retention is not proven");
        return false;
    }
    if (!left.readonlyReceiver) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidAssignmentTarget,
        diagnostics::DiagnosticSeverity::Error,
        expression->first()->range(),
        "readonly receiver cannot invoke a property/indexer setter");
    return false;
}

bool ProjectMethodBodyModel::collectPropertySetterCandidates(
    std::vector<std::size_t>& setters,
    std::string& setterName,
    const ast::AstNode* expression,
    const MethodCallBinding& getterBinding,
    const ClassMemberSymbol& getter,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
    const std::size_t accessingTypeOrdinal = caller != nullptr
        ? caller->declaringTypeOrdinal : getter.declaringTypeOrdinal;
    const std::size_t receiverTypeOrdinal =
        getterBinding.receiverTypeOrdinal.value_or(getter.declaringTypeOrdinal);
    const std::size_t protectedReceiverType = getterBinding.baseReceiver
        ? accessingTypeOrdinal : receiverTypeOrdinal;
    setterName = getter.isPropertyAccessor ? "set_" + getter.associatedName : "set_Item";
    setters = stage38AccessorCandidates(
        receiverTypeOrdinal, setterName, getter.isPropertyAccessor, false, getter.isStatic,
        accessingTypeOrdinal, protectedReceiverType);
    if (setters.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "Stage 38 property/indexer has no accessible setter");
        return false;
    }
    for (const std::size_t setterOrdinal : setters)
    {
        const ClassMemberSymbol* setterSymbol = methodSymbol(setterOrdinal);
        if (setterSymbol == nullptr || !setterSymbol->isInit) continue;
        if (caller == nullptr || caller->kind != ClassMemberKind::Constructor)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                diagnostics::DiagnosticSeverity::Error,
                expression->first()->range(),
                "init accessor can only be assigned during construction (in a constructor)");
            return false;
        }
        break;
    }
    return true;
}

void ProjectMethodBodyModel::appendRecordedSetterExpression(
    std::vector<ExpressionResult>& setterArguments,
    const ast::AstNode* syntax,
    std::size_t methodMemberOrdinal) const
{
    ExpressionResult value;
    const MethodExpressionTypeBinding* binding =
        expressionTypeBinding(syntax, methodMemberOrdinal);
    if (binding != nullptr)
    {
        value.typeKey = binding->typeKey;
        value.integerConstant = binding->integerConstant;
        value.valid = !isErrorType(value.typeKey);
    }
    setterArguments.push_back(std::move(value));
}

void ProjectMethodBodyModel::collectPropertySetterArguments(
    std::vector<ExpressionResult>& setterArguments,
    const ast::AstNode* expression,
    const ClassMemberSymbol& getter,
    std::size_t methodMemberOrdinal) const
{
    if (!getter.isIndexerAccessor ||
        expression->first()->kind() != ast::AstNodeKind::IndexExpression) return;
    const ast::AstNode* indexExpression = expression->first();
    if (!indexExpression->children().empty())
    {
        for (const ast::AstNode* index : indexExpression->children())
            appendRecordedSetterExpression(setterArguments, index, methodMemberOrdinal);
    }
    else if (indexExpression->second() != nullptr)
    {
        appendRecordedSetterExpression(
            setterArguments, indexExpression->second(), methodMemberOrdinal);
    }
}

bool ProjectMethodBodyModel::validatePropertySetterReevaluation(
    const ast::AstNode* expression,
    const MethodCallBinding& getterBinding,
    const ClassMemberSymbol& getter,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const ast::AstNode* offender = nullptr;
    bool reevaluable =
        isAccessorDestinationReevaluable(getterBinding.receiverSyntax, scopeOrdinal, offender);
    if (reevaluable && getter.isIndexerAccessor &&
        expression->first()->kind() == ast::AstNodeKind::IndexExpression)
    {
        const ast::AstNode* indexExpression = expression->first();
        if (!indexExpression->children().empty())
        {
            for (const ast::AstNode* index : indexExpression->children())
            {
                reevaluable = reevaluable &&
                    isAccessorDestinationReevaluable(index, scopeOrdinal, offender);
            }
        }
        else
        {
            reevaluable = isAccessorDestinationReevaluable(
                indexExpression->second(), scopeOrdinal, offender);
        }
    }
    if (reevaluable) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidAssignmentTarget,
        diagnostics::DiagnosticSeverity::Error,
        offender != nullptr ? offender->range() : expression->first()->range(),
        "compound assignment to a property/indexer evaluates its receiver and index "
        "arguments once for the getter and once for the setter, so each has to be a "
        "local, a parameter, `this` or a constant expression; assign this one to a "
        "local first and use that local");
    return false;
}

bool ProjectMethodBodyModel::preparePropertySetterValue(
    ExpressionResult& setterValue,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    const MethodCallBinding& getterBinding,
    const ClassMemberSymbol& getter,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (expression->tokenKind() == TokenKind::Equal) return true;
    const TokenKind baseOperator = lexer::compoundAssignmentBaseOperator(expression->tokenKind());
    if (baseOperator == TokenKind::Invalid)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "unsupported compound assignment operator on a property/indexer");
        return false;
    }
    if (!validatePropertySetterReevaluation(
            expression, getterBinding, getter, scopeOrdinal, diagnostics)) return false;
    const bool sameEnumBitwise = isEnumTypeKey(left.typeKey) && left.typeKey == right.typeKey &&
        (baseOperator == TokenKind::Amp || baseOperator == TokenKind::Pipe ||
         baseOperator == TokenKind::Caret);
    const bool shiftAssignment = isShift(baseOperator) &&
        ProjectTypeConversion::isIntegral(left.typeKey) &&
        ProjectTypeConversion::isIntegral(right.typeKey) &&
        left.typeKey != "builtin:bool" && right.typeKey != "builtin:bool";
    const std::optional<std::string> common = sameEnumBitwise || shiftAssignment
        ? std::optional<std::string>(left.typeKey)
        : ProjectTypeConversion::commonNumericType(
              left.typeKey, right.typeKey, left.integerConstant, right.integerConstant);
    if (!common.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "compound assignment to a property/indexer requires compatible numeric operands");
        return false;
    }
    const ProjectImplicitConversion narrowing =
        classifyImplicit(right.typeKey, left.typeKey, right.integerConstant);
    if (!shiftAssignment && !narrowing.exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->second()->range(),
            "compound assignment cannot implicitly convert its right-hand side from '" +
                right.typeKey + "' to '" + left.typeKey +
                "'; write the read-modify-write with an explicit cast");
        return false;
    }
    ExpressionResult stored;
    stored.typeKey = left.typeKey;
    stored.valid = true;
    setterValue = std::move(stored);
    return true;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzePropertySetterAssignment(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    const MethodCallBinding& getterBinding,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!validatePropertySetterAssignment(expression, left, right, diagnostics)) return {};
    const ClassMemberSymbol& getter =
        memberModel_->members()[*getterBinding.selectedMemberOrdinal];
    std::vector<std::size_t> setters;
    std::string setterName;
    if (!collectPropertySetterCandidates(
            setters, setterName, expression, getterBinding, getter,
            methodMemberOrdinal, diagnostics)) return {};
    std::vector<ExpressionResult> setterArguments;
    collectPropertySetterArguments(
        setterArguments, expression, getter, methodMemberOrdinal);
    ExpressionResult setterValue = right;
    if (!preparePropertySetterValue(
            setterValue, expression, left, right, getterBinding, getter,
            scopeOrdinal, diagnostics)) return {};
    setterArguments.push_back(setterValue);
    ExpressionResult setterResult = resolveSyntheticCall(
        expression, methodMemberOrdinal, scopeOrdinal, setterName, std::move(setters),
        setterArguments, getterBinding.receiverSyntax, getterBinding.receiverTypeKey,
        getterBinding.receiverTypeOrdinal, getterBinding.baseReceiver,
        getterBinding.interfaceCall, diagnostics);
    if (!setterResult.valid && setterResult.typeKey != "builtin:void") return {};
    ExpressionResult assignmentResult = left;
    assignmentResult.assignable = false;
    assignmentResult.valid = true;
    invalidateAssignedStorage(expression->first(), scopeOrdinal);
    return assignmentResult;
}

std::optional<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::analyzeExtensionPropertyAssignment(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (expression->tokenKind() != TokenKind::Equal || namespaceFunctionModel_ == nullptr ||
        expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::MemberExpression) return std::nullopt;
    const NamespaceFunctionReferenceBinding* extension =
        namespaceFunctionModel_->extensionBinding(expression->first());
    if (extension == nullptr || extension->candidateFunctionOrdinals.empty()) return std::nullopt;
    const std::size_t getterOrdinal = extension->candidateFunctionOrdinals.front();
    if (getterOrdinal >= namespaceCallableAdapters().size() ||
        namespaceCallableAdapters()[getterOrdinal].parameters.size() != 1U) return std::nullopt;
    std::string extensionNamespace;
    if (getterOrdinal < namespaceFunctionModel_->functions().size())
        extensionNamespace = namespaceFunctionModel_->functions()[getterOrdinal].namespaceName;
    const std::span<const std::size_t> setterOrdinals = namespaceFunctionModel_->findFunctions(
        extension->projectOrdinal, extensionNamespace, "set_" + extension->writtenName);
    if (setterOrdinals.empty()) return std::nullopt;
    NamespaceFunctionReferenceBinding setterReference;
    setterReference.calleeSyntax = expression->first();
    setterReference.projectOrdinal = extension->projectOrdinal;
    setterReference.fileId = extension->fileId;
    setterReference.lexicalNamespace = extensionNamespace;
    setterReference.writtenName = "set_" + extension->writtenName;
    setterReference.qualified = false;
    setterReference.extension = true;
    setterReference.receiverSyntax = extension->receiverSyntax;
    setterReference.candidateFunctionOrdinals.assign(
        setterOrdinals.begin(), setterOrdinals.end());
    ExpressionResult receiver = analyzeExpression(
        expression->first()->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    receiver.receiverTypeKey = receiver.typeKey;
    if (!receiver.valid) return std::nullopt;
    ExpressionResult setterResult = analyzeNamespaceFunctionCall(
        expression, setterReference, methodMemberOrdinal, scopeOrdinal,
        diagnostics, &receiver, expression->second());
    if (!setterResult.valid && setterResult.typeKey != "builtin:void") return ExpressionResult{};
    ExpressionResult assignmentResult = left;
    assignmentResult.assignable = false;
    assignmentResult.valid = true;
    invalidateAssignedStorage(expression->first(), scopeOrdinal);
    return assignmentResult;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeNullConditionalIndexAssignment(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    bool ownedArrayElementOwnsStorage,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const ProjectImplicitConversion conversion =
        classifyImplicit(right.typeKey, left.typeKey, right.integerConstant);
    if (!conversion.exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot implicitly convert assignment RHS from '" + right.typeKey + "' to '" +
                left.typeKey + "'");
        return {};
    }
    ExpressionResult assignmentResult = right;
    assignmentResult.ownsStorageDestination = ownedArrayElementOwnsStorage;
    assignmentResult.assignable = false;
    assignmentResult.valid = true;
    return assignmentResult;
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::analyzeNullConditionalCompoundAssignment(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    bool ownedFieldOwnsStorage,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const TokenKind compoundBase =
        lexer::compoundAssignmentBaseOperator(expression->tokenKind());
    if (compoundBase == TokenKind::Invalid)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "unsupported compound assignment operator on a null-conditional target");
        return {};
    }
    if (compoundBase == TokenKind::Plus && left.typeKey == "builtin:string" &&
        (right.typeKey == "builtin:string" || right.typeKey == "string-literal"))
    {
        ExpressionResult assignmentResult = left;
        assignmentResult.typeKey = "builtin:string";
        assignmentResult.ownsStorageDestination = ownedFieldOwnsStorage;
        assignmentResult.assignable = false;
        assignmentResult.valid = true;
        return assignmentResult;
    }
    const std::optional<std::string> common = ProjectTypeConversion::commonNumericType(
        left.typeKey, right.typeKey, left.integerConstant, right.integerConstant);
    if (!common.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "null-conditional compound assignment requires compatible numeric operands");
        return {};
    }
    const ProjectImplicitConversion narrowing =
        classifyImplicit(right.typeKey, left.typeKey, right.integerConstant);
    if (!narrowing.exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "null-conditional compound assignment cannot implicitly convert its right-hand "
            "side from '" + right.typeKey + "' to '" + left.typeKey + "'");
        return {};
    }
    ExpressionResult assignmentResult = left;
    assignmentResult.ownsStorageDestination = ownedFieldOwnsStorage;
    assignmentResult.assignable = false;
    assignmentResult.valid = true;
    return assignmentResult;
}

bool ProjectMethodBodyModel::validatePlainAssignmentConversion(
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const ProjectImplicitConversion conversion =
        classifyImplicit(right.typeKey, left.typeKey, right.integerConstant);
    if (!conversion.exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot implicitly convert assignment RHS from '" + right.typeKey + "' to '" +
                left.typeKey + "'");
        return false;
    }
    if (!ProjectTypeConversion::isPointer(left.typeKey) ||
        language::pointerContractImplicitlyAssignable(
            pointerContract(
                right.pointerNullable, right.pointerPointeeReadonly,
                right.pointerHasProvenance, right.pointerKnownAlignment,
                right.pointerAddressSpace),
            pointerContract(
                left.pointerNullable, left.pointerPointeeReadonly,
                left.pointerHasProvenance, left.pointerKnownAlignment,
                left.pointerAddressSpace))) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidConversion,
        diagnostics::DiagnosticSeverity::Error,
        expression->second()->range(),
        "assignment does not satisfy the destination pointer nullability, mutability, provenance, "
        "alignment, or address-space contract");
    return false;
}

void ProjectMethodBodyModel::updatePlainAssignmentState(
    const ast::AstNode* expression,
    const ExpressionResult& right,
    const std::optional<std::size_t>& raiiTargetLocal,
    const std::optional<std::size_t>& assignmentTargetLocal,
    const std::optional<std::size_t>& moveOnlyTargetLocal,
    const std::optional<std::size_t>& moveOnlyTargetParameter,
    const std::optional<std::size_t>& assignedUnionStorage,
    const std::optional<std::size_t>& assignedUnionMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    if (raiiTargetLocal.has_value())
    {
        movedRaiiLocals_.erase(*raiiTargetLocal);
        maybeMovedRaiiLocals_.erase(*raiiTargetLocal);
    }
    if (assignmentTargetLocal.has_value())
    {
        MethodLocalSymbol& targetLocal = locals_[*assignmentTargetLocal];
        targetLocal.callableTrait = right.callableTrait;
        invokedFnOnceLocals_.erase(*assignmentTargetLocal);
        maybeInvokedFnOnceLocals_.erase(*assignmentTargetLocal);
    }
    if (moveOnlyTargetLocal.has_value())
    {
        movedRaiiLocals_.erase(*moveOnlyTargetLocal);
        maybeMovedRaiiLocals_.erase(*moveOnlyTargetLocal);
    }
    if (moveOnlyTargetParameter.has_value())
    {
        movedOwnedParameters_.erase(*moveOnlyTargetParameter);
        maybeMovedOwnedParameters_.erase(*moveOnlyTargetParameter);
    }
    if (const std::optional<std::string> assignedPlace =
            partialMovePlaceKey(expression->first(), scopeOrdinal))
        reinitializePartialPlace(*assignedPlace);
    if (assignedUnionStorage.has_value())
    {
        if (assignedUnionMember.has_value())
            activeUnionMembers_[*assignedUnionStorage] = *assignedUnionMember;
        else
            activeUnionMembers_.erase(*assignedUnionStorage);
    }
    const MethodEscapeOrigin sourceEscape =
        referenceEscapeOriginOfExpression(expression->second(), scopeOrdinal);
    if (!sourceEscape.ordinal.has_value() ||
        (sourceEscape.kind != MethodEscapeOriginKind::ExternalStorage &&
         sourceEscape.kind != MethodEscapeOriginKind::CurrentFrameParameter)) return;
    const MethodEscapeOrigin destinationEscape =
        referenceEscapeOriginOfExpression(expression->first(), scopeOrdinal);
    if (destinationEscape.kind == MethodEscapeOriginKind::ExternalStorage &&
        !destinationEscape.ordinal.has_value())
        noteParameterEscape(methodMemberOrdinal, *sourceEscape.ordinal, language::EscapeClass::Global);
    else if (destinationEscape.kind == MethodEscapeOriginKind::CurrentReceiver ||
             (destinationEscape.kind == MethodEscapeOriginKind::ExternalStorage &&
              destinationEscape.ordinal.has_value()))
        noteParameterEscape(
            methodMemberOrdinal, *sourceEscape.ordinal, language::EscapeClass::CallerOwned);
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::finishPlainAssignment(
    const ast::AstNode* expression,
    ExpressionResult left,
    const ExpressionResult& right,
    const std::optional<std::size_t>& raiiTargetLocal,
    const std::optional<std::size_t>& assignmentTargetLocal,
    const std::optional<std::size_t>& moveOnlyTargetLocal,
    const std::optional<std::size_t>& moveOnlyTargetParameter,
    const std::optional<std::size_t>& assignedUnionStorage,
    const std::optional<std::size_t>& assignedUnionMember,
    bool ownsStorageDestination,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!validatePlainAssignmentConversion(expression, left, right, diagnostics)) return {};
    updatePlainAssignmentState(
        expression, right, raiiTargetLocal, assignmentTargetLocal, moveOnlyTargetLocal,
        moveOnlyTargetParameter, assignedUnionStorage, assignedUnionMember,
        methodMemberOrdinal, scopeOrdinal);
    left.ownsStorageDestination = ownsStorageDestination;
    left.assignable = false;
    invalidateAssignedStorage(expression->first(), scopeOrdinal);
    return left;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::finishPointerCompoundAssignment(
    const ast::AstNode* expression,
    ExpressionResult left,
    const ExpressionResult& right,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const bool offsetOperator = expression->tokenKind() == TokenKind::PlusEqual ||
        expression->tokenKind() == TokenKind::MinusEqual;
    if (!offsetOperator || left.typeKey == "builtin:void*" ||
        !ProjectTypeConversion::isIntegral(right.typeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "a non-void pointer supports only '+=' and '-=' with an integral right-hand side");
        return {};
    }
    left.assignable = false;
    invalidateAssignedStorage(expression->first(), scopeOrdinal);
    return left;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::finishNumericCompoundAssignment(
    const ast::AstNode* expression,
    ExpressionResult left,
    const ExpressionResult& right,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const TokenKind compoundBase = lexer::compoundAssignmentBaseOperator(expression->tokenKind());
    const bool sameEnumBitwise = isEnumTypeKey(left.typeKey) && left.typeKey == right.typeKey &&
        (compoundBase == TokenKind::Amp || compoundBase == TokenKind::Pipe ||
         compoundBase == TokenKind::Caret);
    const bool shiftAssignment = isShift(compoundBase) &&
        ProjectTypeConversion::isIntegral(left.typeKey) &&
        ProjectTypeConversion::isIntegral(right.typeKey) &&
        left.typeKey != "builtin:bool" && right.typeKey != "builtin:bool";
    const std::optional<std::string> common = sameEnumBitwise || shiftAssignment
        ? std::optional<std::string>(left.typeKey)
        : ProjectTypeConversion::commonNumericType(
              left.typeKey, right.typeKey, left.integerConstant, right.integerConstant);
    if (!common.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "compound assignment requires compatible numeric operands in Stage 34");
        return {};
    }
    const ProjectImplicitConversion narrowing =
        classifyImplicit(right.typeKey, left.typeKey, right.integerConstant);
    if (!shiftAssignment && !narrowing.exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "compound assignment cannot implicitly convert its right-hand side from '" +
                right.typeKey + "' to '" + left.typeKey +
                "'; write the read-modify-write with an explicit cast");
        return {};
    }
    left.assignable = false;
    invalidateAssignedStorage(expression->first(), scopeOrdinal);
    return left;
}

bool ProjectMethodBodyModel::prepareAssignmentTarget(
    ExpressionResult& left,
    std::optional<std::size_t>& raiiTargetLocal,
    std::optional<std::size_t>& moveOnlyTargetLocal,
    std::optional<std::size_t>& moveOnlyTargetParameter,
    std::optional<std::size_t>& assignedUnionStorage,
    std::optional<std::size_t>& assignedUnionMember,
    bool& arrayElementMoveTarget,
    bool& ownedArrayElementOwnsStorage,
    bool& borrowedArrayElementTarget,
    bool& ownedFieldMoveTarget,
    bool& genericFieldMoveTarget,
    bool& ownedFieldOwnsStorage,
    std::string& ownedFieldName,
    std::optional<std::size_t>& getterBindingIndex,
    bool& ownedSetterMoveTarget,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    recordAssignedOutParameter(expression, methodMemberOrdinal, scopeOrdinal);
    classifyDirectAssignmentTarget(
        left, raiiTargetLocal, moveOnlyTargetLocal, moveOnlyTargetParameter,
        expression, methodMemberOrdinal, scopeOrdinal);
    if (!analyzeAssignmentLeft(
            left, expression, methodMemberOrdinal, scopeOrdinal, diagnostics)) return false;
    trackRequiredFieldAssignment(left, methodMemberOrdinal);
    classifyAssignedUnion(
        assignedUnionStorage, assignedUnionMember, expression, left,
        methodMemberOrdinal, scopeOrdinal);
    classifyArrayElementAssignmentTarget(
        arrayElementMoveTarget, ownedArrayElementOwnsStorage, borrowedArrayElementTarget,
        expression, left, raiiTargetLocal, methodMemberOrdinal, scopeOrdinal, diagnostics);
    classifyOwnedFieldAssignmentTarget(
        ownedFieldMoveTarget, genericFieldMoveTarget, ownedFieldOwnsStorage, ownedFieldName,
        left, raiiTargetLocal, arrayElementMoveTarget, methodMemberOrdinal);
    getterBindingIndex = assignmentGetterBindingIndex(expression, methodMemberOrdinal);
    if (!raiiTargetLocal.has_value() && !arrayElementMoveTarget && !ownedFieldMoveTarget &&
        getterBindingIndex.has_value() && memberModel_ != nullptr)
    {
        const std::size_t getterOrdinal = *calls_[*getterBindingIndex].selectedMemberOrdinal;
        ownedSetterMoveTarget = memberModel_->members()[getterOrdinal].ownsAccessorStorage;
    }
    return true;
}

bool ProjectMethodBodyModel::prepareAssignmentValue(
    ExpressionResult& right,
    std::optional<std::size_t>& assignmentTargetLocal,
    std::optional<std::size_t>& assignmentTargetParameter,
    bool& borrowedReassignment,
    bool& delegateTarget,
    bool& raiiStringAppend,
    bool& ownsStorageDestination,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const std::optional<std::size_t>& raiiTargetLocal,
    const std::optional<std::size_t>& moveOnlyTargetLocal,
    const std::optional<std::size_t>& moveOnlyTargetParameter,
    bool arrayElementMoveTarget,
    bool ownedArrayElementOwnsStorage,
    bool borrowedArrayElementTarget,
    bool ownedFieldMoveTarget,
    bool genericFieldMoveTarget,
    bool ownedFieldOwnsStorage,
    const std::string& ownedFieldName,
    bool ownedSetterMoveTarget,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (rejectUnavailableAssignmentOwner(
            expression, raiiTargetLocal, scopeOrdinal, diagnostics)) return false;
    delegateTarget = isDelegateTypeKey(left.typeKey);
    right = analyzeAssignmentRight(
        expression, left, raiiTargetLocal, moveOnlyTargetLocal, moveOnlyTargetParameter,
        arrayElementMoveTarget, ownedFieldMoveTarget, genericFieldMoveTarget,
        ownedSetterMoveTarget, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (!right.valid || rejectMoveOnlyAssignmentCopy(expression, left, right, diagnostics)) return false;
    raiiStringAppend = raiiTargetLocal.has_value() &&
        expression->tokenKind() == TokenKind::PlusEqual && left.typeKey == "builtin:string" &&
        (right.typeKey == "builtin:string" || right.typeKey == "string-literal");
    classifyNamedAssignmentTarget(
        assignmentTargetLocal, assignmentTargetParameter, expression, scopeOrdinal);
    if (!validateAssignmentTargetStorage(
            expression, right, assignmentTargetLocal, scopeOrdinal, diagnostics)) return false;
    recordViewAssignmentRoot(expression, scopeOrdinal);
    if (!analyzeBorrowedReassignment(
            borrowedReassignment, expression, left, right, assignmentTargetLocal,
            assignmentTargetParameter, methodMemberOrdinal, scopeOrdinal, diagnostics)) return false;
    if (rejectBorrowedAssignmentEscape(
            expression, left, right, borrowedReassignment,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return false;
    if (rejectOwnerCopyAssignment(
            expression, borrowedReassignment, borrowedArrayElementTarget, raiiTargetLocal,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return false;
    ownsStorageDestination = ownedFieldOwnsStorage || ownedArrayElementOwnsStorage;
    if (!validateOwningStorageAssignment(
            expression, right, ownedFieldOwnsStorage, ownedArrayElementOwnsStorage,
            ownedFieldName, diagnostics)) return false;
    if (delegateTarget && expression->tokenKind() != TokenKind::Equal)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "Stage 42 delegates are single-cast values; use an event for +=/-= multicast subscription");
        return false;
    }
    return validateRaiiAssignmentTarget(
               expression, raiiTargetLocal, raiiStringAppend, diagnostics) &&
        validateMoveOnlyAssignmentTarget(
               expression, moveOnlyTargetLocal, moveOnlyTargetParameter, diagnostics);
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::finishAssignment(
    const ast::AstNode* expression,
    ExpressionResult left,
    const ExpressionResult& right,
    const std::optional<std::size_t>& getterBindingIndex,
    const std::optional<std::size_t>& raiiTargetLocal,
    const std::optional<std::size_t>& assignmentTargetLocal,
    const std::optional<std::size_t>& moveOnlyTargetLocal,
    const std::optional<std::size_t>& moveOnlyTargetParameter,
    const std::optional<std::size_t>& assignedUnionStorage,
    const std::optional<std::size_t>& assignedUnionMember,
    bool ownedFieldOwnsStorage,
    bool ownedArrayElementOwnsStorage,
    bool raiiStringAppend,
    bool ownsStorageDestination,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const MethodCallBinding* getterBinding = assignmentSetterGetter(getterBindingIndex);
    if (getterBinding != nullptr)
        return analyzePropertySetterAssignment(
            expression, left, right, *getterBinding,
            methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (const std::optional<ExpressionResult> extensionAssignment =
            analyzeExtensionPropertyAssignment(
                expression, left, methodMemberOrdinal, scopeOrdinal, diagnostics))
        return *extensionAssignment;
    if (expression->tokenKind() == TokenKind::Equal && expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression)
    {
        ExpressionResult assignmentResult = right;
        assignmentResult.ownsStorageDestination = ownedFieldOwnsStorage;
        assignmentResult.assignable = false;
        assignmentResult.valid = true;
        return assignmentResult;
    }
    if (expression->tokenKind() == TokenKind::Equal && expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::NullConditionalIndexExpression)
        return analyzeNullConditionalIndexAssignment(
            expression, left, right, ownedArrayElementOwnsStorage, diagnostics);
    if (expression->tokenKind() != TokenKind::Equal && expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression)
        return analyzeNullConditionalCompoundAssignment(
            expression, left, right, ownedFieldOwnsStorage, diagnostics);
    if (!left.assignable)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "left side of assignment is not assignable");
        return {};
    }
    if (raiiStringAppend)
    {
        left.ownsStorageDestination = true;
        left.assignable = false;
        left.ownership = sema::OwnershipKind::Owned;
        left.ownershipTransfer = true;
        left.ownershipFreshOwner = true;
        invalidateAssignedStorage(expression->first(), scopeOrdinal);
        return left;
    }
    if (expression->tokenKind() == TokenKind::Equal)
        return finishPlainAssignment(
            expression, left, right, raiiTargetLocal, assignmentTargetLocal,
            moveOnlyTargetLocal, moveOnlyTargetParameter, assignedUnionStorage,
            assignedUnionMember, ownsStorageDestination,
            methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (ProjectTypeConversion::isPointer(left.typeKey))
        return finishPointerCompoundAssignment(
            expression, left, right, scopeOrdinal, diagnostics);
    return finishNumericCompoundAssignment(
        expression, left, right, scopeOrdinal, diagnostics);
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeAssignment(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    recordReferenceFieldAssignmentOrigin(
        expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
    reportScopeAssignmentEscape(expression, scopeOrdinal, diagnostics);
    if (assignmentConflictsWithIterationBorrow(expression, scopeOrdinal, diagnostics)) return {};
    if (expression->tokenKind() == TokenKind::QuestionQuestionEqual)
    {
        return analyzeNullCoalescingAssignment(
            expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    if (expression->tokenKind() == TokenKind::Equal && expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::TupleExpression)
    {
        return analyzeTupleDeconstructionAssignment(
            expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }

    // Stage 52: a plain (non-RAII-local) assignment target may still be a legitimate move(...)
    // destination when it is itself a direct native array element -- e.g. EnsureCapacity's
    // "grown[i] = move(items[i]);" while growing a generic owned T[] backing store to relocate
    // an element from the old array into the new one. This is the exact "single owning slot"
    // invariant move(...) exists to preserve; it just involves two array slots rather than a
    // named RAII local/parameter on either side. The RHS move(...)'s own array-element-source
    // check (see the MoveExpression case in analyzeExpression) independently verifies the
    // source; this only widens which assignment TARGETS are allowed to receive it.
        // SHR-005: y el ARREGLO FIJO, por la misma razon y con la misma forma. La fuente
        // `move(buf[i])` se admite unas mil lineas mas arriba con esta misma condicion doble; si
        // solo se admitiera de un lado, `dst[i] = move(src[j])` compilaria en una direccion y no
        // en la otra. La admision de destino es simetrica a la de fuente a proposito.
        // A native array is raw storage, but an array reached through an `owned`/`raii` binding is
        // also the container's owning element store. Moving into one of its reference-like slots
        // therefore has the same replacement contract as writing an `owned` field: the RHS must
        // transfer ownership and the displaced value must be released before the new value is
        // installed. Value elements remain ordinary value stores.
    // Bloque 1 item 1 (owning field storage): a field declared `owned` is a legitimate move(...)
    // destination. Before this, `owned` FIELDS could be declared and parsed -- the Stage 50 syntax
    // tests assert exactly that -- but nothing could ever be put into one, because no assignment
    // target other than a RAII local or an array element admitted a move. The feature was
    // declarable and unusable.
    //
    // Admitting the target is only half of it: a class that owns a field has to RELEASE it, or the
    // transfer would just move the leak. The generated destruction entry now walks the type's owned
    // fields (see ProjectOopMirDispatchTable::ownedFields), so ownership handed to a field is
    // discharged when the object dies, exactly once.
    // True only when the owned field also OWNS a separate allocation (reference type or native
    // string) -- the storage classes whose stores must be ownership transfers. A generic `owned T`
    // field closed over a value type keeps plain assignment: `owned` is the no-op it has always
    // been there, same rule as owned locals and owned parameters.
    //
    // SHR-005, mitad "agregado por valor" -- MEDIDA, y no es un hueco. Esta condicion es la misma
    // que `requiresOwnedDestruction` en ProjectOopLlvmLowerer.cpp:
    //
    //     requiresOwnedDestruction(t)  ==  (t == "builtin:string" || isClassReference(t))
    //     ownedFieldOwnsStorage        ==  (isReferenceTypeKey(t)  || t == "builtin:string")
    //
    // Las dos mitades de la regla --exigir una expresion de transferencia al escribir, y liberar
    // el valor desplazado al bajar-- disparan sobre EXACTAMENTE el mismo conjunto de tipos. Para
    // un struct por valor no dispara ninguna de las dos, que es lo coherente: no hay reserva
    // separada que transferir ni que liberar. Se comprueba contando destrucciones en ejecucion en
    // Stage50_11OwnershipExecutionTests, que es como este proyecto verifica ownership. Lo que
    // quedaba de SHR-005 era el ARREGLO FIJO, y esta arriba, en arrayElementMoveTarget.
    // Bloque 1 item 9 (ownership-aware properties/indexers): find the getter binding NOW, before
    // the right-hand side is analyzed, so `h.Slot = move(a)` and `h[0] = move(a)` can be admitted
    // as ownership transfers.
    //
    // An assignment to a property or indexer is analyzed by first binding the LHS as a GETTER
    // call and then, further down, re-resolving it as a synthetic `set_X` call with the RHS
    // appended as the last argument. That lookup used to live entirely below the RHS analysis,
    // which was exactly the problem: by the time anything knew a setter was involved, the RHS
    // had already been rejected -- `move(...)` is only accepted when the caller passes
    // allowMove, and no property/indexer target ever set it.
    //
    // The binding is recorded by the `left` analysis above, so the callable-aware index can find
    // it directly. It is remembered as an INDEX rather than a pointer: analyzing the RHS can
    // append to calls_ and invalidate a pointer, while an existing ordinal remains stable.
    // The getter carries the property's ownership (see ClassMemberSymbol::ownsAccessorStorage):
    // when the property is declared `owned`, its setter's implicit `value` parameter is an owned
    // parameter, so storing into it through the setter is a real ownership transfer and not a
    // silent aliasing copy. This is the fourth and last legitimate move(...) destination, next to
    // a RAII local, a native array element and an `owned` field.
    // Stage 52: same value-type bypass as the local-declaration case above -- a borrowed value
    // assigned into a plain local/field/parameter whose CLOSED type never needed ownership
    // tracking (a generic T monomorphized to int, bool, a struct, ...) carries no real lifetime
    // to protect.
    // SHR-013: `v = otra;` cambia de que presta `v`, asi que el enraizamiento grabado en la
    // declaracion deja de valer. Se ACUMULA en vez de reemplazarse: tras dos asignaciones en
    // ramas distintas la vista puede venir de cualquiera de las dos, y la respuesta segura es
    // quedarse con la primera que la ate a un local.
    // El destino puede ser el local a secas (`v = ...`) o un ELEMENTO de un arreglo local de
    // vistas (`tokens[i] = ...`). El segundo caso importa tanto como el primero: un
    // `StringView[]` local lleno de vistas de otro local y devuelto despues seria la misma fuga
    // con un nivel de indireccion. El enraizamiento se acumula EN EL ARREGLO, que es el valor
    // que puede escapar.
    // T41: borrowed names are rebindable alias slots.  Reassignment is safe only when the RHS
    // has explicit, named provenance, and every possible owner root is accumulated instead of
    // replacing the old one.  Accumulation is deliberately conservative across branches: after
    // `if (...) view = a; else view = b;`, moving either owner remains forbidden while `view`
    // lives, and returning the view is rejected if either root is a local/owned parameter.
        // Bloque 1: the same rule with an owned PARAMETER as the source. This was the one hole
        // left in the copy-owner contract, and it was a real double free -- measured, not
        // theorised: `set { backing = value; }` on an `owned` property compiled and destroyed the
        // object twice (the field's release plus the parameter's own end-of-scope cleanup).
        //
        // Every other destination already caught it: owned parameter into a RAII local ("RAII
        // ownership cannot be copied or borrowed"), owned parameter into an owned argument
        // ("cannot copy owned value 'value' into owned parameter; use move(value)"), and an owner
        // LOCAL into a field (the check just above). Only "owned parameter into a field" was
        // missing, which is exactly the store the owned-field work of this same block had just
        // made reachable -- the feature opened a destination and the rule that guards every other
        // destination did not follow it there.
        //
        // Same value-type narrowing as everywhere else: `owned` over a type that owns no separate
        // allocation is the no-op it has always been, so copying one is not a double free and must
        // not be reported as one.
    // Bloque 1 (audit finding "overwrite sin transferencia explicita"): every write into a field
    // that OWNS storage must be an ownership-transfer expression -- move(...), new T(...), null,
    // or an owned-returning call. This is the exact set the owned-return check accepts and the
    // owned-local initializer accepts; a field cannot distinguish initialization from assignment,
    // so it takes the full transfer set rather than the locals' stricter move-only assignment
    // rule, and `field = null` is the legitimate Clear operation (release what the slot holds,
    // leave it empty).
    //
    // What this rejects is the remaining silent hole: a PLAIN alias flowing into an owning slot
    // (`h.slot = plainLocal`, `h.slot = someCall()` returning an unqualified reference). Accepting
    // it converted a non-owner reference into effective ownership -- the container would later
    // release an object somebody else still references, or that somebody else separately frees.
    // The identifier-source cases (raii/owned local, owned parameter) are caught above with their
    // own more specific messages; this catches every other non-transfer shape.
    //
    // The release of the displaced value is the OTHER half of the same rule and is handled in the
    // HIR/lowering (see the Assignment case in ProjectOopHirModel): it fires for every one of
    // these forms, not just the move-spelled one -- that asymmetry was a measured leak (0
    // destructions for `= null`, 1 of 2 for `= new`).
    // Resolved above, before the right-hand side was analyzed, so that an `owned` property or
    // indexer could admit move(...) on its RHS. Only now is it safe to take a pointer: nothing
    // appends to calls_ between here and the last use below.
        // A ref indexer getter exposes its returned location directly. It is an ordinary
        // addressable assignment target and must not be rewritten into a nonexistent setter.
        // A readonly getter can expose a value through an `in` receiver, but that does not make
        // the companion setter callable. Property/indexer assignment is rewritten from the
        // recorded getter before the general `!left.assignable` guard below, so preserve the
        // receiver capability explicitly at this boundary.
        // NS304-T09-F02: un setter `init` solo es asignable durante la construccion. Por ahora el
        // contexto permitido es un constructor de la misma declaracion; el object initializer
        // (`new T { X = 1 }`) queda pendiente del rastreo de contexto de inicializacion.
        // ID 82 -- la asignacion COMPUESTA sobre propiedad e indexador. Hasta aqui esto se
        // rechazaba con «compound assignment to a Stage 38 property/indexer is deferred».
        //
        // QUE SIGNIFICA. `p.X += e` es `set_X(p, get_X(p) + e)`. El getter ya esta ligado arriba
        // (getterBinding) porque el analisis del lado izquierdo lo produjo; el setter se resuelve
        // unas lineas mas abajo. Lo unico que falta es decir de que TIPO es el valor que se le
        // pasa al setter, y decirlo con la MISMA regla que gobierna `x += e` sobre un destino
        // corriente -- no con una segunda.
        //
        // LA REGLA, y es literalmente la de mas abajo en esta misma funcion:
        //
        //   1. `commonNumericType(destino, derecho)` tiene que existir: operandos compatibles.
        //   2. el operando derecho tiene que convertir IMPLICITAMENTE al tipo del destino. Esta
        //      es la divergencia deliberada respecto de C# que el ID 81 escribio: C# mete un cast
        //      implicito de vuelta y TRUNCA (`byte b; int n; b += n;` compila); aqui se pide el
        //      cast escrito. Una propiedad no puede ser la excepcion a esa regla.
        //   3. la operacion ocurre ENTERA en el tipo del destino, que es exactamente lo que
        //      `CompoundStore` hace para un destino con direccion (ver su bajada en
        //      ProjectOopHirModel). Por eso el argumento del setter se anota con el tipo del
        //      GETTER y no con el del operando derecho: asi la conversion que registra
        //      resolveSyntheticCall es la identidad y no hay una segunda tabla de promociones.
        //
        // LO QUE HAY QUE DEMOSTRAR APARTE. El receptor y los argumentos del indexador se bajan
        // dos veces --una por llamada-- porque el arbol HIR es de `unique_ptr` y un nodo no se
        // comparte entre dos padres. Sobre la clase de expresiones que acepta
        // isAccessorDestinationReevaluable esa segunda bajada da el mismo valor por construccion;
        // fuera de ella se rechaza con el rango de fuente del subnodo culpable. Ver la
        // declaracion de esa funcion para el argumento completo.
            // El valor que recibe el setter es del tipo del DESTINO, no del operando derecho.
            // Fresco a proposito: copiar `left` arrastraria assignmentMemberOrdinal y la
            // propiedad de ownership del getter a un valor que ya no es ese almacenamiento.
    // NS304-T08-F02 (residual): `receiver.ExtensionProperty = value` lowers to a namespace call
    // `set_ExtensionProperty(receiver, value)`. The getter was auto-invoked while analyzing the
    // left side; the member's extension binding is re-detected here and re-routed to the setter.
                // The setter lives in the same namespace as the getter, not in the call site's
                // namespace (which may merely import it via `using`).
    // NS304-T10-F05: `obj?.Field = value` evaluates the RHS only on the non-null edge.
    // NS304-T10-F05: `arr?[i] = value` — store condicional al null-check del receptor (array nativo).
    // NS304-T10-F06: null-conditional compound assignment evaluates its RHS and performs the
    // read-modify-write only on the non-null edge.
        // NS304-T10-F06: string concat compound — el lowering hace load + concat + store
        // condicional (la propiedad del string viejo se libera en la ruta normal del backend).
        // The concatenation is a fresh owner. Lowering materializes it before releasing the old
        // buffer, then reuses the ordinary owning-store path to replace the same RAII slot.
    // ID 81. DESTINO PUNTERO: `p += n` y `p -= n` avanzan y retroceden POR ELEMENTOS, igual que
    // `p + n`. Ningun otro operador compuesto tiene sentido sobre un puntero, y `void*` queda
    // fuera por la misma razon que en `*p` y en `++p`: no hay tamano que avanzar.
    // ID 81. Y ADEMAS el operando derecho tiene que caber en el tipo del destino. C# aqui mete un
    // cast implicito de vuelta (`byte b; int n; b += n;` compila y TRUNCA); este lenguaje lo
    // rechaza y pide el cast escrito. Es una DIVERGENCIA DELIBERADA de C#, anotada en el doc 79:
    // el resultado se guarda en el destino, y un estrechamiento que el codigo no escribio es
    // justo la clase de perdida silenciosa que este compilador viene rechazando en todas partes.
    // La conversion constante sigue valiendo, asi que `byte b; b += 1;` compila sin ceremonia.
    ExpressionResult left;
    std::optional<std::size_t> raiiTargetLocal;
    std::optional<std::size_t> moveOnlyTargetLocal;
    std::optional<std::size_t> moveOnlyTargetParameter;
    std::optional<std::size_t> assignedUnionStorage;
    std::optional<std::size_t> assignedUnionMember;
    bool arrayElementMoveTarget = false;
    bool ownedArrayElementOwnsStorage = false;
    bool borrowedArrayElementTarget = false;
    bool ownedFieldMoveTarget = false;
    bool genericFieldMoveTarget = false;
    bool ownedFieldOwnsStorage = false;
    std::string ownedFieldName;
    std::optional<std::size_t> getterBindingIndex;
    bool ownedSetterMoveTarget = false;
    if (!prepareAssignmentTarget(
            left, raiiTargetLocal, moveOnlyTargetLocal, moveOnlyTargetParameter,
            assignedUnionStorage, assignedUnionMember, arrayElementMoveTarget,
            ownedArrayElementOwnsStorage, borrowedArrayElementTarget, ownedFieldMoveTarget,
            genericFieldMoveTarget, ownedFieldOwnsStorage, ownedFieldName, getterBindingIndex,
            ownedSetterMoveTarget, expression, methodMemberOrdinal, scopeOrdinal, diagnostics))
    {
        return {};
    }
    if (left.eventMemberOrdinal.has_value())
    {
        return analyzeEventAssignment(
            expression, left, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }

    ExpressionResult right;
    std::optional<std::size_t> assignmentTargetLocal;
    std::optional<std::size_t> assignmentTargetParameter;
    bool borrowedReassignment = false;
    bool delegateTarget = false;
    bool raiiStringAppend = false;
    bool ownsStorageDestination = false;
    if (!prepareAssignmentValue(
            right, assignmentTargetLocal, assignmentTargetParameter, borrowedReassignment,
            delegateTarget, raiiStringAppend, ownsStorageDestination, expression, left,
            raiiTargetLocal, moveOnlyTargetLocal, moveOnlyTargetParameter, arrayElementMoveTarget,
            ownedArrayElementOwnsStorage, borrowedArrayElementTarget, ownedFieldMoveTarget,
            genericFieldMoveTarget, ownedFieldOwnsStorage, ownedFieldName,
            ownedSetterMoveTarget, methodMemberOrdinal, scopeOrdinal, diagnostics))
    {
        return {};
    }
    return finishAssignment(
        expression, left, right, getterBindingIndex, raiiTargetLocal, assignmentTargetLocal,
        moveOnlyTargetLocal, moveOnlyTargetParameter, assignedUnionStorage, assignedUnionMember,
        ownedFieldOwnsStorage, ownedArrayElementOwnsStorage, raiiStringAppend,
        ownsStorageDestination, methodMemberOrdinal, scopeOrdinal, diagnostics);
}


std::vector<std::size_t> ProjectMethodBodyModel::memberCandidates(
    std::size_t methodMemberOrdinal,
    std::string_view name) const
{
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method == nullptr)
    {
        return {};
    }
    return memberCandidatesForType(method->declaringTypeOrdinal, name);
}

std::vector<std::size_t> ProjectMethodBodyModel::memberCandidatesForType(
    std::size_t typeOrdinal,
    std::string_view name) const
{
    if (memberModel_ == nullptr)
    {
        return {};
    }

    if (interfaceModel_ != nullptr && interfaceModel_->isInterface(typeOrdinal))
    {
        const InterfaceTypeInfo* info = interfaceModel_->interfaceInfo(typeOrdinal);
        std::vector<std::size_t> result;
        if (info == nullptr)
        {
            return result;
        }
        for (const std::size_t memberOrdinal : info->effectiveMethodMemberOrdinals)
        {
            if (memberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[memberOrdinal].name == name)
            {
                result.push_back(memberOrdinal);
            }
        }
        return result;
    }

    std::optional<std::size_t> current = typeOrdinal;
    while (current.has_value())
    {
        const std::span<const std::size_t> found = memberModel_->findMembers(*current, name);
        std::vector<std::size_t> candidates(found.begin(), found.end());
        if (!candidates.empty())
        {
            return candidates;
        }
        current = inheritanceModel_ != nullptr
            ? inheritanceModel_->baseTypeOrdinal(*current)
            : std::nullopt;
    }

    // A default selected for a concrete class is also part of that class's static member
    // surface. Reuse the already-coherent interface-table plan instead of rediscovering
    // precedence here: the same member ordinal must serve a direct/constrained call and the
    // corresponding dyn dispatch slot.
    std::vector<std::size_t> defaults;
    if (interfaceModel_ != nullptr)
    {
        for (const ClassInterfaceTablePlan& plan : interfaceModel_->tablePlans())
        {
            if (plan.classTypeOrdinal != typeOrdinal) continue;
            for (const std::optional<std::size_t> implementation :
                 plan.implementationMemberOrdinals)
            {
                if (!implementation.has_value() ||
                    *implementation >= memberModel_->members().size() ||
                    memberModel_->members()[*implementation].name != name ||
                    std::ranges::find(defaults, *implementation) != defaults.end())
                    continue;
                defaults.push_back(*implementation);
            }
        }
    }
    return defaults;
}

std::optional<std::size_t> ProjectMethodBodyModel::referenceTypeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (inheritanceModel_ == nullptr)
    {
        return std::nullopt;
    }
    const std::optional<std::size_t> ordinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
    if (!ordinal.has_value())
    {
        return std::nullopt;
    }
    if (inheritanceModel_->isClass(*ordinal) ||
        (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)))
    {
        return ordinal;
    }
    return std::nullopt;
}

std::optional<std::size_t> ProjectMethodBodyModel::namedTypeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (inheritanceModel_ == nullptr)
    {
        return std::nullopt;
    }
    return inheritanceModel_->typeOrdinalForTypeKey(typeKey);
}

std::optional<std::size_t> ProjectMethodBodyModel::classTypeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (inheritanceModel_ == nullptr)
    {
        return std::nullopt;
    }
    const std::optional<std::size_t> ordinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
    if (!ordinal.has_value() || !inheritanceModel_->isClass(*ordinal))
    {
        return std::nullopt;
    }
    return ordinal;
}

std::optional<std::size_t> ProjectMethodBodyModel::resolveTypeIdentifier(
    std::size_t contextMemberOrdinal,
    std::string_view name) const noexcept
{
    const ClassMemberSymbol* context = methodSymbol(contextMemberOrdinal);
    if (context == nullptr || nameLookup_ == nullptr ||
        context->declaringTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return std::nullopt;
    }

    const NamedTypeDeclaration& contextType = nameLookup_->namedTypes()[context->declaringTypeOrdinal];
    std::optional<std::size_t> fallback;
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& candidate = nameLookup_->namedTypes()[ordinal];
        if ((candidate.kind != NamedTypeDeclarationKind::Class &&
             candidate.kind != NamedTypeDeclarationKind::Struct &&
             candidate.kind != NamedTypeDeclarationKind::Enum) || candidate.identity.name() != name)
        {
            continue;
        }
        if (candidate.projectOrdinal == contextType.projectOrdinal &&
            candidate.identity.namespaceSegments() == contextType.identity.namespaceSegments())
        {
            return ordinal;
        }
        if (!fallback.has_value())
        {
            fallback = ordinal;
        }
        else
        {
            // Do not guess between ambiguous simple names from other visible namespaces/modules.
            fallback.reset();
        }
    }
    return fallback;
}

bool ProjectMethodBodyModel::contextHasImplicitThis(std::size_t memberOrdinal) const noexcept
{
    const ClassMemberSymbol* member = methodSymbol(memberOrdinal);
    return member != nullptr && member->hasImplicitThis;
}

bool ProjectMethodBodyModel::contextHasReadonlyReceiver(std::size_t memberOrdinal) const noexcept
{
    const ClassMemberSymbol* member = methodSymbol(memberOrdinal);
    return member != nullptr && member->declaringTypeIsStruct && member->hasImplicitThis &&
        member->isReadonly;
}

bool ProjectMethodBodyModel::readonlyFieldAssignable(
    const ClassMemberSymbol& field,
    std::size_t contextMemberOrdinal) const noexcept
{
    if (!field.isReadonly || field.isStatic)
    {
        return false;
    }
    const ClassMemberSymbol* context = methodSymbol(contextMemberOrdinal);
    return context != nullptr && context->kind == ClassMemberKind::Constructor &&
        context->declaringTypeOrdinal == field.declaringTypeOrdinal;
}

std::optional<std::size_t> ProjectMethodBodyModel::automaticPropertyBackingField(
    std::size_t typeOrdinal,
    std::string_view propertyName,
    bool isStatic) const noexcept
{
    if (memberModel_ == nullptr) return std::nullopt;
    for (const std::size_t ordinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (member.kind == ClassMemberKind::Field && member.isCompilerGenerated &&
            !member.isEventBacking && member.associatedName == propertyName &&
            member.isStatic == isStatic)
        {
            return ordinal;
        }
    }
    return std::nullopt;
}
}
