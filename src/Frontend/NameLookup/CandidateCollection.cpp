#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "../Internal/LookupEnvironment.h"
#include "../Internal/ProjectNameLookupFrame.h"

#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Compilation/PartialTypeAggregator.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <deque>
#include <filesystem>
#include <functional>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
void appendUnique(std::vector<std::size_t>& destination, std::span<const std::size_t> source)
{
    for (const std::size_t value : source)
    {
        if (std::find(destination.begin(), destination.end(), value) == destination.end())
        {
            destination.push_back(value);
        }
    }
}
}

std::vector<std::size_t> ProjectNameLookup::resolveModuleQualifiedTypeCandidates(
    std::size_t projectOrdinal,
    const std::string& writtenName,
    std::size_t moduleSeparator) const
{
    const std::string_view requestedModule(writtenName.data(), moduleSeparator);
    const std::string requestedType = writtenName.substr(moduleSeparator + 2U);
    if (requestedModule.empty() || requestedType.empty()) return {};

    std::vector<std::size_t> found;
    for (std::size_t ordinal = 0; ordinal < modules_.size(); ++ordinal)
    {
        if (modules_[ordinal] == nullptr || modules_[ordinal]->moduleName() != requestedModule)
            continue;
        const std::optional<std::size_t> target = findTypeInModule(ordinal, requestedType);
        if (!target.has_value()) continue;
        if (ordinal == projectOrdinal || typeIsVisibleTo(projectOrdinal, *target))
        {
            const std::array<std::size_t, 1> single{*target};
            appendUnique(found, single);
        }
    }
    return found;
}

void ProjectNameLookup::appendImportedNamespaceTypeCandidates(
    std::vector<std::size_t>& found,
    std::size_t projectOrdinal,
    const LookupEnvironment& environment,
    const std::string& candidate,
    bool simpleName) const
{
    if (simpleName)
    {
        appendUnique(found, findExactTypeCandidates(projectOrdinal, candidate, false));
        return;
    }
    const std::optional<std::size_t> own = findTypeInModule(projectOrdinal, candidate);
    if (own.has_value() && nestedTypeIsVisible(*own, environment))
    {
        const std::array<std::size_t, 1> single{*own};
        appendUnique(found, single);
    }
    for (const std::size_t visibleOrdinal : modules_[projectOrdinal]->visibleProjectOrdinals())
    {
        const std::optional<std::size_t> other = findTypeInModule(visibleOrdinal, candidate);
        if (other.has_value() && typeIsVisibleTo(projectOrdinal, *other))
        {
            const std::array<std::size_t, 1> single{*other};
            appendUnique(found, single);
        }
    }
}

void ProjectNameLookup::appendReferencedModuleTypeCandidates(
    std::vector<std::size_t>& found,
    std::size_t projectOrdinal,
    const std::string& candidate,
    bool simpleName) const
{
    for (const std::size_t visibleOrdinal : modules_[projectOrdinal]->visibleProjectOrdinals())
    {
        const std::optional<std::size_t> other = findTypeInModule(visibleOrdinal, candidate);
        if (other.has_value() && typeIsVisibleTo(projectOrdinal, *other))
        {
            const std::array<std::size_t, 1> single{*other};
            appendUnique(found, single);
        }
        if (simpleName) continue;
    }
}

void ProjectNameLookup::appendQualifiedTypeCandidate(
    std::vector<std::size_t>& found,
    std::size_t projectOrdinal,
    const LookupEnvironment& environment,
    const std::string& candidate,
    QualifiedNameTier tier,
    bool simpleName) const
{
    switch (tier)
    {
        case QualifiedNameTier::EnclosingType:
        {
            // Este escalon significa literalmente «miembro de un tipo contenedor».
            // Su escritura fuente usa puntos (Ns.Outer.Item), pero esa misma cadena
            // puede ser tambien la clave canonica de un tipo Item declarado en el
            // namespace Ns.Outer. findTypeInModule consulta primero esa clave canonica
            // y, en la colision, devolvia el tipo de namespace antes de mirar el
            // anidado. Aqui no hay tal ambiguedad: por definicion del escalon solo
            // participa el indice de nombres fuente de tipos anidados.
            const auto own = nestedSourceNamesByModule_[projectOrdinal].find(candidate);
            if (own != nestedSourceNamesByModule_[projectOrdinal].end() &&
                nestedTypeIsVisible(own->second, environment))
            {
                const std::array<std::size_t, 1> single{own->second};
                appendUnique(found, single);
            }
            break;
        }
        case QualifiedNameTier::LexicalNamespace:
        {
            // UN NOMBRE SIMPLE SONDEA SOLO LA CLAVE CANONICA; uno con puntos pasa tambien
            // por el indice de nombres fuente, que es lo que hace que `ns.Outer.Item`
            // pueda ser un anidado. La diferencia estaba ya en las dos ramas de antes y se
            // conserva tal cual: cambiarla movería la resolucion de nombres de todo el
            // arbol para arreglar un defecto que no esta aqui.
            const std::optional<std::size_t> own = findTypeInModule(projectOrdinal, candidate);
            if (own.has_value() && (simpleName || nestedTypeIsVisible(*own, environment)))
            {
                const std::array<std::size_t, 1> single{*own};
                appendUnique(found, single);
            }
            break;
        }
        case QualifiedNameTier::ImportedNamespace:
            appendImportedNamespaceTypeCandidates(
                found, projectOrdinal, environment, candidate, simpleName);
            break;
        case QualifiedNameTier::Global:
        {
            const std::optional<std::size_t> global =
                findTypeInModule(projectOrdinal, candidate);
            if (global.has_value())
            {
                const std::array<std::size_t, 1> single{*global};
                appendUnique(found, single);
            }
            break;
        }
        case QualifiedNameTier::ReferencedModule:
            appendReferencedModuleTypeCandidates(
                found, projectOrdinal, candidate, simpleName);
            break;
    }
}

}
