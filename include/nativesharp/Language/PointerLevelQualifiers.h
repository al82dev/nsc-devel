#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// EN: C1-02. Qualifiers of the intermediate levels of a pointer chain. NativeSharp already keeps the qualifiers of the
//     base of a chain as the pointee marks (`T const**`, `T volatile**`, T08-D17); this mask adds the qualifiers of
//     each pointer object between the top pointer and the base, so `int* volatile*` or `char const* const*` keep
//     every level, as in C and C++. Level 0 is the object the outermost pointer points to; level k is reached after
//     k + 1 dereferences. Each level uses two bits: const (bit 2k) and volatile (bit 2k + 1). Three levels fit the
//     byte the metadata already reserves for pointee qualifiers.
// ES: C1-02. Calificadores de los niveles intermedios de una cadena de punteros. NativeSharp ya conserva los
//     calificadores de la base de una cadena como marcas del apuntado (T08-D17); esta mascara agrega los de cada
//     objeto puntero entre el puntero superior y la base, asi `int* volatile*` o `char const* const*` conservan cada
//     nivel, como en C y C++. El nivel 0 es el objeto al que apunta el puntero exterior; el nivel k se alcanza tras
//     k + 1 desreferencias. Cada nivel usa dos bits: const (bit 2k) y volatile (bit 2k + 1). Tres niveles caben en
//     el byte que la metadata ya reserva para calificadores del apuntado.
namespace nativesharp::language
{
inline constexpr std::size_t kMaxPointerLevelQualifierDepth = 3;

inline constexpr std::uint8_t pointerLevelConstBit(std::size_t level) noexcept
{
    return static_cast<std::uint8_t>(1U << (2U * level));
}

inline constexpr std::uint8_t pointerLevelVolatileBit(std::size_t level) noexcept
{
    return static_cast<std::uint8_t>(1U << (2U * level + 1U));
}

inline constexpr bool pointerLevelConst(std::uint8_t mask, std::size_t level) noexcept
{
    return level < kMaxPointerLevelQualifierDepth && (mask & pointerLevelConstBit(level)) != 0U;
}

inline constexpr bool pointerLevelVolatile(std::uint8_t mask, std::size_t level) noexcept
{
    return level < kMaxPointerLevelQualifierDepth && (mask & pointerLevelVolatileBit(level)) != 0U;
}

// The mask of `*p`: level 1 becomes level 0. / La mascara de `*p`: el nivel 1 pasa a nivel 0.
inline constexpr std::uint8_t pointerLevelsAfterDereference(std::uint8_t mask) noexcept
{
    return static_cast<std::uint8_t>(mask >> 2U);
}

// EN: The mask of `&x` where x is a pointer: x's own storage qualifiers become level 0. Null when the result
//     would need a fourth intermediate level.
// ES: La mascara de `&x` con x puntero: los calificadores del almacenamiento de x pasan a nivel 0. Nulo si el
//     resultado necesitaria un cuarto nivel intermedio.
inline constexpr bool pointerLevelsAfterAddressOf(
    std::uint8_t mask, bool storageConst, bool storageVolatile, std::uint8_t& result) noexcept
{
    if ((mask & 0xC0U) != 0U) return false;
    result = static_cast<std::uint8_t>((mask << 2U) | (storageConst ? 1U : 0U) | (storageVolatile ? 2U : 0U));
    return true;
}

// Pointer depth of a type key (`builtin:int**` = 2). / Profundidad de punteros de una clave de tipo.
inline constexpr std::size_t pointerDepthOfTypeKey(std::string_view typeKey) noexcept
{
    std::size_t depth = 0;
    while (depth < typeKey.size() && typeKey[typeKey.size() - 1 - depth] == '*') ++depth;
    return depth;
}

// EN: Implicit qualification conversion between two pointer types of depth >= 2, with C's rule (C11 6.5.16.1, which
//     NativeSharp already applied to the base, T08-D17): no level may lose a qualifier and only level 0, the object
//     the top pointer points to, may gain one. The base level is depth - 1 and carries the pointee marks.
// ES: Conversion implicita de calificacion entre dos tipos puntero de profundidad >= 2, con la regla de C (C11
//     6.5.16.1, que NativeSharp ya aplicaba a la base, T08-D17): ningun nivel pierde un calificador y solo el nivel
//     0, el objeto al que apunta el puntero superior, puede ganar uno. El nivel base es depth - 1 y lleva las marcas
//     del apuntado.
struct PointerChainQualifiers
{
    std::uint8_t levels = 0;
    bool baseConst = false;
    bool baseVolatile = false;
};

inline constexpr bool pointerChainLevelQualifiers(
    const PointerChainQualifiers& chain, std::size_t depth, std::size_t level, bool& isConst, bool& isVolatile) noexcept
{
    if (level + 1U == depth) { isConst = chain.baseConst; isVolatile = chain.baseVolatile; return true; }
    isConst = pointerLevelConst(chain.levels, level);
    isVolatile = pointerLevelVolatile(chain.levels, level);
    return true;
}

enum class PointerQualificationConversion
{
    Identity,
    Allowed,
    DropsQualifier,
    UnsafeAddition
};

inline constexpr PointerQualificationConversion classifyPointerQualificationConversion(
    const PointerChainQualifiers& source, const PointerChainQualifiers& target, std::size_t depth) noexcept
{
    bool changed = false;
    for (std::size_t level = 0; level < depth; ++level)
    {
        bool sourceConst = false, sourceVolatile = false, targetConst = false, targetVolatile = false;
        pointerChainLevelQualifiers(source, depth, level, sourceConst, sourceVolatile);
        pointerChainLevelQualifiers(target, depth, level, targetConst, targetVolatile);
        if ((sourceConst && !targetConst) || (sourceVolatile && !targetVolatile))
            return PointerQualificationConversion::DropsQualifier;
        const bool adds = (targetConst && !sourceConst) || (targetVolatile && !sourceVolatile);
        if (adds && level != 0) return PointerQualificationConversion::UnsafeAddition;
        changed = changed || adds;
    }
    return changed ? PointerQualificationConversion::Allowed : PointerQualificationConversion::Identity;
}

// EN: The spelling of a pointer chain with its qualifiers (`int const* volatile*`), for diagnostics and sources.
// ES: La escritura de una cadena de punteros con sus calificadores, para diagnosticos y fuentes.
inline void appendPointerLevelSpelling(std::string& text, std::size_t level, std::uint8_t mask)
{
    if (pointerLevelConst(mask, level)) text += " const";
    if (pointerLevelVolatile(mask, level)) text += " volatile";
}
}
