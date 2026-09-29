#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

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

struct GeneratorLocal final { std::size_t size; std::size_t alignment; bool livesAcrossYield; };
struct GeneratorFrame final { std::size_t size; std::size_t alignment; bool requiresPin; };
std::optional<GeneratorFrame> computeGeneratorFrame(
    std::span<const GeneratorLocal> locals,
    bool hasSelfReference) noexcept;

enum class TraversalCategory { SinglePass, MultiPass };
struct EvaluationContract final
{
    TraversalCategory category = TraversalCategory::SinglePass;
    bool lazy = true;
    bool preservesLeftToRightEffects = true;
};
bool mayFuse(const EvaluationContract& source, const EvaluationContract& adapter) noexcept;
}
