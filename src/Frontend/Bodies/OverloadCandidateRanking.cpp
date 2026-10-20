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

bool ProjectMethodBodyModel::betterCandidate(
    const MethodCallCandidate& left,
    const MethodCallCandidate& right) const
{
    if (!left.viable || !right.viable || left.conversions.size() != right.conversions.size())
    {
        return false;
    }

    bool strictlyBetter = false;
    for (std::size_t index = 0; index < left.conversions.size(); ++index)
    {
        if (left.conversions[index].rank > right.conversions[index].rank)
        {
            return false;
        }
        if (left.conversions[index].rank < right.conversions[index].rank)
        {
            strictlyBetter = true;
        }
        if (left.conversions[index].rank == right.conversions[index].rank &&
            left.conversions[index].targetTypeKey != right.conversions[index].targetTypeKey)
        {
            // Equal broad categories still have a better conversion target. For example an int
            // tuple reaches both `(long,long)` and `(double,double)` through numeric widening,
            // but the former itself converts to the latter and not vice versa. Preserve the
            // ordinary overload partial order recursively for structural tuples and references.
            const bool leftReachesRight = classifyImplicit(
                left.conversions[index].targetTypeKey,
                right.conversions[index].targetTypeKey).exists();
            const bool rightReachesLeft = classifyImplicit(
                right.conversions[index].targetTypeKey,
                left.conversions[index].targetTypeKey).exists();
            if (rightReachesLeft && !leftReachesRight) return false;
            if (leftReachesRight && !rightReachesLeft) strictlyBetter = true;
        }
    }
    if (!strictlyBetter && left.defaultedParameterCount != right.defaultedParameterCount)
    {
        return left.defaultedParameterCount < right.defaultedParameterCount;
    }
    if (!strictlyBetter && left.isVariadicCandidate != right.isVariadicCandidate)
    {
        return !left.isVariadicCandidate;
    }
    if (!strictlyBetter &&
        left.usesExpandedParameterPack != right.usesExpandedParameterPack)
    {
        return !left.usesExpandedParameterPack;
    }
    if (!strictlyBetter && genericConstraintModel_ != nullptr)
    {
        const ClassMemberSymbol* leftSymbol = methodSymbol(left.memberOrdinal);
        const ClassMemberSymbol* rightSymbol = methodSymbol(right.memberOrdinal);
        if (leftSymbol != nullptr && rightSymbol != nullptr &&
            leftSymbol->isControlledSpecialization &&
            leftSymbol->genericTemplateOrdinal.has_value() &&
            rightSymbol->genericTemplateOrdinal.has_value())
        {
            return language::specializationStrictlyDominates(
                genericConstraintModel_->normalizedConstraintAtoms(
                    *leftSymbol->genericTemplateOrdinal),
                genericConstraintModel_->normalizedConstraintAtoms(
                    *rightSymbol->genericTemplateOrdinal));
        }
    }
    return strictlyBetter;
}

bool ProjectMethodBodyModel::deferCaptureAllowsLocal(
    std::string_view name, std::size_t localScopeOrdinal) const noexcept
{
    if (deferCaptureStack_.empty()) return true;
    const DeferCaptureContext& context = deferCaptureStack_.back();
    if (context.allowedNames.contains(std::string(name))) return true;

    std::optional<std::size_t> current = localScopeOrdinal;
    while (current.has_value() && *current < scopes_.size())
    {
        if (*current == context.boundaryScopeOrdinal) return true;
        current = scopes_[*current].parentScopeOrdinal;
    }
    return false;
}

bool ProjectMethodBodyModel::deferCaptureAllowsParameter(std::string_view name) const noexcept
{
    return deferCaptureStack_.empty() ||
        deferCaptureStack_.back().allowedNames.contains(std::string(name));
}

bool ProjectMethodBodyModel::deferCaptureAllowsThis() const noexcept
{
    return deferCaptureStack_.empty() || deferCaptureStack_.back().capturesThis;
}

}
