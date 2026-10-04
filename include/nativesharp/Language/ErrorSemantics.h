#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
struct ResultProtocol final
{
    std::string successType;
    std::string errorType;
    bool hasIsSuccess = true;
    bool hasTakeSuccess = true;
    bool hasTakeError = true;
};
bool supportsQuestionPropagation(const ResultProtocol& protocol, std::string& error);
bool errorConversionIsExplicit(std::string_view source, std::string_view target, bool adapterDeclared) noexcept;

enum class ThrowAbi { ResultCarrier, DualReturn };
struct ThrowContract final
{
    std::string errorType;
    ThrowAbi abi = ThrowAbi::ResultCarrier;
    bool crossesCAbi = false;
    bool hasAdapter = false;
};
bool validateThrowContract(const ThrowContract& contract, std::string& error);

bool catchesAllDeclaredErrors(
    std::span<const std::string> declared,
    std::span<const std::string> caught,
    bool hasRethrow) noexcept;

bool neverCoercesTo(std::string_view targetType) noexcept;
std::optional<std::string> unifyLoopBreakTypes(std::span<const std::string> breakTypes);

enum class PanicProfile { Abort, NativeUnwind };
std::optional<PanicProfile> parsePanicProfile(std::string_view value) noexcept;
std::string_view panicProfileName(PanicProfile profile) noexcept;
struct PanicContract final
{
    bool runsCleanup = false;
    bool mayCrossFfi = false;
};
PanicContract panicContract(PanicProfile profile) noexcept;

enum class AssertionKind { Assert, Unreachable, Assume, Panic };
bool assertionRequiresUnsafe(AssertionKind kind) noexcept;
}
