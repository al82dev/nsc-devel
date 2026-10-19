#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/AnonymousLambdaType.h"
#include "nativesharp/Language/BoundMethodLayout.h"
#include "nativesharp/Language/CallableTrait.h"
#include "nativesharp/Language/CallingConvention.h"
#include "nativesharp/Language/Capture.h"
#include "nativesharp/Language/CaptureMode.h"
#include "nativesharp/Language/ClosureLayout.h"
#include "nativesharp/Language/FunctionCarrier.h"
#include "nativesharp/Language/FunctionType.h"
#include "nativesharp/Language/MethodValueKind.h"

namespace nativesharp::language
{
std::string functionTypeIdentity(const FunctionType& type);
std::optional<FunctionType> parseFunctionTypeIdentity(std::string_view identity);
bool isFunctionTypeIdentity(std::string_view identity) noexcept;
bool functionTypesAssignable(const FunctionType& source, const FunctionType& target) noexcept;
std::string_view functionCarrierName(FunctionCarrier carrier) noexcept;
std::string_view callingConventionName(CallingConvention convention) noexcept;

std::string anonymousLambdaTypeIdentity(const AnonymousLambdaType& type);
std::optional<AnonymousLambdaType> parseAnonymousLambdaTypeIdentity(std::string_view identity);
bool isAnonymousLambdaTypeIdentity(std::string_view identity) noexcept;
std::string anonymousLambdaGeneratedName(const AnonymousLambdaType& type);

bool computeClosureLayout(std::span<const Capture> captures, ClosureLayout& layout, std::string& error);
bool validateClosureEscape(std::span<const Capture> captures, bool escapes, std::string& error);

CallableTrait callableTrait(std::span<const Capture> captures) noexcept;
bool nonCapturingLambdaConvertsToPointer(std::span<const Capture> captures) noexcept;
bool genericLambdaUsesErasedDispatch() noexcept;

BoundMethodLayout windowsX64BoundMethodLayout() noexcept;

bool validateMustTail(
    const FunctionType& caller,
    const FunctionType& callee,
    bool hasPendingCleanup,
    std::string& error);
}
