#include "OwnershipArgumentClassification.h"
#include "OperatorClassification.h"
#include "IntegralConstantArithmetic.h"
#include "OwnershipFlowSnapshotQueries.h"

#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

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
std::optional<std::size_t> snapshotLocalIndex(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t globalLocalOrdinal) noexcept
{
    if (globalLocalOrdinal < snapshot.localBaseOrdinal ||
        globalLocalOrdinal >= snapshot.localCount)
    {
        return std::nullopt;
    }
    const std::size_t index = globalLocalOrdinal - snapshot.localBaseOrdinal;
    return index < snapshot.localFlags.size() ? std::optional<std::size_t>(index) : std::nullopt;
}

std::optional<std::size_t> snapshotStorageIndex(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t storageKey) noexcept
{
    if ((storageKey & 1U) != 0U)
    {
        const std::size_t parameterOrdinal = storageKey / 2U;
        return parameterOrdinal < snapshot.parameterFlags.size()
            ? std::optional<std::size_t>(parameterOrdinal)
            : std::nullopt;
    }

    const std::optional<std::size_t> localIndex = snapshotLocalIndex(
        snapshot,
        storageKey / 2U);
    return localIndex.has_value()
        ? std::optional<std::size_t>(snapshot.parameterFlags.size() + *localIndex)
        : std::nullopt;
}

OwnershipFlowStorageFacts* mutableSnapshotStorageFacts(
    OwnershipFlowSnapshot& snapshot,
    std::size_t storageKey) noexcept
{
    const std::optional<std::size_t> index = snapshotStorageIndex(snapshot, storageKey);
    return index.has_value() && *index < snapshot.storageFacts.size()
        ? &snapshot.storageFacts[*index]
        : nullptr;
}

bool snapshotLocalHasAnyFlag(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t localOrdinal,
    std::uint8_t flags) noexcept
{
    const std::optional<std::size_t> index = snapshotLocalIndex(snapshot, localOrdinal);
    return index.has_value() && (snapshot.localFlags[*index] & flags) != 0U;
}

bool snapshotParameterHasAnyFlag(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t parameterOrdinal,
    std::uint8_t flags) noexcept
{
    return parameterOrdinal < snapshot.parameterFlags.size() &&
        (snapshot.parameterFlags[parameterOrdinal] & flags) != 0U;
}
}
