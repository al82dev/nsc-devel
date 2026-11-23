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
    // LKF-413: `cursor + length` wrapped for a length near SIZE_MAX; compare with what is left.
    // LKF-413: `cursor + length` desbordaba con una longitud cercana a SIZE_MAX; se compara con lo restante.
    if (cursor > input.size() || length > input.size() - cursor) return false;
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
    if (value == "sysv_abi") return CallingConvention::SysV;
    if (value == "regcall") return CallingConvention::RegCall;
    if (value == "preserve_most") return CallingConvention::PreserveMost;
    if (value == "preserve_all") return CallingConvention::PreserveAll;
    if (value == "preserve_none") return CallingConvention::PreserveNone;
    // T19-028. / T19-028.
    if (value == "swiftcall") return CallingConvention::Swift;
    if (value == "swiftasynccall") return CallingConvention::SwiftAsync;
    if (value == "intel_ocl_bicc") return CallingConvention::IntelOclBicc;
    return std::nullopt;
}

// EN: A pointee qualifier of a C signature kept in the identity (T08-002): `const` (readonly) and
//     `volatile`. Each one is an optional suffix `<tag><mask>` with one '0'/'1' per parameter and a
//     last one for the result, written only when some flag is set (readonly first, then volatile),
//     so identities without qualifiers keep their exact bytes.
// ES: Un calificador del dato apuntado de una firma C conservado en la identidad (T08-002): `const`
//     (readonly) y `volatile`. Cada uno es un sufijo opcional `<etiqueta><máscara>` con un '0'/'1' por
//     parámetro y uno final para el resultado, escrito solo si hay alguna marca (primero readonly,
//     luego volatile), así las identidades sin calificadores conservan exactamente sus bytes.
struct PointeeQualifier
{
    std::string_view tag;
    std::vector<bool> FunctionType::* parameters;
    bool FunctionType::* result;
};

constexpr PointeeQualifier kPointeeReadonly{
    ":readonly-pointee:", &FunctionType::parameterPointeeReadonly, &FunctionType::resultPointeeReadonly};
constexpr PointeeQualifier kPointeeVolatile{
    ":volatile-pointee:", &FunctionType::parameterPointeeVolatile, &FunctionType::resultPointeeVolatile};
// EN: T19-038. `_Atomic` pointee, written after volatile with the same rules, so existing identities keep their bytes.
// ES: T19-038. Dato apuntado `_Atomic`, escrito tras volatile con las mismas reglas: las identidades existentes no
//     cambian.
constexpr PointeeQualifier kPointeeAtomic{
    ":atomic-pointee:", &FunctionType::parameterPointeeAtomic, &FunctionType::resultPointeeAtomic};

// EN: Flag of parameter `index`; an empty vector or an index outside it reads as false.
// ES: Marca del parámetro `index`; un vector vacío o un índice fuera de él se leen como falso.
bool qualifierParameter(const FunctionType& type, const PointeeQualifier& qualifier, std::size_t index) noexcept
{
    const std::vector<bool>& flags = type.*qualifier.parameters;
    return index < flags.size() && flags[index];
}

bool hasQualifier(const FunctionType& type, const PointeeQualifier& qualifier) noexcept
{
    if (type.*qualifier.result) return true;
    for (std::size_t index = 0; index < type.parameters.size(); ++index)
        if (qualifierParameter(type, qualifier, index)) return true;
    return false;
}

void appendQualifierSuffix(std::string& identity, const FunctionType& type, const PointeeQualifier& qualifier)
{
    if (!hasQualifier(type, qualifier)) return;
    identity += qualifier.tag;
    for (std::size_t index = 0; index < type.parameters.size(); ++index)
        identity.push_back(qualifierParameter(type, qualifier, index) ? '1' : '0');
    identity.push_back(type.*qualifier.result ? '1' : '0');
}

// EN: Decodes one optional suffix written by appendQualifierSuffix at `cursor` and advances past it.
//     Returns false for a mask of the wrong length, a character other than '0'/'1', or an all-zero
//     mask (never written, so the canonical identity stays unique).
// ES: Decodifica en `cursor` un sufijo opcional escrito por appendQualifierSuffix y avanza tras él.
//     Devuelve false ante una máscara de longitud incorrecta, un carácter distinto de '0'/'1' o una
//     máscara sin marcas (nunca se escribe, así la identidad canónica sigue siendo única).
bool consumeQualifierSuffix(
    std::string_view identity, std::size_t& cursor, FunctionType& type, const PointeeQualifier& qualifier)
{
    if (!identity.substr(cursor).starts_with(qualifier.tag)) return true;
    const std::size_t width = type.parameters.size() + 1U;
    const std::string_view mask = identity.substr(cursor + qualifier.tag.size(), width);
    if (mask.size() != width) return false;
    for (const char value : mask)
        if (value != '0' && value != '1') return false;
    bool anyParameter = false;
    for (std::size_t index = 0; index < type.parameters.size(); ++index)
        anyParameter = anyParameter || mask[index] == '1';
    type.*qualifier.result = mask.back() == '1';
    if (!anyParameter && !(type.*qualifier.result)) return false;
    if (anyParameter)
    {
        std::vector<bool>& flags = type.*qualifier.parameters;
        flags.reserve(type.parameters.size());
        for (std::size_t index = 0; index < type.parameters.size(); ++index)
            flags.push_back(mask[index] == '1');
    }
    cursor += qualifier.tag.size() + width;
    return true;
}

// EN: Exact comparison of one qualifier in two signatures. C treats `T*`, `const T*` and
//     `volatile T*` parameters (and results) as different function types, so no direction widens.
// ES: Comparación exacta de un calificador en dos firmas. C considera tipos de función distintos a
//     parámetros (y resultados) `T*`, `const T*` y `volatile T*`, por lo que ninguna dirección amplía.
// EN: C1-02. Intermediate pointer level qualifiers of each parameter and the result: an optional suffix
//     `:pointer-levels:` with two hex digits per parameter and two for the result, written only when some mask is
//     not zero, so identities without them keep their bytes.
// ES: C1-02. Calificadores de niveles intermedios de cada parametro y del resultado: un sufijo opcional con dos
//     digitos hex por parametro y dos para el resultado, escrito solo si alguna mascara no es cero.
constexpr std::string_view kPointerLevelsTag = ":pointer-levels:";

std::uint8_t parameterLevels(const FunctionType& type, std::size_t index) noexcept
{
    return index < type.parameterPointerLevelQualifiers.size() ? type.parameterPointerLevelQualifiers[index] : 0U;
}

void appendPointerLevelsSuffix(std::string& identity, const FunctionType& type)
{
    bool any = type.resultPointerLevelQualifiers != 0U;
    for (std::size_t index = 0; index < type.parameters.size(); ++index) any = any || parameterLevels(type, index);
    if (!any) return;
    constexpr char digits[] = "0123456789abcdef";
    identity += kPointerLevelsTag;
    const auto push = [&](std::uint8_t value)
    {
        identity.push_back(digits[value >> 4U]);
        identity.push_back(digits[value & 15U]);
    };
    for (std::size_t index = 0; index < type.parameters.size(); ++index) push(parameterLevels(type, index));
    push(type.resultPointerLevelQualifiers);
}

bool consumePointerLevelsSuffix(std::string_view identity, std::size_t& cursor, FunctionType& type)
{
    if (!identity.substr(cursor).starts_with(kPointerLevelsTag)) return true;
    const std::size_t width = 2U * (type.parameters.size() + 1U);
    const std::string_view text = identity.substr(cursor + kPointerLevelsTag.size(), width);
    if (text.size() != width) return false;
    const auto digit = [](char value) -> int
    {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        return -1;
    };
    std::vector<std::uint8_t> values;
    for (std::size_t index = 0; index < width; index += 2U)
    {
        const int high = digit(text[index]);
        const int low = digit(text[index + 1U]);
        if (high < 0 || low < 0) return false;
        values.push_back(static_cast<std::uint8_t>(high * 16 + low));
    }
    type.resultPointerLevelQualifiers = values.back();
    values.pop_back();
    bool any = type.resultPointerLevelQualifiers != 0U;
    for (const std::uint8_t value : values) any = any || value != 0U;
    if (!any) return false;
    type.parameterPointerLevelQualifiers = std::move(values);
    cursor += kPointerLevelsTag.size() + width;
    return true;
}

// EN: C3-02. Widths of narrow C `_BitInt(N)` positions: an optional suffix `:bitint:` with two hex digits per
//     parameter and two for the result, written only when some width is not zero.
// ES: C3-02. Anchos de las posiciones `_BitInt(N)` estrechas: un sufijo opcional, escrito solo si alguno no es cero.
constexpr std::string_view kBitIntTag = ":bitint:";

std::uint8_t parameterBitInt(const FunctionType& type, std::size_t index) noexcept
{
    return index < type.parameterBitIntWidths.size() ? type.parameterBitIntWidths[index] : 0U;
}

void appendBitIntSuffix(std::string& identity, const FunctionType& type)
{
    bool any = type.resultBitIntWidth != 0U;
    for (std::size_t index = 0; index < type.parameters.size(); ++index) any = any || parameterBitInt(type, index);
    if (!any) return;
    constexpr char digits[] = "0123456789abcdef";
    identity += kBitIntTag;
    const auto push = [&](std::uint8_t value)
    {
        identity.push_back(digits[value >> 4U]);
        identity.push_back(digits[value & 15U]);
    };
    for (std::size_t index = 0; index < type.parameters.size(); ++index) push(parameterBitInt(type, index));
    push(type.resultBitIntWidth);
}

bool consumeBitIntSuffix(std::string_view identity, std::size_t& cursor, FunctionType& type)
{
    if (!identity.substr(cursor).starts_with(kBitIntTag)) return true;
    const std::size_t width = 2U * (type.parameters.size() + 1U);
    const std::string_view text = identity.substr(cursor + kBitIntTag.size(), width);
    if (text.size() != width) return false;
    std::vector<std::uint8_t> values;
    for (std::size_t index = 0; index < width; index += 2U)
    {
        unsigned value = 0;
        const auto parsed = std::from_chars(text.data() + index, text.data() + index + 2U, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + index + 2U) return false;
        values.push_back(static_cast<std::uint8_t>(value));
    }
    type.resultBitIntWidth = values.back();
    values.pop_back();
    bool any = type.resultBitIntWidth != 0U;
    for (const std::uint8_t value : values) any = any || value != 0U;
    if (!any) return false;
    type.parameterBitIntWidths = std::move(values);
    cursor += kBitIntTag.size() + width;
    return true;
}

bool sameBitInt(const FunctionType& source, const FunctionType& target) noexcept
{
    if (source.resultBitIntWidth != target.resultBitIntWidth) return false;
    for (std::size_t index = 0; index < source.parameters.size(); ++index)
        if (parameterBitInt(source, index) != parameterBitInt(target, index)) return false;
    return true;
}

bool samePointerLevels(const FunctionType& source, const FunctionType& target) noexcept
{
    if (source.resultPointerLevelQualifiers != target.resultPointerLevelQualifiers) return false;
    for (std::size_t index = 0; index < source.parameters.size(); ++index)
        if (parameterLevels(source, index) != parameterLevels(target, index)) return false;
    return true;
}

bool sameQualifier(const FunctionType& source, const FunctionType& target, const PointeeQualifier& qualifier) noexcept
{
    if (source.*qualifier.result != target.*qualifier.result || source.parameters.size() != target.parameters.size())
        return false;
    for (std::size_t index = 0; index < source.parameters.size(); ++index)
    {
        if (qualifierParameter(source, qualifier, index) != qualifierParameter(target, qualifier, index))
            return false;
    }
    return true;
}
}

bool functionParameterPointeeVolatile(const FunctionType& type, std::size_t parameterIndex) noexcept
{
    return qualifierParameter(type, kPointeeVolatile, parameterIndex);
}

std::uint8_t functionParameterPointerLevels(const FunctionType& type, std::size_t parameterIndex) noexcept
{
    return parameterLevels(type, parameterIndex);
}

bool functionHasPointeeVolatile(const FunctionType& type) noexcept
{
    return hasQualifier(type, kPointeeVolatile);
}

bool functionParameterPointeeReadonly(const FunctionType& type, std::size_t parameterIndex) noexcept
{
    return qualifierParameter(type, kPointeeReadonly, parameterIndex);
}

bool functionHasPointeeReadonly(const FunctionType& type) noexcept
{
    return hasQualifier(type, kPointeeReadonly);
}

bool functionParameterPointeeAtomic(const FunctionType& type, std::size_t parameterIndex) noexcept
{
    return qualifierParameter(type, kPointeeAtomic, parameterIndex);
}

bool functionHasPointeeAtomic(const FunctionType& type) noexcept
{
    return hasQualifier(type, kPointeeAtomic);
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
        case CallingConvention::SysV: return "sysv_abi";
        case CallingConvention::RegCall: return "regcall";
        case CallingConvention::PreserveMost: return "preserve_most";
        case CallingConvention::PreserveAll: return "preserve_all";
        case CallingConvention::PreserveNone: return "preserve_none";
        case CallingConvention::Swift: return "swiftcall";
        case CallingConvention::SwiftAsync: return "swiftasynccall";
        case CallingConvention::IntelOclBicc: return "intel_ocl_bicc";
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
    appendQualifierSuffix(identity, type, kPointeeReadonly);
    appendQualifierSuffix(identity, type, kPointeeVolatile);
    appendQualifierSuffix(identity, type, kPointeeAtomic);
    appendPointerLevelsSuffix(identity, type);
    appendBitIntSuffix(identity, type);
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
        !consumeQualifierSuffix(identity, cursor, type, kPointeeReadonly) ||
        !consumeQualifierSuffix(identity, cursor, type, kPointeeVolatile) ||
        !consumeQualifierSuffix(identity, cursor, type, kPointeeAtomic) ||
        !consumePointerLevelsSuffix(identity, cursor, type) || !consumeBitIntSuffix(identity, cursor, type) ||
        cursor != identity.size() ||
        type.result.empty())
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

// EN: Item 7.1. A non-capturing NativeSharp lambda reaches a C function pointer through its generated thunk. On
//     Windows x64 the NativeSharp, C, stdcall, fastcall and ms_abi conventions are the same physical one, so the
//     lambda takes the target's convention; vectorcall and the others keep the exact match.
// ES: Item 7.1. Una lambda NativeSharp sin capturas llega a un puntero a funcion C por su thunk generado. En
//     Windows x64 las convenciones NativeSharp, C, stdcall, fastcall y ms_abi son la misma fisica, asi que la
//     lambda toma la convencion del destino; vectorcall y las demas conservan la igualdad exacta.
FunctionType lambdaSignatureForTarget(const FunctionType& lambda, const FunctionType& target) noexcept
{
    const auto win64 = [](CallingConvention convention) {
        return convention == CallingConvention::NativeSharp || convention == CallingConvention::C ||
            convention == CallingConvention::StdCall || convention == CallingConvention::FastCall ||
            convention == CallingConvention::Win64;
    };
    FunctionType adapted = lambda;
    if (lambda.callingConvention == CallingConvention::NativeSharp && win64(target.callingConvention))
        adapted.callingConvention = target.callingConvention;
    // C3-02: a lambda over the carriers serves a C `_BitInt(N)` pointer through its thunk. / Una lambda sobre los
    //     portadores sirve a un puntero C `_BitInt(N)` por su thunk.
    if (lambda.parameterBitIntWidths.empty() && lambda.resultBitIntWidth == 0U)
    {
        adapted.parameterBitIntWidths = target.parameterBitIntWidths;
        adapted.resultBitIntWidth = target.resultBitIntWidth;
    }
    return adapted;
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
        sameQualifier(source, target, kPointeeReadonly) && sameQualifier(source, target, kPointeeVolatile) &&
        sameQualifier(source, target, kPointeeAtomic) && samePointerLevels(source, target) &&
        sameBitInt(source, target);
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
