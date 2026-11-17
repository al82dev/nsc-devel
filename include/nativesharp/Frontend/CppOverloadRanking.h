#pragma once

#include "nativesharp/Frontend/ClassMemberSymbol.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
// EN: T19-006. The C++ type of a NativeSharp arithmetic argument, used to rank C++ overloads as Clang does.
//     `long` is 64 bits in NativeSharp and `long long` in C++ on Windows. Returns nothing for other types.
// ES: T19-006. El tipo C++ de un argumento aritmetico NativeSharp, para ordenar sobrecargas C++ como Clang.
//     `long` mide 64 bits en NativeSharp y es `long long` en C++ en Windows. No devuelve nada para otros tipos.
inline std::optional<std::string_view> cppArithmeticArgumentType(std::string_view typeKey)
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 13> types = {{
        {"builtin:bool", "bool"},
        {"builtin:sbyte", "signed char"},
        {"builtin:byte", "unsigned char"},
        {"builtin:short", "short"},
        {"builtin:ushort", "unsigned short"},
        {"builtin:int", "int"},
        {"builtin:uint", "unsigned int"},
        {"builtin:long", "long long"},
        {"builtin:ulong", "unsigned long long"},
        {"builtin:nint", "long long"},
        {"builtin:nuint", "unsigned long long"},
        {"builtin:float", "float"},
        {"builtin:double", "double"}}};
    for (const auto& entry : types)
    {
        if (entry.first == typeKey) return entry.second;
    }
    return std::nullopt;
}

// EN: T19-006. The rank of the C++ standard conversion from an arithmetic argument type to an arithmetic
//     parameter ([over.ics.scs]): 0 exact match, 1 promotion ([conv.prom], [conv.fpprom]), 2 conversion.
//     Every type narrower than int fits int on Windows, so it promotes to int.
// ES: T19-006. El rango de la conversion estandar C++ de un argumento aritmetico a un parametro aritmetico
//     ([over.ics.scs]): 0 coincidencia exacta, 1 promocion, 2 conversion. En Windows todo tipo mas estrecho
//     que int cabe en int, asi que promueve a int.
inline int cppArithmeticConversionRank(std::string_view argument, std::string_view parameter)
{
    if (argument == parameter) return 0;
    const bool promotesToInt = argument == "bool" || argument == "signed char" || argument == "unsigned char" ||
        argument == "short" || argument == "unsigned short";
    if (promotesToInt && parameter == "int") return 1;
    if (argument == "float" && parameter == "double") return 1;
    return 2;
}

// EN: T19-006. A C++ arithmetic parameter spelling, for the methods that keep only their spellings.
// ES: T19-006. Una escritura C++ de parametro aritmetico, para los metodos que solo conservan sus escrituras.
inline bool cppArithmeticSpelling(std::string_view spelling)
{
    static constexpr std::array<std::string_view, 19> spellings = {
        "bool", "char", "signed char", "unsigned char", "short", "unsigned short", "int", "unsigned int", "long",
        "unsigned long", "long long", "unsigned long long", "float", "double", "long double", "wchar_t", "char8_t",
        "char16_t", "char32_t"};
    return std::find(spellings.begin(), spellings.end(), spelling) != spellings.end();
}

// EN: T19-006. Compares two C++ overloads that NativeSharp ranks alike by the C++ conversion of each argument
//     to the parameters whose spellings differ, as Clang does. `ordinals` maps each argument to its parameter in
//     both overloads and `sources` holds each argument's NativeSharp type key. Returns nothing when no spelling
//     differs or a differing position is not arithmetic; otherwise whether `left` is better than `right`.
// ES: T19-006. Compara dos sobrecargas C++ que NativeSharp ordena igual por la conversion C++ de cada argumento a
//     los parametros cuyas escrituras difieren, como Clang. `ordinals` asigna cada argumento a su parametro en
//     ambas y `sources` tiene la clave de tipo NativeSharp de cada argumento. No devuelve nada si ninguna escritura
//     difiere o una posicion distinta no es aritmetica; si no, si `left` es mejor que `right`.
inline std::optional<bool> compareCppArithmeticSpellings(
    std::span<const std::string> left, std::span<const std::string> right,
    std::span<const std::size_t> ordinals, std::span<const std::string> sources)
{
    if (ordinals.size() != sources.size()) return std::nullopt;
    bool differs = false;
    bool noWorse = true;
    bool better = false;
    for (std::size_t argument = 0; argument < ordinals.size(); ++argument)
    {
        const std::size_t parameter = ordinals[argument];
        if (parameter >= left.size() || parameter >= right.size()) return std::nullopt;
        if (left[parameter] == right[parameter]) continue;
        differs = true;
        const auto source = cppArithmeticArgumentType(sources[argument]);
        if (!source || !cppArithmeticSpelling(left[parameter]) || !cppArithmeticSpelling(right[parameter]))
            return std::nullopt;
        const int leftRank = cppArithmeticConversionRank(*source, left[parameter]);
        const int rightRank = cppArithmeticConversionRank(*source, right[parameter]);
        if (leftRank > rightRank) noWorse = false;
        if (leftRank < rightRank) better = true;
    }
    if (!differs) return std::nullopt;
    return noWorse && better;
}

// EN: T19-006 (T17-003). One note per overload candidate of a C++ method or constructor: its C++ declaration
//     and, when `explainRejection`, why it does not take the call (its arity, or the first argument whose type
//     does not convert). `listed` holds the indices into `evaluated` to describe. Empty unless every candidate
//     examined is an imported C++ member with its declaration, so NativeSharp members keep their diagnostics.
// ES: T19-006 (T17-003). Una nota por candidato de un metodo o constructor C++: su declaracion C++ y, con
//     `explainRejection`, por que no acepta la llamada (su aridad, o el primer argumento cuyo tipo no se
//     convierte). `listed` tiene los indices de `evaluated` a describir. Vacio salvo que todo candidato examinado
//     sea un miembro C++ importado con su declaracion, asi los miembros NativeSharp conservan sus diagnosticos.
template <typename Candidate, typename OrdinalOf>
std::vector<std::string> cppMemberCandidateNotes(
    std::span<const ClassMemberSymbol> members, std::span<const Candidate> evaluated,
    std::span<const std::size_t> listed, bool explainRejection, OrdinalOf ordinalOf)
{
    const auto readable = [](std::string_view key) {
        for (const std::string_view prefix : {std::string_view("builtin:"), std::string_view("type:")})
            if (key.starts_with(prefix)) return std::string(key.substr(prefix.size()));
        return std::string(key);
    };
    if (evaluated.empty()) return {};
    for (const Candidate& candidate : evaluated)
    {
        const std::optional<std::size_t> ordinal = ordinalOf(candidate);
        if (!ordinal || *ordinal >= members.size() || !members[*ordinal].isCppExternal ||
            members[*ordinal].cppDeclarationSpelling.empty()) return {};
    }
    std::vector<std::string> notes;
    constexpr std::size_t limit = 8;
    for (const std::size_t index : listed)
    {
        if (index >= evaluated.size()) continue;
        if (notes.size() == limit)
        {
            notes.push_back("and " + std::to_string(listed.size() - limit) + " more candidate(s)");
            break;
        }
        const Candidate& candidate = evaluated[index];
        const ClassMemberSymbol& member = members[*ordinalOf(candidate)];
        std::string note = "candidate: " + member.cppDeclarationSpelling;
        if (explainRejection && !candidate.viable)
        {
            const auto failed = std::find_if(candidate.conversions.begin(), candidate.conversions.end(),
                [](const auto& conversion) { return !conversion.exists(); });
            if (candidate.conversions.empty())
                note += " takes " + std::to_string(member.parameters.size()) + " argument(s)";
            else if (failed != candidate.conversions.end())
                note += ": argument " + std::to_string(failed - candidate.conversions.begin() + 1) + " of type '" +
                    readable(failed->sourceTypeKey) + "' does not convert to '" + readable(failed->targetTypeKey) + "'";
            else
                note += " does not accept these arguments";
        }
        if (std::find(notes.begin(), notes.end(), note) == notes.end()) notes.push_back(std::move(note));
    }
    return notes;
}
}
