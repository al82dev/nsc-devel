#include "nativesharp/Language/ErrorSemantics.h"

#include <algorithm>
#include <unordered_set>

namespace nativesharp::language
{
bool supportsQuestionPropagation(const ResultProtocol& protocol, std::string& error)
{
    error.clear();
    if (protocol.successType.empty() || protocol.errorType.empty() || !protocol.hasIsSuccess ||
        !protocol.hasTakeSuccess || !protocol.hasTakeError)
    {
        error = "? requires complete static Result protocol members";
        return false;
    }
    return true;
}

bool errorConversionIsExplicit(
    std::string_view source,
    std::string_view target,
    bool adapterDeclared) noexcept
{
    return source == target || adapterDeclared;
}

bool validateThrowContract(const ThrowContract& contract, std::string& error)
{
    error.clear();
    if (contract.errorType.empty()) { error = "throws requires an explicit error type"; return false; }
    if (contract.crossesCAbi && !contract.hasAdapter)
    { error = "typed throws cannot cross a C ABI boundary without an adapter"; return false; }
    return true;
}

bool catchesAllDeclaredErrors(
    std::span<const std::string> declared,
    std::span<const std::string> caught,
    bool hasRethrow) noexcept
{
    if (hasRethrow) return true;
    const std::unordered_set<std::string> handled(caught.begin(), caught.end());
    return std::ranges::all_of(declared, [&](const std::string& type) { return handled.contains(type); });
}

bool neverCoercesTo(std::string_view targetType) noexcept { return !targetType.empty(); }

std::optional<std::string> unifyLoopBreakTypes(std::span<const std::string> breakTypes)
{
    std::string result = "never";
    for (const std::string& type : breakTypes)
    {
        if (type == "never") continue;
        if (result == "never") result = type;
        else if (result != type) return std::nullopt;
    }
    return result;
}

std::optional<PanicProfile> parsePanicProfile(std::string_view value) noexcept
{
    if (value == "abort" || value == "Abort") return PanicProfile::Abort;
    if (value == "unwind-native" || value == "UnwindNative") return PanicProfile::NativeUnwind;
    return std::nullopt;
}

std::string_view panicProfileName(PanicProfile profile) noexcept
{
    return profile == PanicProfile::Abort ? "abort" : "unwind-native";
}

PanicContract panicContract(PanicProfile profile) noexcept
{
    return profile == PanicProfile::Abort ? PanicContract{false, false} : PanicContract{true, false};
}

bool assertionRequiresUnsafe(AssertionKind kind) noexcept { return kind == AssertionKind::Assume; }
}
