#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "Support/OwnershipArgumentClassification.h"
#include "Support/OperatorClassification.h"
#include "Support/IntegralConstantArithmetic.h"
#include "Support/OwnershipFlowSnapshotQueries.h"


#include "MethodBodyInternals.h"
#include "../Internal/Stage37ConstantValue.h"

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
using lexer::TokenKind;

using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;

std::optional<std::size_t> ProjectMethodBodyModel::delegateTypeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return std::nullopt;
    const std::string_view key = typeKey.substr(5);
    const std::optional<std::size_t> ordinal = nameLookup_->typeOrdinalForIdentity(key);
    const NamedTypeDeclaration* type = ordinal.has_value()
        ? nameLookup_->typeForOrdinal(*ordinal) : nullptr;
    return type != nullptr && type->kind == NamedTypeDeclarationKind::Delegate
        ? ordinal : std::nullopt;
}

bool ProjectMethodBodyModel::isDelegateTypeKey(std::string_view typeKey) const noexcept
{
    return language::isFunctionTypeIdentity(typeKey) ||
        language::isAnonymousLambdaTypeIdentity(typeKey) ||
        delegateTypeOrdinalForTypeKey(typeKey).has_value();
}

bool ProjectMethodBodyModel::delegateSignature(
    std::string_view delegateTypeKey,
    std::string& returnTypeKey,
    std::vector<std::string>& parameterTypeKeys,
    std::size_t& delegateTypeOrdinal) const
{
    returnTypeKey.clear();
    parameterTypeKeys.clear();
    if (const std::optional<language::AnonymousLambdaType> lambdaType =
            language::parseAnonymousLambdaTypeIdentity(delegateTypeKey))
    {
        returnTypeKey = lambdaType->signature.result;
        parameterTypeKeys = lambdaType->signature.parameters;
        delegateTypeOrdinal = std::numeric_limits<std::size_t>::max();
        return true;
    }
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(delegateTypeKey))
    {
        returnTypeKey = functionType->result;
        parameterTypeKeys = functionType->parameters;
        delegateTypeOrdinal = std::numeric_limits<std::size_t>::max();
        return true;
    }
    const std::optional<std::size_t> ordinal = delegateTypeOrdinalForTypeKey(delegateTypeKey);
    if (!ordinal.has_value() || nameLookup_ == nullptr || memberModel_ == nullptr ||
        *ordinal >= nameLookup_->namedTypes().size())
    {
        return false;
    }
    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[*ordinal];
    if (type.syntax == nullptr || type.syntax->kind() != ast::AstNodeKind::DelegateDeclaration)
    {
        return false;
    }
    returnTypeKey = memberModel_->canonicalTypeKey(type.syntax->typeSyntax(), *nameLookup_);
    for (const ast::AstNode* parameter : type.syntax->children())
    {
        if (parameter != nullptr && parameter->kind() == ast::AstNodeKind::ParameterDeclaration)
        {
            parameterTypeKeys.push_back(memberModel_->canonicalTypeKey(parameter->typeSyntax(), *nameLookup_));
        }
    }
    delegateTypeOrdinal = *ordinal;
    return !returnTypeKey.empty() && !isErrorType(returnTypeKey);
}

bool ProjectMethodBodyModel::methodGroupExactlyMatchesDelegate(
    const ExpressionResult& methodGroup,
    std::string_view delegateTypeKey) const
{
    if (!methodGroup.methodGroup) return false;
    std::string returnType;
    std::vector<std::string> parameterTypes;
    std::size_t delegateOrdinal = 0;
    if (!delegateSignature(delegateTypeKey, returnType, parameterTypes, delegateOrdinal))
        return false;
    const std::string expectedErrorType = delegateErrorTypeKey(delegateTypeKey);
    const std::optional<language::FunctionType> structuralTarget =
        language::parseFunctionTypeIdentity(delegateTypeKey);
    std::size_t matches = 0;
    for (const std::size_t candidateOrdinal : methodGroup.methodCandidates)
    {
        const ClassMemberSymbol* candidate = methodSymbol(candidateOrdinal);
        if (candidate == nullptr || candidate->kind != ClassMemberKind::Method ||
            candidate->parameters.size() != parameterTypes.size() ||
            candidate->declaredTypeKey != returnType ||
            candidate->declaredErrorTypeKey != expectedErrorType ||
            (candidate->isUnsafe &&
             (!structuralTarget.has_value() || !structuralTarget->isUnsafe)))
            continue;
        bool exact = true;
        for (std::size_t index = 0; index < parameterTypes.size(); ++index)
        {
            if (candidate->parameters[index].typeKey != parameterTypes[index])
            {
                exact = false;
                break;
            }
        }
        if (exact && ++matches > 1) return false;
    }
    return matches == 1;
}

std::optional<std::size_t> ProjectMethodBodyModel::lambdaCaptureOrdinal(
    const MethodLambdaBinding& binding,
    MethodLambdaCaptureKind kind,
    std::optional<std::size_t> localOrdinal,
    std::optional<std::size_t> parameterOrdinal) const noexcept
{
    for (std::size_t index = 0; index < binding.captures.size(); ++index)
    {
        const MethodLambdaCapture& capture = binding.captures[index];
        if (capture.kind == kind && capture.localOrdinal == localOrdinal &&
            capture.parameterOrdinal == parameterOrdinal)
        {
            return index;
        }
    }
    return std::nullopt;
}

std::unordered_map<std::string, language::CaptureMode>
ProjectMethodBodyModel::explicitLambdaCaptureModes(MethodLambdaBinding& binding) const
{
    std::unordered_map<std::string, language::CaptureMode> explicitModes;
    const std::string_view captureContract = binding.syntax != nullptr
        ? std::string_view(binding.syntax->auxiliaryText()) : std::string_view{};
    binding.explicitCaptureList = captureContract.starts_with("captures:");
    if (!binding.explicitCaptureList) return explicitModes;
    std::string_view remaining = captureContract.substr(std::string_view("captures:").size());
    while (!remaining.empty())
    {
        const std::size_t separator = remaining.find(';');
        const std::string_view entry = remaining.substr(0, separator);
        const std::size_t equals = entry.find('=');
        if (equals != std::string_view::npos && equals + 1U < entry.size())
        {
            const std::string_view spelling = entry.substr(0, equals);
            language::CaptureMode mode = language::CaptureMode::Reference;
            if (spelling == "copy") mode = language::CaptureMode::Copy;
            else if (spelling == "move") mode = language::CaptureMode::Move;
            else if (spelling == "mutref") mode = language::CaptureMode::MutableReference;
            explicitModes.emplace(std::string(entry.substr(equals + 1U)), mode);
        }
        if (separator == std::string_view::npos) break;
        remaining.remove_prefix(separator + 1U);
    }
    return explicitModes;
}

std::optional<language::CaptureMode> ProjectMethodBodyModel::requestedLambdaCaptureMode(
    const MethodLambdaBinding& binding,
    const std::unordered_map<std::string, language::CaptureMode>& explicitModes,
    std::unordered_set<std::string>& reportedImplicitCaptures,
    std::string_view name,
    SourceRange range,
    bool& safe,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!binding.explicitCaptureList) return language::CaptureMode::Reference;
    const auto found = explicitModes.find(std::string(name));
    if (found != explicitModes.end()) return found->second;
    if (reportedImplicitCaptures.insert(std::string(name)).second)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            range,
            "lambda capture '" + std::string(name) +
                "' is not present in the explicit capture list");
    }
    safe = false;
    return std::nullopt;
}

std::size_t ProjectMethodBodyModel::addLambdaCapture(
    MethodLambdaBinding& target,
    MethodLambdaCapture capture) const
{
    const std::optional<std::size_t> existing = lambdaCaptureOrdinal(
        target, capture.kind, capture.localOrdinal, capture.parameterOrdinal);
    if (existing.has_value())
    {
        MethodLambdaCapture& prior = target.captures[*existing];
        // A transitive nested capture may require a stronger carrier than another child.
        // Move dominates because it owns the value; mutable reference dominates immutable
        // reference; copy remains independent only when no stronger requirement exists.
        if (prior.mode != capture.mode)
        {
            if (prior.mode == language::CaptureMode::Move ||
                capture.mode == language::CaptureMode::Move)
            {
                prior.mode = language::CaptureMode::Move;
            }
            else if (prior.mode == language::CaptureMode::MutableReference ||
                     capture.mode == language::CaptureMode::MutableReference)
            {
                prior.mode = language::CaptureMode::MutableReference;
            }
            else if (prior.mode == language::CaptureMode::Reference ||
                     capture.mode == language::CaptureMode::Reference)
            {
                prior.mode = language::CaptureMode::Reference;
            }
        }
        prior.ownsCapturedValue = prior.ownsCapturedValue || capture.ownsCapturedValue;
        prior.clearMovedSource = prior.clearMovedSource || capture.clearMovedSource;
        return *existing;
    }
    const std::size_t ordinal = target.captures.size();
    target.captures.push_back(std::move(capture));
    return ordinal;
}

MethodLambdaCapture ProjectMethodBodyModel::routeLambdaCaptureThroughParents(
    MethodLambdaCapture capture)
{
    switch (capture.kind)
    {
        case MethodLambdaCaptureKind::Local:
            capture.sourceKind = MethodLambdaCaptureSourceKind::OriginalLocal;
            capture.sourceOrdinal = capture.localOrdinal;
            break;
        case MethodLambdaCaptureKind::Parameter:
            capture.sourceKind = MethodLambdaCaptureSourceKind::OriginalParameter;
            capture.sourceOrdinal = capture.parameterOrdinal;
            break;
        case MethodLambdaCaptureKind::ThisReference:
            capture.sourceKind = MethodLambdaCaptureSourceKind::OriginalThis;
            capture.sourceOrdinal.reset();
            break;
    }
    // activeLambdaBindings_ contains the lexical ancestors after the current lambda has been
    // popped.  Walk from the outermost to the immediate parent.  Every intermediate lambda
    // either owns the local/parameter itself or receives a transitive capture slot, producing
    // a real closure chain for arbitrary nesting depth.
    for (MethodLambdaBinding* ancestor : activeLambdaBindings_)
    {
        if (ancestor == nullptr) continue;
        bool ownedByAncestor = false;
        if (capture.kind == MethodLambdaCaptureKind::Local &&
            capture.localOrdinal.has_value() && *capture.localOrdinal < locals_.size())
        {
            const MethodLocalSymbol& local = locals_[*capture.localOrdinal];
            ownedByAncestor = local.lambdaOrdinal == ancestor->lambdaOrdinal;
            if (ownedByAncestor && local.lambdaParameterOrdinal.has_value())
            {
                capture.sourceKind = MethodLambdaCaptureSourceKind::ParentParameter;
                capture.sourceOrdinal = local.lambdaParameterOrdinal;
            }
            else if (ownedByAncestor)
            {
                capture.sourceKind = MethodLambdaCaptureSourceKind::ParentLocal;
                capture.sourceOrdinal = capture.localOrdinal;
            }
        }
        if (ownedByAncestor) continue;
        const std::size_t parentCapture = addLambdaCapture(*ancestor, capture);
        capture.sourceKind = MethodLambdaCaptureSourceKind::ParentCapture;
        capture.sourceOrdinal = parentCapture;
    }
    return capture;
}

bool ProjectMethodBodyModel::validateLambdaLocalCaptureLifetime(
    const MethodNameBinding& name,
    const MethodLocalSymbol& local,
    language::CaptureMode mode,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool safe = true;
    // SHR-013: una VISTA capturada por un lambda es el mismo defecto que las dos de
    // abajo llegando por el tipo en vez de por el calificativo. El delegate se puede
    // guardar en un campo, devolver, o llamar horas despues; lo prestado no lo sabe.
    if (typeKeyIsViewLike(local.typeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "view local '" + local.name + "' of type '" + local.typeKey +
                "' cannot be captured by a lambda because the delegate may outlive what "
                "the view borrows from");
        safe = false;
    }
    if (local.variableLengthArrayRootLocalOrdinal.has_value())
    {
        const std::size_t root = *local.variableLengthArrayRootLocalOrdinal;
        const std::string rootName = root < locals_.size() ? locals_[root].name : local.name;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "pointer into variable-length array '" + rootName +
                "' cannot be captured by a lambda because the delegate may outlive its stack frame");
        safe = false;
    }
    if (local.isScope)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "scope object '" + local.name +
                "' cannot be captured by a lambda because it lives in the current frame");
        safe = false;
    }
    if (local.ownership == sema::OwnershipKind::Borrowed ||
        ((mode == language::CaptureMode::Copy ||
          mode == language::CaptureMode::Reference ||
          mode == language::CaptureMode::MutableReference) &&
         (local.isRaii || local.isMoveOnly || local.requiresValueDrop)))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            local.ownership == sema::OwnershipKind::Borrowed
                ? "borrowed local '" + local.name +
                    "' cannot be captured by a lambda because its source owner may not "
                    "outlive the closure"
                : local.isRaii
                    ? "RAII owner '" + local.name +
                        "' cannot be captured by a lambda without an explicit move capture"
                    : "move-only owner '" + local.name +
                        "' cannot be captured by a lambda without an explicit move capture");
        safe = false;
    }
    if (local.isReference)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "reference local '" + local.name +
                "' cannot be captured by a lambda because the delegate may outlive the referenced storage");
        safe = false;
    }
    // EN: Freeze 3.0 P10: a local that copies an exception parameter loan.
    // ES: Congelamiento 3.0 P10: un local que copia un prestamo de parametro de excepcion.
    if (local.loanedExceptionParameterOrdinal.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "the exception in '" + local.name + "' is a loan from the caller (it may be a caught "
                "exception) and cannot be captured by a lambda");
        safe = false;
    }
    return safe;
}

bool ProjectMethodBodyModel::validateLambdaLocalCapture(
    const MethodNameBinding& name,
    const MethodLocalSymbol& local,
    language::CaptureMode mode,
    const MethodLambdaBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool safe = validateLambdaLocalCaptureLifetime(name, local, mode, diagnostics);
    if (mode != language::CaptureMode::Move) return safe;
    if (local.isPinned)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "cannot move-capture pinned owner '" + local.name + "'");
        safe = false;
    }
    if (movedRaiiLocals_.contains(*name.localOrdinal) ||
        maybeMovedRaiiLocals_.contains(*name.localOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "cannot move-capture already moved or possibly moved local '" + local.name + "'");
        safe = false;
    }
    if (hasActiveBorrowOfOwnerLocal(*name.localOrdinal, binding.scopeOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "cannot move-capture owner '" + local.name + "' while a borrow is alive");
        safe = false;
    }
    if (local.isRaii || local.isMoveOnly || local.requiresValueDrop)
    {
        maybeMovedRaiiLocals_.erase(*name.localOrdinal);
        movedRaiiLocals_.insert(*name.localOrdinal);
        invalidateHistoricalBorrowsOfOwnerLocal(*name.localOrdinal);
    }
    return safe;
}

bool ProjectMethodBodyModel::captureLambdaLocal(
    const MethodNameBinding& name,
    const MethodLocalSymbol& local,
    language::CaptureMode mode,
    MethodLambdaBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const bool safe = validateLambdaLocalCapture(name, local, mode, binding, diagnostics);
    const bool ownsCapturedValue = mode == language::CaptureMode::Move &&
        (local.isRaii || local.requiresValueDrop ||
         local.ownership == sema::OwnershipKind::Owned);
    const bool clearMovedSource = mode == language::CaptureMode::Move &&
        (ownsCapturedValue || local.isMoveOnly);
    MethodLambdaCapture captureDescription{
        MethodLambdaCaptureKind::Local, local.name, local.typeKey,
        *name.localOrdinal, std::nullopt,
        MethodLambdaCaptureSourceKind::OriginalLocal, *name.localOrdinal,
        mode, ownsCapturedValue, clearMovedSource};
    captureDescription = routeLambdaCaptureThroughParents(std::move(captureDescription));
    const std::size_t capture = addLambdaCapture(binding, std::move(captureDescription));
    lambdaValueBindings_.push_back(MethodLambdaValueBinding{
        name.syntax, binding.lambdaOrdinal, false, false, capture, local.typeKey});
    return safe;
}

bool ProjectMethodBodyModel::validateLambdaParameterCaptureLifetime(
    const MethodNameBinding& name,
    const MethodParameterSymbol& parameter,
    language::CaptureMode mode,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    bool safe = true;
    // SHR-013: y lo mismo para un PARAMETRO de tipo vista. Un parametro sobrevive a la
    // llamada, pero no al delegate que lo captura.
    if (typeKeyIsViewLike(parameter.typeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "view parameter '" + parameter.name + "' of type '" + parameter.typeKey +
                "' cannot be captured by a lambda because the delegate may outlive what "
                "the view borrows from");
        safe = false;
    }
    if (parameter.ownership == sema::OwnershipKind::Borrowed ||
        (parameter.ownership == sema::OwnershipKind::Owned &&
         mode != language::CaptureMode::Move) ||
        (mode == language::CaptureMode::Copy && memberModel_ != nullptr &&
         (memberModel_->typeIsMoveOnly(parameter.typeKey) ||
          memberModel_->typeRequiresValueDrop(parameter.typeKey))))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            parameter.ownership == sema::OwnershipKind::Borrowed
                ? "borrowed parameter '" + parameter.name +
                    "' cannot be captured by a lambda because its source owner may not "
                    "outlive the closure"
                : "owned parameter '" + parameter.name +
                    "' cannot be captured by a lambda without an explicit move capture");
        safe = false;
    }
    if (parameter.passKind != ParameterPassKind::Value)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "reference parameter '" + parameter.name +
                "' cannot be captured by a lambda because the delegate may outlive the referenced storage");
        safe = false;
    }
    // EN: Freeze 3.0 P10: an exception parameter is a loan from the caller; a closure may outlive it.
    // ES: Congelamiento 3.0 P10: un parametro de excepcion es un prestamo del llamador; un closure
    //     puede sobrevivirlo.
    if (parameterIsExceptionLoan(parameter))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "the exception in '" + parameter.name + "' is a loan from the caller (it may be a caught "
                "exception) and cannot be captured by a lambda");
        safe = false;
    }
    return safe;
}

bool ProjectMethodBodyModel::validateLambdaParameterCapture(
    const MethodNameBinding& name,
    const MethodParameterSymbol& parameter,
    language::CaptureMode mode,
    const MethodLambdaBinding& binding,
    bool& moveTracked,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool safe = validateLambdaParameterCaptureLifetime(name, parameter, mode, diagnostics);
    moveTracked = parameter.ownership == sema::OwnershipKind::Owned ||
        (parameter.passKind == ParameterPassKind::Value && memberModel_ != nullptr &&
         (memberModel_->typeIsMoveOnly(parameter.typeKey) ||
          memberModel_->typeRequiresValueDrop(parameter.typeKey)));
    if (mode != language::CaptureMode::Move || !moveTracked) return safe;
    if (movedOwnedParameters_.contains(*name.parameterOrdinal) ||
        maybeMovedOwnedParameters_.contains(*name.parameterOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "cannot move-capture already moved or possibly moved parameter '" +
                parameter.name + "'");
        safe = false;
    }
    if (hasActiveBorrowOfOwnedParameter(*name.parameterOrdinal, binding.scopeOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "cannot move-capture parameter '" + parameter.name +
                "' while a borrow is alive");
        safe = false;
    }
    maybeMovedOwnedParameters_.erase(*name.parameterOrdinal);
    movedOwnedParameters_.insert(*name.parameterOrdinal);
    invalidateHistoricalBorrowsOfOwnedParameter(*name.parameterOrdinal);
    return safe;
}

bool ProjectMethodBodyModel::captureLambdaParameter(
    const MethodNameBinding& name,
    const MethodParameterSymbol& parameter,
    language::CaptureMode mode,
    MethodLambdaBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool moveTracked = false;
    const bool safe = validateLambdaParameterCapture(
        name, parameter, mode, binding, moveTracked, diagnostics);
    const bool ownsCapturedValue = mode == language::CaptureMode::Move &&
        (parameter.ownership == sema::OwnershipKind::Owned ||
         (memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(parameter.typeKey)));
    const bool clearMovedSource = mode == language::CaptureMode::Move &&
        (ownsCapturedValue || moveTracked);
    MethodLambdaCapture captureDescription{
        MethodLambdaCaptureKind::Parameter, parameter.name, parameter.typeKey,
        std::nullopt, *name.parameterOrdinal,
        MethodLambdaCaptureSourceKind::OriginalParameter, *name.parameterOrdinal,
        mode, ownsCapturedValue, clearMovedSource};
    captureDescription = routeLambdaCaptureThroughParents(std::move(captureDescription));
    const std::size_t capture = addLambdaCapture(binding, std::move(captureDescription));
    lambdaValueBindings_.push_back(MethodLambdaValueBinding{
        name.syntax, binding.lambdaOrdinal, false, false, capture, parameter.typeKey});
    return safe;
}

bool ProjectMethodBodyModel::methodNameBindingNeedsThis(
    const MethodNameBinding& name) const noexcept
{
    return name.kind == MethodNameBindingKind::ThisReference ||
        name.kind == MethodNameBindingKind::BaseReference ||
        name.kind == MethodNameBindingKind::ImplicitField ||
        name.kind == MethodNameBindingKind::ImplicitMethodGroup ||
        name.kind == MethodNameBindingKind::ExplicitThisField ||
        name.kind == MethodNameBindingKind::ExplicitThisMethodGroup ||
        name.kind == MethodNameBindingKind::ExplicitBaseField ||
        name.kind == MethodNameBindingKind::ExplicitBaseMethodGroup ||
        name.kind == MethodNameBindingKind::ImplicitEvent ||
        name.kind == MethodNameBindingKind::ExplicitThisEvent;
}

bool ProjectMethodBodyModel::captureLambdaThis(
    const MethodNameBinding& name,
    language::CaptureMode mode,
    std::size_t methodMemberOrdinal,
    MethodLambdaBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (binding.explicitCaptureList &&
        (mode == language::CaptureMode::Copy || mode == language::CaptureMode::Move))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "'this' is a borrowed receiver and must be captured explicitly as 'ref this' "
            "or 'ref mut this'; copy/move would invent receiver ownership");
        return false;
    }
    const ClassMemberSymbol* owner = methodSymbol(methodMemberOrdinal);
    if (mode == language::CaptureMode::MutableReference &&
        owner != nullptr && owner->isReadonly)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            name.syntax->range(),
            "readonly method cannot capture 'this' as 'ref mut this'");
        return false;
    }
    const std::string typeKey = implicitThisTypeKey(methodMemberOrdinal);
    MethodLambdaCapture captureDescription{
        MethodLambdaCaptureKind::ThisReference, "this", typeKey,
        std::nullopt, std::nullopt,
        MethodLambdaCaptureSourceKind::OriginalThis, std::nullopt,
        mode, false, false};
    captureDescription = routeLambdaCaptureThroughParents(std::move(captureDescription));
    const std::size_t capture = addLambdaCapture(binding, std::move(captureDescription));
    if (name.kind == MethodNameBindingKind::ThisReference ||
        name.kind == MethodNameBindingKind::BaseReference)
    {
        lambdaValueBindings_.push_back(MethodLambdaValueBinding{
            name.syntax, binding.lambdaOrdinal, false, false, capture, typeKey});
    }
    return true;
}

void ProjectMethodBodyModel::collectLambdaNameCapture(
    const MethodNameBinding& name,
    const std::unordered_map<std::string, language::CaptureMode>& explicitModes,
    std::unordered_set<std::string>& reportedImplicitCaptures,
    std::size_t methodMemberOrdinal,
    MethodLambdaBinding& binding,
    bool& safe,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (name.kind == MethodNameBindingKind::Local && name.localOrdinal.has_value() &&
        *name.localOrdinal < locals_.size())
    {
        const MethodLocalSymbol& local = locals_[*name.localOrdinal];
        if (local.isStatic)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                name.syntax->range(),
                "static local '" + local.name +
                    "' cannot be captured; its module storage is not closure-owned");
            safe = false;
            return;
        }
        if (local.lambdaOrdinal == binding.lambdaOrdinal)
        {
            if (local.lambdaParameterOrdinal.has_value())
            {
                lambdaValueBindings_.push_back(MethodLambdaValueBinding{
                    name.syntax, binding.lambdaOrdinal, true,
                    *local.lambdaParameterOrdinal < binding.parameterByReference.size() &&
                        binding.parameterByReference[*local.lambdaParameterOrdinal],
                    *local.lambdaParameterOrdinal, local.typeKey});
            }
            return;
        }
        const std::optional<language::CaptureMode> mode = requestedLambdaCaptureMode(
            binding, explicitModes, reportedImplicitCaptures,
            local.name, name.syntax->range(), safe, diagnostics);
        if (!mode.has_value()) return;
        const bool captureSafe = captureLambdaLocal(name, local, *mode, binding, diagnostics);
        safe = captureSafe && safe;
        return;
    }
    if (name.kind == MethodNameBindingKind::Parameter && name.parameterOrdinal.has_value())
    {
        const ClassMemberSymbol* owner = methodSymbol(methodMemberOrdinal);
        if (owner == nullptr || *name.parameterOrdinal >= owner->parameters.size()) return;
        const MethodParameterSymbol& parameter = owner->parameters[*name.parameterOrdinal];
        const std::optional<language::CaptureMode> mode = requestedLambdaCaptureMode(
            binding, explicitModes, reportedImplicitCaptures,
            parameter.name, name.syntax->range(), safe, diagnostics);
        if (!mode.has_value()) return;
        const bool captureSafe = captureLambdaParameter(
            name, parameter, *mode, binding, diagnostics);
        safe = captureSafe && safe;
        return;
    }
    if (!methodNameBindingNeedsThis(name)) return;
    const std::optional<language::CaptureMode> mode = requestedLambdaCaptureMode(
        binding, explicitModes, reportedImplicitCaptures,
        "this", name.syntax->range(), safe, diagnostics);
    if (!mode.has_value()) return;
    const bool captureSafe = captureLambdaThis(
        name, *mode, methodMemberOrdinal, binding, diagnostics);
    safe = captureSafe && safe;
}

bool ProjectMethodBodyModel::collectLambdaCaptures(
    MethodLambdaBinding& binding,
    std::size_t nameBindingBegin,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool safe = true;
    const std::unordered_map<std::string, language::CaptureMode> explicitModes =
        explicitLambdaCaptureModes(binding);
    std::unordered_set<std::string> reportedImplicitCaptures;
    for (std::size_t index = nameBindingBegin; index < nameBindings_.size(); ++index)
    {
        const auto owner = lambdaByNameBindingOrdinal_.find(index);
        if (owner == lambdaByNameBindingOrdinal_.end() ||
            owner->second != binding.lambdaOrdinal)
        {
            continue;
        }
        const MethodNameBinding& name = nameBindings_[index];
        if (name.methodMemberOrdinal != methodMemberOrdinal || name.syntax == nullptr) continue;
        collectLambdaNameCapture(
            name, explicitModes, reportedImplicitCaptures,
            methodMemberOrdinal, binding, safe, diagnostics);
    }
    return safe;
}

bool ProjectMethodBodyModel::resolveLambdaDelegateSignature(
    const ast::AstNode* syntax,
    std::string_view delegateTypeKey,
    bool inferAnonymous,
    std::string& returnType,
    std::vector<std::string>& parameterTypes,
    std::size_t& delegateOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (inferAnonymous)
    {
        for (const ast::AstNode* parameter : syntax->children())
        {
            if (parameter == nullptr || parameter->typeSyntax() == nullptr)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    parameter != nullptr ? parameter->range() : syntax->range(),
                    "an unconverted lambda requires an explicit type for every parameter");
                return false;
            }
            if (!parameter->auxiliaryText().empty())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    parameter->range(),
                    "an unconverted lambda parameter must be passed by value; "
                    "ref/out/in require a target function type");
                return false;
            }
            parameterTypes.push_back(
                memberModel_->canonicalTypeKey(parameter->typeSyntax(), *nameLookup_));
        }
        if (syntax->typeSyntax() != nullptr)
        {
            returnType = memberModel_->canonicalTypeKey(syntax->typeSyntax(), *nameLookup_);
        }
    }
    else if (!delegateSignature(delegateTypeKey, returnType, parameterTypes, delegateOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "Stage 43 lambda target must be a delegate type");
        return false;
    }
    if (syntax->children().size() == parameterTypes.size()) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidArgumentCount,
        diagnostics::DiagnosticSeverity::Error,
        syntax->range(),
        "lambda parameter count does not match target delegate signature");
    return false;
}

MethodLambdaBinding ProjectMethodBodyModel::createLambdaBinding(
    const ast::AstNode* syntax,
    std::string_view delegateTypeKey,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    std::size_t lambdaOrdinal,
    std::size_t delegateOrdinal,
    bool inferAnonymous,
    std::string_view returnType,
    const std::vector<std::string>& parameterTypes) const
{
    MethodLambdaBinding binding;
    binding.syntax = syntax;
    binding.lambdaOrdinal = lambdaOrdinal;
    binding.parentLambdaOrdinal = currentLambdaOrdinal_;
    binding.ownerMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.delegateTypeKey = std::string(delegateTypeKey);
    binding.delegateTypeOrdinal = delegateOrdinal;
    binding.returnTypeKey = std::string(returnType);
    binding.errorTypeKey = delegateErrorTypeKey(delegateTypeKey);
    binding.parameterTypeKeys = parameterTypes;
    binding.blockBody = syntax->flag();
    binding.anonymousNominal = inferAnonymous;
    language::AnonymousLambdaType anonymousIdentity;
    anonymousIdentity.ownerOrdinal = methodMemberOrdinal;
    anonymousIdentity.lambdaOrdinal = lambdaOrdinal;
    binding.generatedName = language::anonymousLambdaGeneratedName(anonymousIdentity);
    return binding;
}

std::vector<const ast::AstNode*> ProjectMethodBodyModel::delegateLambdaParameters(
    std::size_t delegateOrdinal) const
{
    std::vector<const ast::AstNode*> delegateParameters;
    if (nameLookup_ == nullptr || delegateOrdinal >= nameLookup_->namedTypes().size())
    {
        return delegateParameters;
    }
    const ast::AstNode* delegateSyntax = nameLookup_->namedTypes()[delegateOrdinal].syntax;
    if (delegateSyntax == nullptr) return delegateParameters;
    for (const ast::AstNode* child : delegateSyntax->children())
    {
        if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration)
        {
            delegateParameters.push_back(child);
        }
    }
    return delegateParameters;
}

std::string ProjectMethodBodyModel::lambdaParameterValueType(
    std::string_view parameterType,
    bool byReference) const
{
    return byReference && !parameterType.empty() && parameterType.back() == '*'
        ? std::string(parameterType.substr(0, parameterType.size() - 1))
        : std::string(parameterType);
}

bool ProjectMethodBodyModel::bindLambdaParameter(
    const ast::AstNode* syntax,
    const ast::AstNode* parameterSyntax,
    const ast::AstNode* delegateParameter,
    std::string_view parameterType,
    std::size_t parameterOrdinal,
    std::size_t methodMemberOrdinal,
    std::size_t lambdaScope,
    bool inferAnonymous,
    std::unordered_set<std::string>& parameterNames,
    MethodLambdaBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::string name = parameterSyntax != nullptr
        ? parameterSyntax->text() : std::string{};
    if (name.empty() || !parameterNames.insert(name).second)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            parameterSyntax != nullptr ? parameterSyntax->range() : syntax->range(),
            "duplicate lambda parameter '" + name + "'");
        return false;
    }
    // E11 typed lambdas.  An omitted type is contextual; a written type is a contract and
    // must match the target delegate exactly.  ref/out/in are represented as pointer types
    // by the parser, but their spelling still matters: accepting `ref` for an `out` target
    // would make definite-assignment and caller guarantees disagree even though both have
    // the same machine representation.
    const std::string lambdaPass = parameterSyntax != nullptr
        ? parameterSyntax->auxiliaryText() : std::string{};
    const std::string delegatePass = delegateParameter != nullptr
        ? delegateParameter->auxiliaryText() : std::string{};
    if ((!lambdaPass.empty() || !delegatePass.empty()) && lambdaPass != delegatePass)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            parameterSyntax != nullptr ? parameterSyntax->range() : syntax->range(),
            inferAnonymous
                ? "an unconverted lambda parameter must be passed by value"
                : "lambda parameter '" + name + "' pass mode does not match target delegate");
        return false;
    }
    if (parameterSyntax != nullptr && parameterSyntax->typeSyntax() != nullptr)
    {
        const std::string explicitType =
            memberModel_->canonicalTypeKey(parameterSyntax->typeSyntax(), *nameLookup_);
        if (explicitType != parameterType)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                parameterSyntax->range(),
                "lambda parameter '" + name + "' has type '" + explicitType +
                    "' but target delegate requires '" + std::string(parameterType) + "'");
            return false;
        }
    }
    const bool byReference = !delegatePass.empty();
    const std::string valueType = lambdaParameterValueType(parameterType, byReference);
    binding.parameterNames.push_back(name);
    binding.parameterValueTypeKeys.push_back(valueType);
    binding.parameterByReference.push_back(byReference);
    MethodLocalSymbol local;
    local.methodMemberOrdinal = methodMemberOrdinal;
    local.scopeOrdinal = lambdaScope;
    local.declarationOrdinal = scopes_[lambdaScope].localOrdinals.size();
    const ClassMemberSymbol* owner = methodSymbol(methodMemberOrdinal);
    if (owner != nullptr) local.fileId = owner->fileId;
    local.range = parameterSyntax != nullptr ? parameterSyntax->range() : syntax->range();
    local.syntax = parameterSyntax;
    local.name = name;
    local.typeKey = valueType;
    local.lambdaOrdinal = binding.lambdaOrdinal;
    local.lambdaParameterOrdinal = parameterOrdinal;
    const std::size_t localOrdinal = locals_.size();
    locals_.push_back(std::move(local));
    scopes_[lambdaScope].localOrdinals.push_back(localOrdinal);
    scopeNames_[lambdaScope].push_back(ScopeName{
        ScopeNameKind::Local, name, localOrdinal,
        parameterSyntax != nullptr ? parameterSyntax->range() : syntax->range()});
    // C7: a lambda parameter is bound by the caller, exactly like a method parameter.
    assignedLocals_.insert(localOrdinal);
    return true;
}

bool ProjectMethodBodyModel::bindLambdaParameters(
    const ast::AstNode* syntax,
    std::span<const ast::AstNode* const> delegateParameters,
    const std::vector<std::string>& parameterTypes,
    std::size_t methodMemberOrdinal,
    std::size_t lambdaScope,
    bool inferAnonymous,
    MethodLambdaBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::unordered_set<std::string> parameterNames;
    for (std::size_t index = 0; index < syntax->children().size(); ++index)
    {
        const ast::AstNode* delegateParameter =
            index < delegateParameters.size() ? delegateParameters[index] : nullptr;
        if (!bindLambdaParameter(
                syntax, syntax->children()[index], delegateParameter, parameterTypes[index], index,
                methodMemberOrdinal, lambdaScope, inferAnonymous,
                parameterNames, binding, diagnostics))
        {
            return false;
        }
    }
    return true;
}

void ProjectMethodBodyModel::bindLocalFunctionSelf(
    const ast::AstNode* syntax,
    std::span<const std::string> parameterTypes,
    std::string_view returnType,
    std::size_t methodMemberOrdinal,
    std::size_t lambdaScope,
    const MethodLambdaBinding& binding)
{
    // NS304-T04-F06: recursion de funcion local. El lambda lleva "local-function:<name>"; registra
    // <name> en el scope DEL LAMBDA (no en el enclosing) con su fn* para que el cuerpo se
    // auto-referencie sin que el lambda lo trate como captura del entorno.
    if (!syntax->auxiliaryText().starts_with("local-function:")) return;
    const std::string selfName = syntax->auxiliaryText().substr(
        std::string_view("local-function:").size());
    if (selfName.empty() || returnType.empty()) return;
    language::AnonymousLambdaType selfType;
    selfType.ownerOrdinal = methodMemberOrdinal;
    selfType.lambdaOrdinal = binding.lambdaOrdinal;
    selfType.signature.parameters.assign(parameterTypes.begin(), parameterTypes.end());
    selfType.signature.result = returnType;
    selfType.signature.carrier = language::FunctionCarrier::Pointer;
    const std::string selfTypeKey = language::anonymousLambdaTypeIdentity(selfType);
    MethodLocalSymbol selfLocal;
    selfLocal.methodMemberOrdinal = methodMemberOrdinal;
    selfLocal.scopeOrdinal = lambdaScope;
    selfLocal.name = selfName;
    selfLocal.typeKey = selfTypeKey;
    selfLocal.range = syntax->range();
    selfLocal.syntax = syntax;
    selfLocal.lambdaOrdinal = binding.lambdaOrdinal;
    const std::size_t selfOrdinal = locals_.size();
    locals_.push_back(std::move(selfLocal));
    scopes_[lambdaScope].localOrdinals.push_back(selfOrdinal);
    scopeNames_[lambdaScope].push_back(ScopeName{
        ScopeNameKind::Local, selfName, selfOrdinal, syntax->range()});
    assignedLocals_.insert(selfOrdinal);
}

bool ProjectMethodBodyModel::analyzeLambdaSyntaxBody(
    const ast::AstNode* syntax,
    std::size_t methodMemberOrdinal,
    std::size_t lambdaScope,
    bool inferAnonymous,
    std::string& returnType,
    MethodLambdaBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (syntax->first() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ExpectedExpression,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "lambda requires a body");
        return false;
    }
    if (binding.blockBody && inferAnonymous)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "an unconverted lambda with an inferred return type requires an expression body; "
            "convert a block-bodied lambda to an explicit fn* target");
        return false;
    }
    if (binding.blockBody)
    {
        analyzeBlock(syntax->first(), methodMemberOrdinal, lambdaScope, false, diagnostics);
        return !diagnostics.hasErrors();
    }
    ExpressionResult body = analyzeExpression(
        syntax->first(), methodMemberOrdinal, lambdaScope,
        diagnostics, isDelegateTypeKey(returnType));
    if (body.methodGroup && isDelegateTypeKey(returnType))
    {
        body = bindMethodGroupToDelegate(
            syntax->first(), body, returnType, methodMemberOrdinal, lambdaScope, diagnostics);
    }
    bool bodyValid = body.valid;
    if (body.valid && inferAnonymous && returnType.empty())
    {
        returnType = body.typeKey;
        binding.returnTypeKey = returnType;
    }
    else if (body.valid && inferAnonymous &&
        !classifyImplicit(body.typeKey, returnType, body.integerConstant).exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax->first()->range(),
            "recursive generic-lambda body type '" + body.typeKey +
                "' is not implicitly convertible to inferred return type '" +
                returnType + "'");
        bodyValid = false;
    }
    else if (body.valid && returnType == "builtin:void" && body.typeKey != "builtin:void")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax->first()->range(),
            "void delegate lambda expression body must have type void");
        bodyValid = false;
    }
    if (body.valid && !inferAnonymous && returnType != "builtin:void" &&
        !classifyImplicit(body.typeKey, returnType, body.integerConstant).exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax->first()->range(),
            "lambda expression body type '" + body.typeKey +
                "' is not implicitly convertible to delegate return type '" + returnType + "'");
        bodyValid = false;
    }
    return bodyValid;
}

bool ProjectMethodBodyModel::analyzeLambdaBodyAndCaptures(
    const ast::AstNode* syntax,
    std::size_t methodMemberOrdinal,
    std::size_t lambdaScope,
    bool inferAnonymous,
    std::string& returnType,
    MethodLambdaBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::size_t nameBindingBegin = nameBindings_.size();
    const OwnershipFlowSnapshot previousOwnershipFlow = captureOwnershipFlow();
    const std::optional<std::size_t> previousLambda = currentLambdaOrdinal_;
    const std::string previousReturn = currentLambdaReturnTypeKey_;
    const std::string previousError = currentLambdaErrorTypeKey_;
    currentLambdaOrdinal_ = binding.lambdaOrdinal;
    currentLambdaReturnTypeKey_ = returnType;
    currentLambdaErrorTypeKey_ = binding.errorTypeKey;
    activeLambdaBindings_.push_back(&binding);
    const std::size_t previousCatchDepth = currentCatchDepth_;
    currentCatchDepth_ = 0;
    // EN: Freeze 3.0 C-13: a lambda body runs later, outside the try blocks around its creation.
    // ES: Congelamiento 3.0 C-13: el cuerpo de una lambda corre despues, fuera de los try que la rodean.
    std::vector<std::vector<std::string>> previousErrorHandlers = std::move(activeErrorHandlers_);
    activeErrorHandlers_.clear();
    bool bodyValid = analyzeLambdaSyntaxBody(
        syntax, methodMemberOrdinal, lambdaScope,
        inferAnonymous, returnType, binding, diagnostics);
    activeErrorHandlers_ = std::move(previousErrorHandlers);
    currentLambdaOrdinal_ = previousLambda;
    currentLambdaReturnTypeKey_ = previousReturn;
    currentLambdaErrorTypeKey_ = previousError;
    currentCatchDepth_ = previousCatchDepth;
    activeLambdaBindings_.pop_back();
    restoreOwnershipFlow(previousOwnershipFlow);
    // Nested lambda bodies are analyzed while their parent body is being walked.  Claim only the
    // bindings not already claimed by a child, so the parent's capture scan cannot accidentally
    // treat names used solely inside a nested lambda as direct parent uses.
    for (std::size_t index = nameBindingBegin; index < nameBindings_.size(); ++index)
    {
        if (!lambdaByNameBindingOrdinal_.contains(index))
        {
            lambdaByNameBindingOrdinal_[index] = binding.lambdaOrdinal;
        }
    }
    if (!collectLambdaCaptures(binding, nameBindingBegin, methodMemberOrdinal, diagnostics))
    {
        bodyValid = false;
    }
    return bodyValid;
}

bool ProjectMethodBodyModel::validateStaticLambdaCaptures(
    const ast::AstNode* syntax,
    const MethodLambdaBinding& binding,
    bool bodyValid,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // NS304-T04-F05: static is an AST modifier, never a substring of unrelated textual metadata.
    // Local functions are represented as lambdas, so use their structural modifier and retain a
    // precise diagnostic that identifies the source construct.
    if (!bodyValid || binding.captures.empty() ||
        !syntax->hasModifier(TokenKind::KwStatic))
    {
        return bodyValid;
    }
    const bool localFunction = syntax->auxiliaryText().starts_with("local-function:");
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        syntax->range(),
        localFunction
            ? "a static local function cannot capture variables from the enclosing scope"
            : "a static lambda cannot capture variables from the enclosing scope");
    return false;
}

bool ProjectMethodBodyModel::computeLambdaClosureLayout(
    const ast::AstNode* syntax,
    MethodLambdaBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    std::vector<language::Capture> closureCaptures;
    closureCaptures.reserve(binding.captures.size());
    for (const MethodLambdaCapture& capture : binding.captures)
    {
        // The current native environment stores one cell pointer per capture. Copy/move own an
        // independent cell; ref/ref-mut share the source cell, but the environment slot layout
        // is deliberately identical and therefore stable at the MIR boundary.
        closureCaptures.push_back(language::Capture{
            capture.name, sizeof(void*), alignof(void*), capture.mode, true});
    }
    std::string closureLayoutError;
    if (!language::computeClosureLayout(
            closureCaptures, binding.closureLayout, closureLayoutError))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            closureLayoutError);
        binding.callableTrait = language::callableTrait(closureCaptures);
        return false;
    }
    binding.callableTrait = language::callableTrait(closureCaptures);
    return true;
}

bool ProjectMethodBodyModel::validateLambdaEnvironmentRestrictions(
    const ast::AstNode* syntax,
    std::string_view delegateTypeKey,
    std::size_t methodMemberOrdinal,
    bool inferAnonymous,
    const MethodLambdaBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    bool valid = true;
    const ClassMemberSymbol* lambdaOwner = methodSymbol(methodMemberOrdinal);
    if (lambdaOwner != nullptr && lambdaOwner->isNoHeap && !binding.captures.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "capturing closure allocates an environment and is not allowed in a noheap callable");
        valid = false;
    }
    if (isExternCDelegateTypeKey(delegateTypeKey) && !binding.captures.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "extern C lambda must not capture state; a C function pointer has no environment channel");
        valid = false;
    }
    if (inferAnonymous && !binding.captures.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "an unconverted lambda is a zero-size non-capturing value and cannot capture state; "
            "use an explicit closure target");
        valid = false;
    }
    return valid;
}

std::string ProjectMethodBodyModel::lambdaResultTypeKey(
    const ast::AstNode* syntax,
    std::span<const std::string> parameterTypes,
    std::string_view returnType,
    std::size_t methodMemberOrdinal,
    bool inferAnonymous,
    bool bodyValid,
    MethodLambdaBinding& binding) const
{
    std::string resultTypeKey = binding.delegateTypeKey;
    if (!inferAnonymous || !bodyValid) return resultTypeKey;
    language::AnonymousLambdaType anonymousIdentity;
    anonymousIdentity.ownerOrdinal = methodMemberOrdinal;
    anonymousIdentity.lambdaOrdinal = binding.lambdaOrdinal;
    anonymousIdentity.signature.parameters.assign(parameterTypes.begin(), parameterTypes.end());
    anonymousIdentity.signature.result = returnType;
    anonymousIdentity.signature.carrier = language::FunctionCarrier::Pointer;
    anonymousIdentity.signature.callingConvention = language::CallingConvention::NativeSharp;
    // NS304-T04-F02: efectos noheap/unsafe del lambda, guardados por el parser como
    // sufijo "effects:<efecto>" del auxiliaryText (tras la lista de capturas).
    if (const std::string_view auxiliary = syntax->auxiliaryText(); !auxiliary.empty())
    {
        const std::size_t effectsPos = auxiliary.rfind("effects:");
        if (effectsPos != std::string_view::npos)
        {
            const std::string_view effect = auxiliary.substr(effectsPos + 8U);
            if (effect.find("noheap") != std::string_view::npos)
            {
                anonymousIdentity.signature.isNoHeap = true;
            }
            if (effect.find("unsafe") != std::string_view::npos)
            {
                anonymousIdentity.signature.isUnsafe = true;
            }
        }
        const std::size_t conventionPos = auxiliary.rfind("calling-convention:");
        if (conventionPos != std::string_view::npos)
        {
            const std::string_view convention = auxiliary.substr(conventionPos + 19U);
            if (convention.find('C') != std::string_view::npos)
            {
                anonymousIdentity.signature.callingConvention = language::CallingConvention::C;
            }
        }
    }
    resultTypeKey = language::anonymousLambdaTypeIdentity(anonymousIdentity);
    binding.delegateTypeKey = resultTypeKey;
    return resultTypeKey;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::bindLambdaToDelegate(
    const ast::AstNode* syntax,
    std::string_view delegateTypeKey,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (syntax == nullptr || syntax->kind() != ast::AstNodeKind::LambdaExpression)
    {
        return result;
    }
    if (genericConstraintModel_ != nullptr &&
        !genericConstraintModel_->specializationSatisfiesConstraints(syntax))
    {
        genericConstraintModel_->reportSpecializationConstraintViolations(
            syntax, syntax->range(), diagnostics);
        return result;
    }
    const bool inferAnonymous = delegateTypeKey.empty();
    std::string returnType;
    std::vector<std::string> parameterTypes;
    std::size_t delegateOrdinal = std::numeric_limits<std::size_t>::max();
    if (!resolveLambdaDelegateSignature(
            syntax, delegateTypeKey, inferAnonymous, returnType,
            parameterTypes, delegateOrdinal, diagnostics))
    {
        return result;
    }
    const std::size_t lambdaOrdinal = nextLambdaOrdinal_++;
    MethodLambdaBinding binding = createLambdaBinding(
        syntax, delegateTypeKey, methodMemberOrdinal, scopeOrdinal,
        lambdaOrdinal, delegateOrdinal, inferAnonymous, returnType, parameterTypes);
    const std::vector<const ast::AstNode*> delegateParameters =
        delegateLambdaParameters(delegateOrdinal);
    const std::size_t lambdaScope = createScope(
        MethodScopeKind::Block, methodMemberOrdinal, scopeOrdinal);
    if (!bindLambdaParameters(
            syntax, delegateParameters, parameterTypes, methodMemberOrdinal,
            lambdaScope, inferAnonymous, binding, diagnostics))
    {
        return result;
    }
    bindLocalFunctionSelf(
        syntax, parameterTypes, returnType, methodMemberOrdinal, lambdaScope, binding);
    bool bodyValid = analyzeLambdaBodyAndCaptures(
        syntax, methodMemberOrdinal, lambdaScope,
        inferAnonymous, returnType, binding, diagnostics);
    bodyValid = validateStaticLambdaCaptures(
        syntax, binding, bodyValid, diagnostics);
    if (!computeLambdaClosureLayout(syntax, binding, diagnostics))
    {
        bodyValid = false;
    }
    if (!validateLambdaEnvironmentRestrictions(
            syntax, delegateTypeKey, methodMemberOrdinal,
            inferAnonymous, binding, diagnostics))
    {
        bodyValid = false;
    }
    std::string resultTypeKey = lambdaResultTypeKey(
        syntax, parameterTypes, returnType, methodMemberOrdinal,
        inferAnonymous, bodyValid, binding);
    const language::CallableTrait callableTrait = binding.callableTrait;
    lambdaBindings_.push_back(std::move(binding));

    if (!bodyValid) return result;
    result.typeKey = std::move(resultTypeKey);
    result.callableTrait = callableTrait;
    result.valid = true;
    result.assignable = false;
    result.ownershipFreshOwner =
        !inferAnonymous && !isExternCDelegateTypeKey(delegateTypeKey);
    recordExpressionType(syntax, methodMemberOrdinal, scopeOrdinal, result);
    return result;
}

std::vector<std::size_t> ProjectMethodBodyModel::compatibleDelegateMethods(
    const ExpressionResult& methodGroup,
    const std::vector<std::string>& parameterTypes,
    std::string_view returnType,
    std::string_view expectedErrorType) const
{
    std::vector<std::size_t> compatible;
    for (const std::size_t candidateOrdinal : methodGroup.methodCandidates)
    {
        const ClassMemberSymbol* candidatePointer = methodSymbol(candidateOrdinal);
        if (candidatePointer == nullptr) continue;
        const ClassMemberSymbol& candidate = *candidatePointer;
        if (candidate.kind != ClassMemberKind::Method ||
            candidate.parameters.size() != parameterTypes.size()) continue;
        if (candidate.declaredTypeKey != returnType ||
            candidate.declaredErrorTypeKey != expectedErrorType) continue;
        bool exact = true;
        for (std::size_t index = 0; index < parameterTypes.size(); ++index)
        {
            if (candidate.parameters[index].typeKey == parameterTypes[index]) continue;
            exact = false;
            break;
        }
        if (exact) compatible.push_back(candidateOrdinal);
    }
    return compatible;
}

bool ProjectMethodBodyModel::validateDelegateMethodTarget(
    const ast::AstNode* syntax,
    const ExpressionResult& methodGroup,
    std::string_view delegateTypeKey,
    std::size_t selected,
    const ClassMemberSymbol& target,
    bool& interfaceDispatch,
    bool& virtualDispatch,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (selected >= namespaceCallableBase_ && namespaceFunctionModel_ != nullptr)
    {
        const std::size_t functionOrdinal = selected - namespaceCallableBase_;
        if (functionOrdinal >= namespaceFunctionModel_->functions().size()) return false;
        const NamespaceFunctionSymbol& function =
            namespaceFunctionModel_->functions()[functionOrdinal];
        if (function.definitionSyntax == nullptr && !function.isExternC && !function.isPrecompiled)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::MissingFunctionDefinition,
                diagnostics::DiagnosticSeverity::Error,
                syntax != nullptr ? syntax->range() : SourceRange{},
                "namespace function '" + function.signatureKey +
                    "' is used as a delegate target but has no definition");
            return false;
        }
    }
    // Interop Foundation (callbacks C -> NativeSharp): an extern C delegate value IS a raw C
    // function pointer -- no cell, no receiver channel, no closure environment. Only a STATIC
    // method can convert; an instance method would need somewhere to smuggle `this` and a C
    // function pointer has none. The pointer produced is the internal function itself, whose
    // LLVM signature is the Win64 C ABI for every C-expressible type since Stage 40.2 --
    // certified against an MSVC-ABI host executing under Wine.
    if (isExternCDelegateTypeKey(delegateTypeKey) && !target.isStatic)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "extern C delegate requires a static method; a C function pointer has no receiver");
        return false;
    }
    interfaceDispatch = !target.isStatic && methodGroup.receiverTypeOrdinal.has_value() &&
        interfaceModel_ != nullptr && interfaceModel_->isInterface(*methodGroup.receiverTypeOrdinal);
    virtualDispatch = !target.isStatic && !interfaceDispatch && !methodGroup.baseReceiver &&
        (target.isAbstract || target.isVirtual || target.isOverride);
    if (target.isAbstract && !virtualDispatch && !interfaceDispatch)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "an abstract method value requires virtual or interface dispatch through a receiver");
        return false;
    }
    if (!target.isStatic && target.declaringTypeIsStruct)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "Stage 42 delegate instance targets must be class references; "
            "struct receiver lifetime capture is deferred");
        return false;
    }
    const bool receiverIsFreshObject = methodGroup.receiverSyntax != nullptr &&
        methodGroup.receiverSyntax->kind() == ast::AstNodeKind::ObjectCreationExpression;
    if (!target.isStatic && !methodGroup.methodGroupMovesReceiver &&
        (methodGroup.ownershipFreshOwner || receiverIsFreshObject))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "temporary receiver cannot be borrowed by a method value; use "
            "move(receiver.Method) to transfer receiver ownership explicitly");
        return false;
    }
    return true;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::recordMethodGroupDelegateBinding(
    const ast::AstNode* syntax,
    const ExpressionResult& methodGroup,
    std::string_view delegateTypeKey,
    std::size_t delegateOrdinal,
    std::size_t selected,
    const ClassMemberSymbol& target,
    bool interfaceDispatch,
    bool virtualDispatch,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    MethodDelegateBinding binding;
    binding.kind = MethodDelegateOperationKind::Create;
    binding.syntax = syntax;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.delegateTypeKey = std::string(delegateTypeKey);
    binding.delegateTypeOrdinal = delegateOrdinal;
    binding.selectedMemberOrdinal = selected;
    binding.staticTarget = target.isStatic;
    binding.virtualDispatch = virtualDispatch;
    binding.interfaceDispatch = interfaceDispatch;
    binding.ownsReceiver = methodGroup.methodGroupMovesReceiver;
    binding.receiverSourceLocalOrdinal = methodGroup.ownershipSourceLocalOrdinal;
    binding.receiverSourceParameterOrdinal = methodGroup.ownershipSourceParameterOrdinal;
    if (!target.isStatic)
    {
        binding.receiverSyntax = methodGroup.receiverSyntax;
        binding.receiverTypeKey = methodGroup.receiverTypeKey;
        binding.receiverTypeOrdinal = methodGroup.receiverTypeOrdinal;
        if (binding.receiverSyntax == nullptr && contextHasImplicitThis(methodMemberOrdinal))
        {
            binding.receiverTypeKey = implicitThisTypeKey(methodMemberOrdinal);
            binding.receiverTypeOrdinal = target.declaringTypeOrdinal;
        }
    }
    binding.resultTypeKey = std::string(delegateTypeKey);
    const bool ownsReceiver = binding.ownsReceiver;
    delegateBindings_.push_back(std::move(binding));
    ExpressionResult result;
    result.typeKey = std::string(delegateTypeKey);
    result.valid = true;
    result.assignable = false;
    if (!isExternCDelegateTypeKey(delegateTypeKey)) result.ownershipFreshOwner = true;
    if (!target.isStatic && !ownsReceiver)
    {
        result.boundMethodBorrowsReceiver = true;
        result.ownershipSourceLocalOrdinal = methodGroup.ownershipSourceLocalOrdinal;
        result.ownershipSourceParameterOrdinal = methodGroup.ownershipSourceParameterOrdinal;
        result.borrowedLocalOrdinal = methodGroup.borrowedLocalOrdinal;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::bindMethodGroupToDelegate(
    const ast::AstNode* syntax,
    const ExpressionResult& methodGroup,
    std::string_view delegateTypeKey,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (!methodGroup.methodGroup || memberModel_ == nullptr) return result;
    std::string returnType;
    std::vector<std::string> parameterTypes;
    std::size_t delegateOrdinal = 0;
    if (!delegateSignature(delegateTypeKey, returnType, parameterTypes, delegateOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "method group target is not a Stage 42 delegate type");
        return result;
    }
    const std::vector<std::size_t> compatible = compatibleDelegateMethods(
        methodGroup, parameterTypes, returnType, delegateErrorTypeKey(delegateTypeKey));
    if (compatible.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "no method in the method group exactly matches delegate signature '" +
                std::string(delegateTypeKey) + "' in Stage 42");
        return result;
    }
    if (compatible.size() != 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "ambiguous method group conversion to delegate type '" +
                std::string(delegateTypeKey) + "'");
        return result;
    }
    const std::size_t selected = compatible.front();
    const ClassMemberSymbol* target = methodSymbol(selected);
    if (target == nullptr) return result;
    bool interfaceDispatch = false;
    bool virtualDispatch = false;
    if (!validateDelegateMethodTarget(
            syntax, methodGroup, delegateTypeKey, selected, *target,
            interfaceDispatch, virtualDispatch, diagnostics)) return result;
    return recordMethodGroupDelegateBinding(
        syntax, methodGroup, delegateTypeKey, delegateOrdinal, selected, *target,
        interfaceDispatch, virtualDispatch, methodMemberOrdinal, scopeOrdinal);
}
std::optional<std::size_t> ProjectMethodBodyModel::eventBackingMember(
    std::size_t eventMemberOrdinal) const noexcept
{
    if (memberModel_ == nullptr || eventMemberOrdinal >= memberModel_->members().size()) return std::nullopt;
    const ClassMemberSymbol& event = memberModel_->members()[eventMemberOrdinal];
    if (event.kind != ClassMemberKind::Event) return std::nullopt;
    for (const std::size_t ordinal : memberModel_->membersOfType(event.declaringTypeOrdinal))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& candidate = memberModel_->members()[ordinal];
        if (candidate.isEventBacking && candidate.associatedName == event.name && candidate.isStatic == event.isStatic)
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

}
