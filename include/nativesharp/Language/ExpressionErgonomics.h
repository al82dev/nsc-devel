#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
enum class ExpressionFeature
{
    NamedArguments,
    TypedParameterPacks,
    NullAwareOperators,
    RangeIndex,
    GeneralizedInitializers,
    StaticInterpolation,
    WithExpressions
};

struct ExpressionLoweringContract final
{
    ExpressionFeature feature = ExpressionFeature::NamedArguments;
    std::string_view spelling;
    bool sourceOrder = true;
    bool primaryEvaluatedOnce = true;
    bool mayAllocateImplicitly = false;
    bool preservesAbi = true;
};

const ExpressionLoweringContract* expressionLoweringContract(ExpressionFeature feature) noexcept;
bool expressionFeatureAvailable(ExpressionFeature feature) noexcept;

bool isTypedParameterPackType(std::string_view typeKey) noexcept;

enum class NullAwareOperandKind
{
    NotNullable,
    Pointer,
    Reference,
    StaticOptional
};

NullAwareOperandKind classifyNullAwareOperand(std::string_view typeKey) noexcept;
bool supportsNullAwareOperators(std::string_view typeKey) noexcept;

struct RangeEndpoint final
{
    std::size_t value = 0;
    bool fromEnd = false;
};

struct ResolvedRange final
{
    std::size_t start = 0;
    std::size_t end = 0;
};

std::optional<std::size_t> resolveRangeEndpoint(
    RangeEndpoint endpoint, std::size_t length) noexcept;
std::optional<ResolvedRange> resolveHalfOpenRange(
    std::optional<RangeEndpoint> start,
    std::optional<RangeEndpoint> end,
    std::size_t length) noexcept;

enum class InterpolationPartKind
{
    Literal,
    Hole
};

struct InterpolationPart final
{
    InterpolationPartKind kind = InterpolationPartKind::Literal;
    std::string text;
    std::string format;
    // Byte position of the trimmed hole expression inside the decoded interpolation template.
    // Literal parts leave these at zero. The parser uses this only to keep diagnostics for a
    // reparsed runtime hole close to the source spelling; semantic identity never depends on it.
    std::size_t expressionOffset = 0;
    std::size_t expressionLength = 0;
};

bool parseStaticInterpolation(
    std::string_view contents,
    std::vector<InterpolationPart>& parts,
    std::string& error);

// T01-F06. Interpolation parsing and handler selection are deliberately separate. The parser
// freezes literal/hole order; a compile-time handler decides which hole spellings it accepts and
// appends their representation. The handler is supplied by reference, so selecting a custom
// handler does not imply target-side allocation or a dependency on NativeSharp.Core.String.
class StaticInterpolationHandler
{
public:
    virtual ~StaticInterpolationHandler() = default;
    virtual bool appendLiteral(std::string_view text, std::string& error) = 0;
    virtual bool appendHole(const InterpolationPart& hole, std::string& error) = 0;
};

bool renderStaticInterpolation(
    std::span<const InterpolationPart> parts,
    StaticInterpolationHandler& handler,
    std::string& error);

// Built-in handler selected for an ordinary $"..." whose holes are bool/integer constants.
// Its result is emitted as immutable program data; it does not call String or observe a locale.
bool builtinStaticInterpolationHoleSupported(const InterpolationPart& part) noexcept;
bool renderBuiltinStaticInterpolation(
    std::span<const InterpolationPart> parts,
    std::string& result,
    std::string& error);
}
