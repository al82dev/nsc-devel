#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "nativesharp/Language/BuilderStorage.h"
#include "nativesharp/Language/EvaluationContract.h"
#include "nativesharp/Language/FusionDecision.h"
#include "nativesharp/Language/FusionRejection.h"
#include "nativesharp/Language/FusionRequest.h"
#include "nativesharp/Language/GeneratorFrame.h"
#include "nativesharp/Language/GeneratorFrameSlot.h"
#include "nativesharp/Language/GeneratorLocal.h"
#include "nativesharp/Language/GeneratorState.h"
#include "nativesharp/Language/GeneratorStorage.h"
#include "nativesharp/Language/GeneratorSuspension.h"
#include "nativesharp/Language/GeneratorYieldKind.h"
#include "nativesharp/Language/IterationMode.h"
#include "nativesharp/Language/IteratorProtocol.h"
#include "nativesharp/Language/LazyIntegerRange.h"
#include "nativesharp/Language/SourceMutation.h"
#include "nativesharp/Language/SynchronousGeneratorPlan.h"
#include "nativesharp/Language/TraversalCategory.h"

namespace nativesharp::language
{
bool validateIteratorProtocol(const IteratorProtocol& protocol, std::string& error);

// NS304-T06-F02: surface type key of a synchronous generator's opaque iterator.
// iteratorTypeKey("builtin:int") == "iterator:builtin:int"; the inverse strips the prefix.
std::string iteratorTypeKey(std::string_view itemTypeKey);

std::optional<std::string_view> iteratorInnerTypeKey(std::string_view typeKey) noexcept;

// NS304-T06-F07: Iterator<T,E> maps to iterator:<T>|<E> (the pipe separates item from error).
std::string iteratorTypeKey(std::string_view itemTypeKey, std::string_view errorTypeKey);

std::optional<std::string_view> iteratorErrorTypeKey(std::string_view typeKey) noexcept;

// NS304-T07-F01: surface type key of an async generator's opaque async iterator.
// asyncIteratorTypeKey("builtin:int", "type:E") == "async-iterator:builtin:int|type:E".
std::string asyncIteratorTypeKey(std::string_view itemTypeKey, std::string_view errorTypeKey);

std::optional<std::string_view> asyncIteratorInnerTypeKey(std::string_view typeKey) noexcept;

std::optional<std::string_view> asyncIteratorErrorTypeKey(std::string_view typeKey) noexcept;

std::optional<std::size_t> lazyRangeCount(const LazyIntegerRange& range, std::string& error) noexcept;
bool rangeMaterializesCollection() noexcept;

bool comprehensionHasImplicitAllocation(BuilderStorage storage) noexcept;

bool mutationAllowedDuringBorrowedIteration(
    const IteratorProtocol& protocol,
    SourceMutation mutation) noexcept;

std::optional<GeneratorFrame> computeGeneratorFrame(
    std::span<const GeneratorLocal> locals,
    bool hasSelfReference) noexcept;

// Freezes the state-machine ABI after semantic liveness/borrow analysis. The result is directly
// usable by HIR lowering: every suspension has a deterministic resume state and every local that
// crosses a yield owns one stable frame slot. Storage policy is explicit and never upgraded to a
// heap allocation by this planner.
std::optional<SynchronousGeneratorPlan> buildSynchronousGeneratorPlan(
    std::span<const GeneratorLocal> locals,
    std::span<const GeneratorSuspension> suspensions,
    GeneratorStorage storage,
    bool hasSelfReference,
    std::string& error) noexcept;

// Returns frame-slot ordinals, in destruction order, for cancellation at a suspended state.
// Locals that are not live in that state are never dropped.
std::vector<std::size_t> generatorCancellationDropOrder(
    const SynchronousGeneratorPlan& plan,
    std::size_t suspendedState) noexcept;

// Entry state 0 resumes to the first yield. Each yield resumes to the following yield and the
// final yield resumes to completedState. Completed/invalid states do not resume.
std::optional<std::size_t> generatorResumeState(
    const SynchronousGeneratorPlan& plan,
    std::size_t currentState) noexcept;

FusionDecision analyzeFusion(const FusionRequest& request) noexcept;
bool mayFuse(const EvaluationContract& source, const EvaluationContract& adapter) noexcept;
}
