#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::language
{
enum class IterationMode { Value, Reference, MutableReference };
struct IteratorProtocol final
{
    std::string itemType;
    IterationMode mode = IterationMode::Value;
    bool hasMoveNext = true;
    bool hasCurrent = true;
    bool hasCleanup = true;
    bool lendsFromSource = false;
};
bool validateIteratorProtocol(const IteratorProtocol& protocol, std::string& error);

// NS304-T06-F02: surface type key of a synchronous generator's opaque iterator.
// iteratorTypeKey("builtin:int") == "iterator:builtin:int"; the inverse strips the prefix.
inline std::string iteratorTypeKey(std::string_view itemTypeKey)
{
    std::string key = "iterator:";
    key.append(itemTypeKey);
    return key;
}

inline std::optional<std::string_view> iteratorInnerTypeKey(std::string_view typeKey) noexcept
{
    constexpr std::string_view prefix = "iterator:";
    if (!typeKey.starts_with(prefix) || typeKey.ends_with('*')) return std::nullopt;
    return typeKey.substr(prefix.size());
}

// NS304-T07-F01: surface type key of an async generator's opaque async iterator.
// asyncIteratorTypeKey("builtin:int", "type:E") == "async-iterator:builtin:int|type:E".
inline std::string asyncIteratorTypeKey(std::string_view itemTypeKey, std::string_view errorTypeKey)
{
    std::string key = "async-iterator:";
    key.append(itemTypeKey);
    if (!errorTypeKey.empty())
    {
        key.push_back('|');
        key.append(errorTypeKey);
    }
    return key;
}

inline std::optional<std::string_view> asyncIteratorInnerTypeKey(std::string_view typeKey) noexcept
{
    constexpr std::string_view prefix = "async-iterator:";
    if (!typeKey.starts_with(prefix) || typeKey.ends_with('*')) return std::nullopt;
    const std::string_view rest = typeKey.substr(prefix.size());
    const std::size_t pipe = rest.find('|');
    return pipe == std::string_view::npos ? rest : rest.substr(0, pipe);
}

inline std::optional<std::string_view> asyncIteratorErrorTypeKey(std::string_view typeKey) noexcept
{
    constexpr std::string_view prefix = "async-iterator:";
    if (!typeKey.starts_with(prefix) || typeKey.ends_with('*')) return std::nullopt;
    const std::string_view rest = typeKey.substr(prefix.size());
    const std::size_t pipe = rest.find('|');
    if (pipe == std::string_view::npos) return std::nullopt;
    return rest.substr(pipe + 1);
}

struct LazyIntegerRange final
{
    std::int64_t begin = 0;
    std::int64_t end = 0;
    std::int64_t step = 1;
    bool inclusive = false;
};
std::optional<std::size_t> lazyRangeCount(const LazyIntegerRange& range, std::string& error) noexcept;
bool rangeMaterializesCollection() noexcept;

enum class BuilderStorage { Stack, Caller, HeapExplicit };
bool comprehensionHasImplicitAllocation(BuilderStorage storage) noexcept;

enum class SourceMutation { Element, Structural };
bool mutationAllowedDuringBorrowedIteration(
    const IteratorProtocol& protocol,
    SourceMutation mutation) noexcept;

struct GeneratorLocal final
{
    std::size_t size;
    std::size_t alignment;
    bool livesAcrossYield;
    bool requiresDrop = false;
    bool moveOnly = false;
    bool borrowed = false;
    // Inclusive suspension-state interval. Zero means the legacy frame-only query did not
    // provide state liveness and is accepted only by computeGeneratorFrame().
    std::size_t firstLiveState = 0;
    std::size_t lastLiveState = 0;
};
struct GeneratorFrame final { std::size_t size; std::size_t alignment; bool requiresPin; };
std::optional<GeneratorFrame> computeGeneratorFrame(
    std::span<const GeneratorLocal> locals,
    bool hasSelfReference) noexcept;

enum class GeneratorStorage { Stack, CallerProvided, Arena, ExplicitHeap };
enum class GeneratorYieldKind { Value, SharedReference, MutableReference };

struct GeneratorSuspension final
{
    // State zero is the entry state. Productive yields are numbered contiguously from one;
    // the completed state is suspension-count + one.
    std::size_t state = 0;
    std::string itemType;
    GeneratorYieldKind kind = GeneratorYieldKind::Value;
    std::size_t cleanupDepth = 0;
    // Reference yields may only lend storage owned by the frame or by a captured source whose
    // lifetime is carried by the frame. This marker is supplied after borrow analysis.
    bool lifetimeTiedToFrame = false;
};

struct GeneratorFrameSlot final
{
    std::size_t localIndex = 0;
    std::size_t offset = 0;
    std::size_t size = 0;
    std::size_t alignment = 1;
    std::size_t firstLiveState = 0;
    std::size_t lastLiveState = 0;
    bool requiresDrop = false;
    bool moveOnly = false;
    bool borrowed = false;
};

struct GeneratorState final
{
    std::size_t state = 0;
    std::size_t resumeState = 0;
    std::size_t cleanupDepth = 0;
    std::vector<std::size_t> liveSlots;
};

struct SynchronousGeneratorPlan final
{
    std::string itemType;
    GeneratorStorage storage = GeneratorStorage::Stack;
    GeneratorFrame frame{1, 1, false};
    std::vector<GeneratorFrameSlot> slots;
    std::vector<GeneratorState> states;
    std::size_t completedState = 1;
    bool yieldsBorrowedItems = false;
    bool hasImplicitHeapAllocation = false;
};

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

enum class TraversalCategory { SinglePass, MultiPass };
struct EvaluationContract final
{
    TraversalCategory category = TraversalCategory::SinglePass;
    bool lazy = true;
    bool preservesLeftToRightEffects = true;
    bool mayShortCircuit = false;
    bool repeatsElements = false;
    bool buffersElements = false;
    bool yieldsBorrowedItems = false;
};

enum class FusionRejection
{
    None,
    EagerBoundary,
    EffectReordering,
    TraversalUpgrade,
    RepeatedEvaluation,
    ShortCircuitLost,
    BorrowedItemBuffered
};

struct FusionRequest final
{
    EvaluationContract source;
    EvaluationContract adapter;
    // The optimizer states what the fused loop will actually preserve. These are proof facts,
    // not optimization wishes; a missing fact rejects fusion conservatively.
    bool fusedIsLazy = true;
    bool fusedPreservesLeftToRightEffects = true;
    bool fusedPreservesShortCircuit = true;
    bool fusedEvaluatesEachSourceAtMostOnce = true;
};

struct FusionDecision final
{
    bool allowed = false;
    FusionRejection rejection = FusionRejection::None;
    TraversalCategory resultCategory = TraversalCategory::SinglePass;
};

FusionDecision analyzeFusion(const FusionRequest& request) noexcept;
bool mayFuse(const EvaluationContract& source, const EvaluationContract& adapter) noexcept;
}
