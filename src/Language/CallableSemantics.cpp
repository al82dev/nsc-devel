#include "nativesharp/Language/CallableSemantics.h"

#include "CheckedLayoutArithmetic.h"

#include <algorithm>
#include <charconv>

namespace nativesharp::language
{
namespace
{
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
    if (value == "win64") return CallingConvention::Win64;
    return std::nullopt;
}

// EN: Tag of the optional identity suffix that records volatile pointees of a C signature.
// ES: Etiqueta del sufijo opcional de la identidad que registra datos apuntados volatile de una firma C.
constexpr std::string_view kPointeeVolatileTag = ":volatile-pointee:";

// EN: Appends the volatile-pointee suffix: one '0'/'1' per parameter followed by one for the
//     result. Called only when at least one flag is set, so identities without the qualifier keep
//     their exact historical bytes. Complexity: O(parameter count).
// ES: Agrega el sufijo de datos apuntados volatile: un '0'/'1' por parámetro y uno final para el
//     resultado. Solo se llama cuando hay al menos una marca, por lo que las identidades sin el
//     calificador conservan exactamente sus bytes anteriores. Complejidad: O(número de parámetros).
void appendPointeeVolatileSuffix(std::string& identity, const FunctionType& type)
{
    identity += kPointeeVolatileTag;
    for (std::size_t index = 0; index < type.parameters.size(); ++index)
        identity.push_back(functionParameterPointeeVolatile(type, index) ? '1' : '0');
    identity.push_back(type.resultPointeeVolatile ? '1' : '0');
}

// EN: Decodes the optional suffix written by appendPointeeVolatileSuffix. `cursor` points just after
//     the error type. Returns false for any other trailing text, a mask of the wrong length, a
//     character other than '0'/'1', or an all-zero mask (which is never written, so the canonical
//     identity stays unique).
// ES: Decodifica el sufijo opcional escrito por appendPointeeVolatileSuffix. `cursor` apunta justo
//     después del tipo de error. Devuelve false ante cualquier otro texto final, una máscara de
//     longitud incorrecta, un carácter distinto de '0'/'1' o una máscara sin marcas (nunca se
//     escribe, así la identidad canónica sigue siendo única).
bool consumePointeeVolatileSuffix(std::string_view identity, std::size_t cursor, FunctionType& type)
{
    if (cursor == identity.size()) return true;
    if (!consumeLiteral(identity, cursor, kPointeeVolatileTag)) return false;
    const std::string_view mask = identity.substr(cursor);
    if (mask.size() != type.parameters.size() + 1U) return false;
    bool anyParameter = false;
    for (const char value : mask)
        if (value != '0' && value != '1') return false;
    for (std::size_t index = 0; index < type.parameters.size(); ++index)
        anyParameter = anyParameter || mask[index] == '1';
    type.resultPointeeVolatile = mask.back() == '1';
    if (!anyParameter && !type.resultPointeeVolatile) return false;
    if (anyParameter)
    {
        type.parameterPointeeVolatile.reserve(type.parameters.size());
        for (std::size_t index = 0; index < type.parameters.size(); ++index)
            type.parameterPointeeVolatile.push_back(mask[index] == '1');
    }
    return true;
}

// EN: Exact comparison of the volatile-pointee flags of two signatures. C treats `T*` and
//     `volatile T*` parameters (and results) as different function types, so no direction widens.
// ES: Comparación exacta de las marcas volatile de dos firmas. C considera tipos de función
//     distintos a parámetros (y resultados) `T*` y `volatile T*`, por lo que ninguna dirección amplía.
bool samePointeeVolatile(const FunctionType& source, const FunctionType& target) noexcept
{
    if (source.resultPointeeVolatile != target.resultPointeeVolatile ||
        source.parameters.size() != target.parameters.size())
        return false;
    for (std::size_t index = 0; index < source.parameters.size(); ++index)
    {
        if (functionParameterPointeeVolatile(source, index) !=
            functionParameterPointeeVolatile(target, index))
            return false;
    }
    return true;
}
}

// EN: True when parameter `parameterIndex` is a pointer whose pointee is volatile. An empty flag
//     vector or an index outside it reads as false.
// ES: Verdadero cuando el parámetro `parameterIndex` es un puntero cuyo dato apuntado es volatile.
//     Un vector de marcas vacío o un índice fuera de él se leen como falso.
bool functionParameterPointeeVolatile(const FunctionType& type, std::size_t parameterIndex) noexcept
{
    return parameterIndex < type.parameterPointeeVolatile.size() &&
        type.parameterPointeeVolatile[parameterIndex];
}

// EN: True when any parameter or the result carries a volatile pointee.
// ES: Verdadero cuando algún parámetro o el resultado tiene un dato apuntado volatile.
bool functionHasPointeeVolatile(const FunctionType& type) noexcept
{
    if (type.resultPointeeVolatile) return true;
    for (std::size_t index = 0; index < type.parameters.size(); ++index)
        if (functionParameterPointeeVolatile(type, index)) return true;
    return false;
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
        case CallingConvention::Win64: return "win64";
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
    if (functionHasPointeeVolatile(type)) appendPointeeVolatileSuffix(identity, type);
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
        !consumePointeeVolatileSuffix(identity, cursor, type) || type.result.empty())
        return std::nullopt;
    return type;
}

bool isFunctionTypeIdentity(std::string_view identity) noexcept
{
    return identity.starts_with("fn:") && parseFunctionTypeIdentity(identity).has_value();
}

std::string anonymousLambdaTypeIdentity(const AnonymousLambdaType& type)
{
    return "lambda:" + std::to_string(type.ownerOrdinal) + ":" +
        std::to_string(type.lambdaOrdinal) + ":" + functionTypeIdentity(type.signature);
}

std::optional<AnonymousLambdaType> parseAnonymousLambdaTypeIdentity(std::string_view identity)
{
    if (!identity.starts_with("lambda:")) return std::nullopt;
    std::size_t cursor = 7U;
    const auto consumeOrdinal = [&](std::size_t& value) -> bool
    {
        const char* first = identity.data() + cursor;
        const char* last = identity.data() + identity.size();
        const auto parsed = std::from_chars(first, last, value);
        if (parsed.ec != std::errc{} || parsed.ptr == first || parsed.ptr == last ||
            *parsed.ptr != ':')
            return false;
        cursor = static_cast<std::size_t>(parsed.ptr - identity.data()) + 1U;
        return true;
    };

    AnonymousLambdaType type;
    if (!consumeOrdinal(type.ownerOrdinal) || !consumeOrdinal(type.lambdaOrdinal))
        return std::nullopt;
    const std::optional<FunctionType> signature =
        parseFunctionTypeIdentity(identity.substr(cursor));
    if (!signature.has_value()) return std::nullopt;
    type.signature = *signature;
    return type;
}

bool isAnonymousLambdaTypeIdentity(std::string_view identity) noexcept
{
    try
    {
        return identity.starts_with("lambda:") &&
            parseAnonymousLambdaTypeIdentity(identity).has_value();
    }
    catch (...)
    {
        return false;
    }
}

std::string anonymousLambdaGeneratedName(const AnonymousLambdaType& type)
{
    return "__nsc_lambda_" + std::to_string(type.ownerOrdinal) + "_" +
        std::to_string(type.lambdaOrdinal);
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
        (!source.nullable || target.nullable) &&
        samePointeeVolatile(source, target);
}

bool computeClosureLayout(
    std::span<const Capture> captures,
    ClosureLayout& layout,
    std::string& error)
{
    error.clear();
    layout = {};
    ClosureLayout computed;
    for (const Capture& capture : captures)
    {
        if (capture.name.empty() || capture.size == 0 || capture.alignment == 0 ||
            capture.alignment > 16 || (capture.alignment & (capture.alignment - 1U)) != 0)
        { error = "closure capture has invalid name, size, or alignment"; return false; }
        computed.alignment = std::max(computed.alignment, capture.alignment);
        const std::optional<std::size_t> alignedSize =
            detail::checkedLayoutAlignUp(computed.size, capture.alignment);
        const std::optional<std::size_t> captureEnd = alignedSize.has_value()
            ? detail::checkedLayoutAdd(*alignedSize, capture.size)
            : std::nullopt;
        if (!captureEnd.has_value())
        {
            error = "closure layout overflows size_t";
            return false;
        }
        computed.size = *captureEnd;
    }
    if (!captures.empty())
    {
        const std::optional<std::size_t> finalSize =
            detail::checkedLayoutAlignUp(computed.size, computed.alignment);
        if (!finalSize.has_value())
        {
            error = "closure layout overflows size_t";
            return false;
        }
        computed.size = *finalSize;
    }
    layout = computed;
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

// Native ABI 1.1: code + receiver + receiver/environment destroy callback + producer free hook.
BoundMethodLayout windowsX64BoundMethodLayout() noexcept { return {32, 8}; }

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
