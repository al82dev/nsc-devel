#include "nativesharp/Language/CallableSemantics.h"

#include <algorithm>
#include <charconv>

namespace nativesharp::language
{
namespace
{
std::size_t alignUp(std::size_t value, std::size_t alignment) noexcept
{
    return (value + alignment - 1U) / alignment * alignment;
}

void appendLengthPrefixed(std::string& output, std::string_view value)
{
    output += std::to_string(value.size());
    output.push_back('#');
    output += value;
}

bool consumeLiteral(std::string_view input, std::size_t& cursor, std::string_view literal) noexcept
{
    if (cursor + literal.size() > input.size() || input.substr(cursor, literal.size()) != literal)
        return false;
    cursor += literal.size();
    return true;
}

bool consumeLengthPrefixed(std::string_view input, std::size_t& cursor, std::string& value)
{
    if (cursor >= input.size()) return false;
    std::size_t length = 0;
    const char* first = input.data() + cursor;
    const char* last = input.data() + input.size();
    const auto parsed = std::from_chars(first, last, length);
    if (parsed.ec != std::errc{} || parsed.ptr == first || parsed.ptr == last ||
        *parsed.ptr != '#')
        return false;
    cursor = static_cast<std::size_t>(parsed.ptr - input.data()) + 1U;
    if (cursor + length > input.size()) return false;
    value.assign(input.substr(cursor, length));
    cursor += length;
    return true;
}

std::optional<FunctionCarrier> parseCarrier(std::string_view value) noexcept
{
    if (value == "ptr") return FunctionCarrier::Pointer;
    if (value == "ref") return FunctionCarrier::Reference;
    return std::nullopt;
}

std::optional<CallingConvention> parseConvention(std::string_view value) noexcept
{
    if (value == "native") return CallingConvention::NativeSharp;
    if (value == "c") return CallingConvention::C;
    if (value == "stdcall") return CallingConvention::StdCall;
    if (value == "fastcall") return CallingConvention::FastCall;
    if (value == "vectorcall") return CallingConvention::VectorCall;
    return std::nullopt;
}
}

std::string_view functionCarrierName(FunctionCarrier carrier) noexcept
{
    switch (carrier)
    {
        case FunctionCarrier::Pointer: return "ptr";
        case FunctionCarrier::Reference: return "ref";
    }
    return "ptr";
}

std::string_view callingConventionName(CallingConvention convention) noexcept
{
    switch (convention)
    {
        case CallingConvention::NativeSharp: return "native";
        case CallingConvention::C: return "c";
        case CallingConvention::StdCall: return "stdcall";
        case CallingConvention::FastCall: return "fastcall";
        case CallingConvention::VectorCall: return "vectorcall";
    }
    return "native";
}

std::string functionTypeIdentity(const FunctionType& type)
{
    std::string identity = "fn:";
    identity += functionCarrierName(type.carrier);
    identity.push_back(':');
    identity += callingConventionName(type.callingConvention);
    identity.push_back(':');
    identity.push_back(type.variadic ? '1' : '0');
    identity.push_back(type.isUnsafe ? '1' : '0');
    identity.push_back(type.isNoHeap ? '1' : '0');
    identity.push_back(type.isAsync ? '1' : '0');
    identity.push_back(type.nullable ? '1' : '0');
    identity.push_back(':');
    appendLengthPrefixed(identity, type.lifetime);
    identity.push_back(':');
    identity += std::to_string(type.parameters.size());
    identity.push_back(':');
    for (const std::string& parameter : type.parameters)
    {
        appendLengthPrefixed(identity, parameter);
        identity.push_back(':');
    }
    appendLengthPrefixed(identity, type.result);
    identity.push_back(':');
    appendLengthPrefixed(identity, type.errorType);
    return identity;
}

std::optional<FunctionType> parseFunctionTypeIdentity(std::string_view identity)
{
    if (!identity.starts_with("fn:")) return std::nullopt;
    std::size_t cursor = 3U;
    const std::size_t carrierEnd = identity.find(':', cursor);
    if (carrierEnd == std::string_view::npos) return std::nullopt;
    const std::optional<FunctionCarrier> carrier =
        parseCarrier(identity.substr(cursor, carrierEnd - cursor));
    if (!carrier.has_value()) return std::nullopt;
    cursor = carrierEnd + 1U;

    const std::size_t conventionEnd = identity.find(':', cursor);
    if (conventionEnd == std::string_view::npos) return std::nullopt;
    const std::optional<CallingConvention> convention =
        parseConvention(identity.substr(cursor, conventionEnd - cursor));
    if (!convention.has_value()) return std::nullopt;
    cursor = conventionEnd + 1U;
    if (cursor + 6U > identity.size()) return std::nullopt;
    const std::string_view flags = identity.substr(cursor, 5U);
    if (!std::ranges::all_of(flags, [](char value) { return value == '0' || value == '1'; }) ||
        identity[cursor + 5U] != ':')
        return std::nullopt;
    cursor += 6U;

    FunctionType type;
    type.carrier = *carrier;
    type.callingConvention = *convention;
    type.variadic = flags[0] == '1';
    type.isUnsafe = flags[1] == '1';
    type.isNoHeap = flags[2] == '1';
    type.isAsync = flags[3] == '1';
    type.nullable = flags[4] == '1';
    if (!consumeLengthPrefixed(identity, cursor, type.lifetime) ||
        !consumeLiteral(identity, cursor, ":"))
        return std::nullopt;

    std::size_t parameterCount = 0;
    const char* first = identity.data() + cursor;
    const char* last = identity.data() + identity.size();
    const auto parsedCount = std::from_chars(first, last, parameterCount);
    if (parsedCount.ec != std::errc{} || parsedCount.ptr == first ||
        parsedCount.ptr == last || *parsedCount.ptr != ':')
        return std::nullopt;
    cursor = static_cast<std::size_t>(parsedCount.ptr - identity.data()) + 1U;
    type.parameters.reserve(parameterCount);
    for (std::size_t index = 0; index < parameterCount; ++index)
    {
        std::string parameter;
        if (!consumeLengthPrefixed(identity, cursor, parameter) ||
            !consumeLiteral(identity, cursor, ":"))
            return std::nullopt;
        type.parameters.push_back(std::move(parameter));
    }
    if (!consumeLengthPrefixed(identity, cursor, type.result) ||
        !consumeLiteral(identity, cursor, ":") ||
        !consumeLengthPrefixed(identity, cursor, type.errorType) ||
        cursor != identity.size() || type.result.empty())
        return std::nullopt;
    return type;
}

bool isFunctionTypeIdentity(std::string_view identity) noexcept
{
    return identity.starts_with("fn:") && parseFunctionTypeIdentity(identity).has_value();
}

bool functionTypesAssignable(const FunctionType& source, const FunctionType& target) noexcept
{
    return source.parameters == target.parameters && source.result == target.result &&
        source.carrier == target.carrier &&
        source.callingConvention == target.callingConvention &&
        (source.errorType == target.errorType ||
            (source.errorType.empty() && !target.errorType.empty())) &&
        source.lifetime == target.lifetime && source.variadic == target.variadic &&
        (!source.isUnsafe || target.isUnsafe) &&
        (!target.isNoHeap || source.isNoHeap) &&
        source.isAsync == target.isAsync &&
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
