#include "nativesharp/Language/CallableSemantics.h"

#include <algorithm>

namespace nativesharp::language
{
namespace
{
std::size_t alignUp(std::size_t value, std::size_t alignment) noexcept
{
    return (value + alignment - 1U) / alignment * alignment;
}
}

std::string functionTypeIdentity(const FunctionType& type)
{
    std::string result = "fn[cc=" + std::to_string(static_cast<int>(type.callingConvention)) +
        ",var=" + std::to_string(type.variadic) + ",unsafe=" + std::to_string(type.isUnsafe) +
        ",nullable=" + std::to_string(type.nullable) + "](";
    for (const std::string& parameter : type.parameters)
        result += std::to_string(parameter.size()) + ":" + parameter + ";";
    result += ")->" + type.result + " throws " + type.errorType;
    return result;
}

bool functionTypesAssignable(const FunctionType& source, const FunctionType& target) noexcept
{
    return source.parameters == target.parameters && source.result == target.result &&
        source.callingConvention == target.callingConvention && source.errorType == target.errorType &&
        source.variadic == target.variadic && source.isUnsafe == target.isUnsafe &&
        (!source.nullable || target.nullable);
}

bool computeClosureLayout(
    std::span<const Capture> captures,
    ClosureLayout& layout,
    std::string& error)
{
    error.clear();
    layout = {};
    for (const Capture& capture : captures)
    {
        if (capture.name.empty() || capture.size == 0 || capture.alignment == 0 ||
            capture.alignment > 16 || (capture.alignment & (capture.alignment - 1U)) != 0)
        { error = "closure capture has invalid name, size, or alignment"; return false; }
        layout.alignment = std::max(layout.alignment, capture.alignment);
        layout.size = alignUp(layout.size, capture.alignment) + capture.size;
    }
    layout.size = captures.empty() ? 0 : alignUp(layout.size, layout.alignment);
    return true;
}

bool validateClosureEscape(
    std::span<const Capture> captures,
    bool escapes,
    std::string& error)
{
    error.clear();
    if (!escapes) return true;
    for (const Capture& capture : captures)
        if ((capture.mode == CaptureMode::Reference || capture.mode == CaptureMode::MutableReference) &&
            !capture.sourceOutlivesClosure)
        { error = "closure reference capture escapes its source lifetime: " + capture.name; return false; }
    return true;
}

CallableTrait callableTrait(std::span<const Capture> captures) noexcept
{
    if (std::ranges::any_of(captures, [](const Capture& item) { return item.mode == CaptureMode::Move; }))
        return CallableTrait::FnOnce;
    if (std::ranges::any_of(captures, [](const Capture& item) { return item.mode == CaptureMode::MutableReference; }))
        return CallableTrait::FnMut;
    return CallableTrait::Fn;
}

bool nonCapturingLambdaConvertsToPointer(std::span<const Capture> captures) noexcept
{
    return captures.empty();
}

bool genericLambdaUsesErasedDispatch() noexcept { return false; }

BoundMethodLayout windowsX64BoundMethodLayout() noexcept { return {16, 8}; }

bool validateMustTail(
    const FunctionType& caller,
    const FunctionType& callee,
    bool hasPendingCleanup,
    std::string& error)
{
    error.clear();
    if (hasPendingCleanup) { error = "must_tail call has pending cleanup"; return false; }
    if (!functionTypesAssignable(callee, caller))
    { error = "must_tail caller and callee ABIs are incompatible"; return false; }
    return true;
}
}
