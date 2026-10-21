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
// NSC-LIMIT-009. Que un tipo cruce el limite de modulo. Para uno de namespace es la pregunta de
// siempre --`public`-- y para un ANIDADO son todas: `public class Outer { public struct Item }`
// exporta `Item`, y `class Outer { public struct Item }` no exporta nada, porque el contenedor no
// sale. Sin esta cadena, un `public` interior habria colado al anidado por encima de su
// contenedor internal.
bool ProjectNameLookup::typeIsVisibleTo(
    std::size_t accessingProjectOrdinal,
    std::size_t typeOrdinal) const
{
    // NSC-LIMIT-009, segunda vuelta. `protected` TAMBIEN CRUZA -- o mejor dicho, tiene derecho a
    // que se le pregunte. Un anidado protegido de un contenedor public es alcanzable desde una
    // clase derivada, y esa clase puede estar en OTRO proyecto: es el caso normal de una
    // biblioteca que declara `protected class Reservada` para que sus derivadas la usen.
    //
    // La version anterior lo descartaba aqui, con lo cual el nombre no llegaba a producir un
    // binding resuelto y `checkProtectedNestedTypeUses` --que es quien sabe decidirlo-- no
    // recibia nunca la oportunidad. `private` e `internal` si cortan: no hay ninguna fase
    // posterior que pueda convertirlos en alcanzables desde fuera del modulo.
    //
    // El ESLABON SUPERIOR se exige public aparte, en el bucle: un contenedor `protected` de
    // namespace no existe --solo un anidado puede serlo-- asi que un tipo raiz que no sea public
    // no sale del modulo por ningun camino.
    std::optional<std::size_t> current = typeOrdinal;
    while (current.has_value() && *current < namedTypes_.size())
    {
        const NamedTypeDeclaration& type = namedTypes_[*current];
        const bool nested = type.containingTypeOrdinal.has_value();
        bool crosses = type.accessibility == NamedTypeAccessibility::Public ||
            (nested && type.accessibility == NamedTypeAccessibility::Protected);
        if (!nested && type.projectOrdinal < packageByProject_.size() &&
            accessingProjectOrdinal < packageByProject_.size())
        {
            const auto fold = [](std::string value)
            {
                std::ranges::transform(value, value.begin(), [](unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                });
                return value;
            };
            if (type.accessibility == NamedTypeAccessibility::Package &&
                fold(packageByProject_[type.projectOrdinal]) ==
                    fold(packageByProject_[accessingProjectOrdinal]))
            {
                crosses = true;
            }
            if (type.accessibility == NamedTypeAccessibility::Internal &&
                type.projectOrdinal < friendsByProject_.size() &&
                accessingProjectOrdinal < modules_.size())
            {
                const std::string accessing = fold(modules_[accessingProjectOrdinal]->moduleName());
                crosses = std::ranges::any_of(
                    friendsByProject_[type.projectOrdinal],
                    [&](const std::string& name) { return fold(name) == accessing; });
            }
        }
        if (!crosses)
        {
            return false;
        }
        current = type.containingTypeOrdinal;
    }
    return true;
}

bool ProjectNameLookup::typeIsVisibleFromProject(
    std::size_t accessingProjectOrdinal,
    std::size_t typeOrdinal) const
{
    if (typeOrdinal >= namedTypes_.size()) return false;
    if (namedTypes_[typeOrdinal].projectOrdinal == accessingProjectOrdinal) return true;
    return typeIsVisibleTo(accessingProjectOrdinal, typeOrdinal);
}

bool ProjectNameLookup::nestedTypeIsVisibleFrom(
    std::size_t typeOrdinal,
    std::span<const std::size_t> enclosingTypeOrdinals) const noexcept
{
    // Se recorre la cadena de contenedores hacia fuera. Basta con que UNO de ellos declare el
    // eslabon como `private` y no este entre los contenedores lexicos del sitio de uso para que
    // el nombre no se vea: `Outer.Privada.Item` no es alcanzable desde fuera de `Outer` aunque
    // `Item` sea public dentro de `Privada`.
    std::optional<std::size_t> current = typeOrdinal;
    while (current.has_value() && *current < namedTypes_.size())
    {
        const NamedTypeDeclaration& type = namedTypes_[*current];
        const std::optional<std::size_t> container = type.containingTypeOrdinal;
        if (!container.has_value()) return true;
        if (type.accessibility == NamedTypeAccessibility::Private &&
            std::ranges::find(enclosingTypeOrdinals, *container) ==
                enclosingTypeOrdinals.end())
        {
            return false;
        }
        // `Protected` NO se decide aqui, y no por comodidad: la pregunta es «el tipo que escribe
        // esto deriva del contenedor?», y el modelo de herencia se construye DESPUES de resolver
        // nombres. Responder aqui obligaria a una de dos mentiras: tratarlo como private
        // --rechazando el uso legitimo desde una derivada-- o como internal, que deja pasar el
        // modulo entero. Se acepta, se guarda la cadena del sitio en el binding, y lo comprueba
        // ProjectInheritanceModel, que es la primera fase que tiene la respuesta.
        current = container;
    }
    return true;
}

std::optional<std::size_t> ProjectNameLookup::findTypeInModule(
    std::size_t moduleOrdinal,
    const std::string& dottedName) const
{
    if (moduleOrdinal >= typesByModule_.size())
    {
        return std::nullopt;
    }
    const auto canonical = typesByModule_[moduleOrdinal].find(dottedName);
    if (canonical != typesByModule_[moduleOrdinal].end())
    {
        return canonical->second;
    }
    const auto nested = nestedSourceNamesByModule_[moduleOrdinal].find(dottedName);
    if (nested != nestedSourceNamesByModule_[moduleOrdinal].end())
    {
        return nested->second;
    }
    if (moduleOrdinal < reexportedTypesByModule_.size())
    {
        const auto facade = reexportedTypesByModule_[moduleOrdinal].find(dottedName);
        if (facade != reexportedTypesByModule_[moduleOrdinal].end()) return facade->second;
    }
    return std::nullopt;
}

bool ProjectNameLookup::nestedTypeIsVisible(
    std::size_t typeOrdinal,
    const LookupEnvironment& environment) const
{
    return nestedTypeIsVisibleFrom(typeOrdinal, environment.enclosingTypeOrdinals);
}

std::vector<std::size_t> ProjectNameLookup::findExactTypeCandidates(
    std::size_t projectOrdinal,
    const std::string& fullyQualifiedName,
    bool currentModuleFirst) const
{
    if (projectOrdinal >= typesByModule_.size())
    {
        return {};
    }

    const auto current = typesByModule_[projectOrdinal].find(fullyQualifiedName);
    if (currentModuleFirst && current != typesByModule_[projectOrdinal].end())
    {
        return {current->second};
    }

    std::vector<std::size_t> candidates;
    if (current != typesByModule_[projectOrdinal].end())
    {
        candidates.push_back(current->second);
    }
    for (const std::size_t visibleOrdinal : modules_[projectOrdinal]->visibleProjectOrdinals())
    {
        const auto iterator = typesByModule_[visibleOrdinal].find(fullyQualifiedName);
        if (iterator != typesByModule_[visibleOrdinal].end() &&
            typeIsVisibleTo(projectOrdinal, iterator->second))
        {
            candidates.push_back(iterator->second);
        }
    }
    return candidates;
}

}
