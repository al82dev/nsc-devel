#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../../Internal/CloneDepthGuard.h"
#include "../../Cache/CachePayloadReader.h"
#include "../../Internal/EnumConstResolver.h"
#include "../../Internal/SpecializationWaveTimer.h"
#include "../../Internal/InterfaceFingerprint.h"
#include "../../Internal/ImplementationFingerprint.h"
#include "../../Internal/LocatedNode.h"
#include "../../Internal/MirFunctionKey.h"
#include "../../Internal/MirCacheRecord.h"
#include "../../Internal/ProjectGenericDependencyFrame.h"
#include "../../Internal/ProjectGenericBodyFrame.h"
#include "../../Internal/ProjectGenericAliasFrame.h"
#include "../../Internal/ProjectGenericChildWork.h"
#include "../../Internal/ProjectGenericCollectFrame.h"
#include "../../Internal/ProjectGenericProcessFrame.h"
#include "../../Internal/ProjectGenericReservedDemand.h"
#include "../../Internal/TypeWork.h"
#include "nativesharp/Frontend/ProjectOpaqueResultModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "TemplateSpelling.h"
namespace nativesharp::frontend::internal
{
std::string qualifiedName(std::string_view ns, std::string_view name)
{
    if (ns.empty()) return std::string(name);
    return std::string(ns) + "." + std::string(name);
}

// NSC-LIMIT-009. La cadena de tipos contenedores extendida con uno mas, unida con `+` igual que
// `TypeIdentity::fullyQualifiedName`.
//
// Este modelo llevaba UN nombre de dueno --el del contenedor mas interno-- y lo REEMPLAZABA al
// bajar, de modo que `A.Inner` y `B.Inner` eran los dos «Inner». Con eso, dos plantillas sin
// ninguna relacion compartian clave canonica, se agrupaban como sobrecargas la una de la otra y
// podian compartir especializacion. La cadena entera es lo unico que las separa.
std::string appendOwner(std::string_view chain, std::string_view name)
{
    if (chain.empty()) return std::string(name);
    return std::string(chain) + "+" + std::string(name);
}

// El receptor de `N1.C.F<int>(x)` TAL COMO SE ESCRIBIO: `N1.C`. Con puntos, sin interpretar.
//
// LA VERSION ANTERIOR LO CONVERTIA A `N1+C`, y eso era un error de principio, no de detalle: esta
// fase corre ANTES de la resolucion de nombres, de modo que no puede saber que en `N1.C` el primero
// es un namespace y el segundo un tipo, ni que en `A.Inner` los dos son tipos. Poner `+` es afirmar
// una de las dos lecturas sin datos para elegir.
//
// Medido con la version anterior: `N1.C.F<int>(1)` producia `N1+C`, que no coincide con NINGUN
// registro --el de N1 tiene owner `C`-- de modo que el filtro de dueno descartaba las dos
// plantillas y solo el fallback rescataba la llamada. Con dos `F<T>` visibles de la misma aridad,
// ese fallback no es ambiguo a veces: lo es siempre, y dos llamadas completamente calificadas no
// compilaban.
//
// Devuelve vacio si el receptor no es una cadena de identificadores --una expresion, un indice, una
// llamada--; entonces se conserva el comportamiento anterior.
std::string receiverSourceSpelling(const ast::AstNode* receiver)
{
    if (receiver == nullptr) return {};
    if (receiver->kind() == ast::AstNodeKind::IdentifierExpression) return receiver->text();
    std::vector<std::string_view> members;
    std::size_t resultSize = 0;
    const ast::AstNode* current = receiver;
    while (current != nullptr && current->kind() == ast::AstNodeKind::MemberExpression)
    {
        members.push_back(current->text());
        resultSize += current->text().size() + 1U;
        current = current->first();
    }
    if (current == nullptr || current->kind() != ast::AstNodeKind::IdentifierExpression ||
        current->text().empty())
    {
        return {};
    }

    resultSize += current->text().size();
    std::string result;
    result.reserve(resultSize);
    result.append(current->text());
    for (auto iterator = members.rbegin(); iterator != members.rend(); ++iterator)
    {
        result.push_back('.');
        result.append(*iterator);
    }
    return result;
}

// La escritura fuente COMPLETA de una plantilla: `N1.C`, `a3.A.Inner`. El registro guarda el
// namespace y la cadena de contenedores por separado, y la cadena usa `+` porque es la identidad
// canonica; aqui se vuelve a puntos, que es lo que el programador escribe.
std::string templateSourceSpelling(std::string_view namespaceName, std::string_view ownerChain)
{
    std::string owner(ownerChain);
    for (char& character : owner)
    {
        if (character == '+') character = '.';
    }
    return qualifiedName(namespaceName, owner);
}

// LOS SEGMENTOS DE UN NOMBRE PUNTEADO. `A.B` -> {"A","B"}; el vacio no da segmentos.
std::vector<std::string> splitDotted(std::string_view text)
{
    std::vector<std::string> segments;
    std::size_t begin = 0;
    while (begin <= text.size())
    {
        const std::size_t dot = text.find('.', begin);
        const std::size_t stop = dot == std::string_view::npos ? text.size() : dot;
        if (stop > begin) segments.emplace_back(text.substr(begin, stop - begin));
        if (dot == std::string_view::npos) break;
        begin = dot + 1;
    }
    return segments;
}

// LA ESCRITURA COMPLETA DE CADA TIPO CONTENEDOR DEL SITIO, de fuera adentro. El modelo guarda la
// cadena de duenos con `+` --es la identidad canonica-- y aqui hace falta con puntos y ACUMULADA:
// dentro de `A.Outer+Medio` los contenedores son `A.Outer` y `A.Outer.Medio`, en ese orden, que es
// lo que el escalon de contenedores necesita para probar de dentro afuera.
std::vector<std::string> enclosingTypeSpellings(
    std::string_view namespaceName, std::string_view ownerChain)
{
    std::vector<std::string> spellings;
    std::string current(namespaceName);
    std::size_t begin = 0;
    while (begin <= ownerChain.size())
    {
        const std::size_t plus = ownerChain.find('+', begin);
        const std::size_t stop = plus == std::string_view::npos ? ownerChain.size() : plus;
        if (stop > begin)
        {
            if (!current.empty()) current.push_back('.');
            current.append(ownerChain.substr(begin, stop - begin));
            spellings.push_back(current);
        }
        if (plus == std::string_view::npos) break;
        begin = plus + 1;
    }
    return spellings;
}

std::string stableHashSuffix(std::string_view text)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const char raw : text)
    {
        const unsigned char ch = static_cast<unsigned char>(raw);
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ULL;
    }
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << hash;
    return stream.str();
}
}
