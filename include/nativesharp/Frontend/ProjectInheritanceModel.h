#pragma once

#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ClassInheritanceInfo.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess;

class ProjectInheritanceModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const ClassInheritanceInfo> classes() const noexcept;
    const ClassInheritanceInfo* classInfo(std::size_t typeOrdinal) const noexcept;
    std::optional<std::size_t> baseTypeOrdinal(std::size_t typeOrdinal) const noexcept;
    bool isClass(std::size_t typeOrdinal) const noexcept;
    bool isDerivedFrom(
        std::size_t derivedTypeOrdinal,
        std::size_t targetBaseTypeOrdinal,
        bool includeIdentity = false) const noexcept;
    std::optional<std::size_t> inheritanceDistance(
        std::size_t derivedTypeOrdinal,
        std::size_t targetBaseTypeOrdinal) const noexcept;
    std::optional<std::size_t> typeOrdinalForTypeKey(std::string_view typeKey) const noexcept;
    std::string typeKeyForOrdinal(std::size_t typeOrdinal) const;
    bool isBuilt() const noexcept;

private:
    friend struct ProjectOopHirModelTestAccess;

    // NSC-LIMIT-009. `protected` sobre un tipo ANIDADO, comprobado aqui y no en la resolucion de
    // nombres. Es la primera fase que puede responder «el sitio de uso deriva del contenedor?»,
    // que es literalmente lo que la palabra significa; antes de la herencia no hay con que.
    void checkProtectedNestedTypeUses(diagnostics::DiagnosticEngine& diagnostics);
    // El eslabon protegido de la cadena de `typeOrdinal` que NO alcanza ninguno de los tipos
    // lexicos dados, o nullopt si el nombre entero es alcanzable. Devuelve el eslabon --y no un
    // bool-- porque el diagnostico tiene que nombrar el contenedor concreto que cierra el paso.
    std::optional<std::size_t> firstUnreachableProtectedLink(
        std::size_t typeOrdinal,
        std::span<const std::size_t> enclosingTypes) const;
    enum class VisitState
    {
        Unvisited,
        Visiting,
        Complete,
        Failed
    };

    bool resolveBaseSpecifications(diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> objectRootTypeOrdinal() const;
    bool validateBaseClassDerivation(
        const NamedTypeDeclaration& type,
        const ClassInheritanceInfo& info,
        const NamedTypeDeclaration& candidateType,
        std::size_t candidateOrdinal,
        const ast::TypeSyntax* baseSyntax,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool resolveBaseSpecification(
        const NamedTypeDeclaration& type,
        const ClassInheritanceInfo& info,
        const ast::TypeSyntax* baseSyntax,
        std::optional<std::size_t>& selectedBase,
        const ast::TypeSyntax*& selectedSyntax,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool resolveClassBaseSpecifications(
        ClassInheritanceInfo& info,
        std::optional<std::size_t> objectRootOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool visitClass(
        std::size_t typeOrdinal,
        std::vector<VisitState>& states,
        diagnostics::DiagnosticEngine& diagnostics);

    const ProjectNameLookup* nameLookup_ = nullptr;
    std::vector<ClassInheritanceInfo> classes_;
    std::vector<std::optional<std::size_t>> classInfoByType_;
    bool built_ = false;
};
}
