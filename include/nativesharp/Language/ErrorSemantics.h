#pragma once

#include "nativesharp/Language/AssertionKind.h"
#include "nativesharp/Language/PanicContract.h"
#include "nativesharp/Language/PanicProfile.h"
#include "nativesharp/Language/ResultProtocol.h"
#include "nativesharp/Language/ThrowAbi.h"
#include "nativesharp/Language/ThrowContract.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
bool supportsQuestionPropagation(const ResultProtocol& protocol, std::string& error);
bool errorConversionIsExplicit(std::string_view source, std::string_view target, bool adapterDeclared) noexcept;

bool validateThrowContract(const ThrowContract& contract, std::string& error);

bool catchesAllDeclaredErrors(
    std::span<const std::string> declared,
    std::span<const std::string> caught,
    bool hasRethrow) noexcept;

bool neverCoercesTo(std::string_view targetType) noexcept;
std::optional<std::string> unifyLoopBreakTypes(std::span<const std::string> breakTypes);

std::optional<PanicProfile> parsePanicProfile(std::string_view value) noexcept;
std::string_view panicProfileName(PanicProfile profile) noexcept;
PanicContract panicContract(PanicProfile profile) noexcept;

bool assertionRequiresUnsafe(AssertionKind kind) noexcept;
}
