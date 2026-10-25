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
using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;

void ProjectMethodBodyModel::recordExpressionType(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    const ExpressionResult& result)
{
    if (expression == nullptr || !result.valid || result.methodGroup || isErrorType(result.typeKey))
    {
        return;
    }
    // NSC-LIMIT-010: se copian TAMBIEN ownership, transferencia y origen. Antes se quedaban en
    // ExpressionResult, y ProjectConstructionModel --que solo ve estos bindings-- no podia
    // distinguir `move(duenio)` de `duenio`. Los campos se nombran en vez de ir por posicion: la
    // lista crecio a nueve, y un inicializador agregado posicional de nueve campos es un error de
    // una linea esperando a que alguien inserte un miembro en medio.
    MethodExpressionTypeBinding binding = makeExpressionTypeBinding(
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        result);
    const std::size_t bindingOrdinal = expressionTypes_.size();
    expressionTypes_.push_back(std::move(binding));
    expressionTypeBindingIndex_.record(methodMemberOrdinal, expression, bindingOrdinal);
}

MethodExpressionTypeBinding ProjectMethodBodyModel::makeExpressionTypeBinding(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    const ExpressionResult& result) const
{
    MethodExpressionTypeBinding binding;
    binding.syntax = expression;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.typeKey = result.typeKey;
    if (result.addressable)
    {
        binding.valueCategory = result.ownership == sema::OwnershipKind::Borrowed
            ? sema::ValueCategory::BorrowedValue
            : sema::ValueCategory::Place;
    }
    else if (!result.ownershipTransfer && !result.ownershipFreshOwner &&
             typeKeyIsStruct(result.typeKey) &&
             (expression->kind() == ast::AstNodeKind::CallExpression ||
              expression->kind() == ast::AstNodeKind::ObjectCreationExpression ||
              expression->kind() == ast::AstNodeKind::AggregateInitializerExpression ||
              expression->kind() == ast::AstNodeKind::TupleExpression ||
              expression->kind() == ast::AstNodeKind::WithExpression))
    {
        binding.valueCategory = sema::ValueCategory::Temporary;
    }
    else
    {
        binding.valueCategory = sema::ValueCategory::Value;
    }
    binding.integerConstant = result.integerConstant;
    binding.pointerPointeeReadonly = result.pointerPointeeReadonly;
    binding.ownership = result.ownership;
    binding.ownershipTransfer = result.ownershipTransfer;
    binding.ownershipFreshOwner = result.ownershipFreshOwner;
    binding.ownershipSourceLocalOrdinal = result.ownershipSourceLocalOrdinal;
    binding.ownershipSourceParameterOrdinal = result.ownershipSourceParameterOrdinal;
    binding.ownsStorageDestination = result.ownsStorageDestination;
    binding.objectLifetimeOwnership = result.objectLifetimeOwnership;
    binding.storageOwnership = result.storageOwnership;
    binding.borrowedElementSlots = result.borrowedElementSlots;
    binding.flowProvenNonNull = result.flowProvenNonNull;
    binding.flowIntegerExcludesZero = result.flowIntegerExcludesZero;
    binding.flowIntegerMinimum = result.flowIntegerMinimum;
    binding.flowIntegerMaximum = result.flowIntegerMaximum;
    binding.flowActiveVariantCase = result.flowActiveVariantCase;
    binding.flowRuntimeCheckElided = result.flowRuntimeCheckElided;
    return binding;
}


void ProjectMethodBodyModel::recordCallBinding(MethodCallBinding binding)
{
    const std::size_t bindingOrdinal = calls_.size();
    const ast::AstNode* syntax = binding.syntax;
    const std::size_t callableOrdinal = binding.callerMethodMemberOrdinal;
    calls_.push_back(std::move(binding));
    callBindingIndex_.record(callableOrdinal, syntax, bindingOrdinal);
}

void ProjectMethodBodyModel::recordNameBinding(MethodNameBinding binding)
{
    const std::size_t bindingOrdinal = nameBindings_.size();
    const ast::AstNode* syntax = binding.syntax;
    const std::size_t callableOrdinal = binding.methodMemberOrdinal;
    nameBindings_.push_back(std::move(binding));
    nameBindingIndex_.record(callableOrdinal, syntax, bindingOrdinal);
}

void ProjectMethodBodyModel::reportPrevious(
    SourceRange range,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string message) const
{
    diagnostics.report(
        diagnostics::DiagnosticId::PreviousDeclaration,
        diagnostics::DiagnosticSeverity::Note,
        range,
        std::move(message));
}

}
