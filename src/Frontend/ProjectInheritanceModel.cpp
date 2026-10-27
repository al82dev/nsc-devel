#include "nativesharp/Frontend/ProjectInheritanceModel.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{
bool ProjectInheritanceModel::build(
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (built_ || nameLookup_ != nullptr || !classes_.empty() || !classInfoByType_.empty())
    {
        return false;
    }
    if (!nameLookup.isBuilt())
    {
        return false;
    }

    nameLookup_ = &nameLookup;
    classInfoByType_.resize(nameLookup.namedTypes().size());
    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
    {
        if (nameLookup.namedTypes()[typeOrdinal].kind != NamedTypeDeclarationKind::Class)
        {
            continue;
        }
        classInfoByType_[typeOrdinal] = classes_.size();
        ClassInheritanceInfo info;
        info.typeOrdinal = typeOrdinal;
        classes_.push_back(std::move(info));
    }

    const bool basesOk = resolveBaseSpecifications(diagnostics);
    std::vector<VisitState> states(nameLookup.namedTypes().size(), VisitState::Unvisited);
    bool graphOk = basesOk;
    for (const ClassInheritanceInfo& info : classes_)
    {
        if (!visitClass(info.typeOrdinal, states, diagnostics))
        {
            graphOk = false;
        }
    }

    for (const ClassInheritanceInfo& info : classes_)
    {
        if (!info.baseTypeOrdinal.has_value())
        {
            continue;
        }
        if (*info.baseTypeOrdinal < classInfoByType_.size() &&
            classInfoByType_[*info.baseTypeOrdinal].has_value())
        {
            classes_[*classInfoByType_[*info.baseTypeOrdinal]].directlyDerivedTypeOrdinals.push_back(
                info.typeOrdinal);
        }
    }

    built_ = true;
    checkProtectedNestedTypeUses(diagnostics);
    return graphOk && !diagnostics.hasErrors();
}

std::optional<std::size_t> ProjectInheritanceModel::firstUnreachableProtectedLink(
    std::size_t typeOrdinal,
    std::span<const std::size_t> enclosingTypes) const
{
    // Hacia fuera por la cadena de contenedores: basta con que UN eslabon protegido quede fuera
    // de alcance para que el nombre entero no lo este, aunque el ultimo sea public dentro de su
    // contenedor.
    std::optional<std::size_t> current = typeOrdinal;
    while (current.has_value() && *current < nameLookup_->namedTypes().size())
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[*current];
        const std::optional<std::size_t> container = type.containingTypeOrdinal;
        if (!container.has_value())
        {
            return std::nullopt;
        }
        if (type.accessibility == NamedTypeAccessibility::Protected)
        {
            bool reachable = false;
            for (const std::size_t enclosing : enclosingTypes)
            {
                if (enclosing == *container || isDerivedFrom(enclosing, *container, false))
                {
                    reachable = true;
                    break;
                }
            }
            if (!reachable)
            {
                return current;
            }
        }
        current = container;
    }
    return std::nullopt;
}

void ProjectInheritanceModel::checkProtectedNestedTypeUses(
    diagnostics::DiagnosticEngine& diagnostics)
{
    // Un anidado `protected` se ve desde su contenedor, desde los tipos anidados en el, y desde
    // cualquier tipo que DERIVE del contenedor. Esa tercera es la que obliga a esperar hasta
    // aqui, y por eso la resolucion de nombres dejo pasar el nombre y guardo en el binding la
    // cadena de tipos del sitio: lo unico que faltaba era la relacion de herencia.
    //
    // Se recorren los BINDINGS y no las declaraciones porque la pregunta es por USO: el mismo
    // tipo protegido puede ser legitimo en una linea e ilegitimo en la siguiente.
    const auto report = [&](std::size_t blockedOrdinal, SourceRange range)
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[blockedOrdinal];
        const std::size_t container = *type.containingTypeOrdinal;
        diagnostics.report(
            diagnostics::DiagnosticId::UnknownTypeName,
            diagnostics::DiagnosticSeverity::Error,
            range,
            "nested type '" + type.identity.fullyQualifiedName() +
                "' is protected and is not accessible here; it is visible inside '" +
                nameLookup_->namedTypes()[container].identity.fullyQualifiedName() +
                "' and inside types derived from it");
    };

    for (const TypeNameBinding& binding : nameLookup_->bindings())
    {
        if (binding.kind != TypeNameBindingKind::Resolved || !binding.targetTypeOrdinal.has_value())
        {
            continue;
        }
        const std::optional<std::size_t> blocked = firstUnreachableProtectedLink(
            *binding.targetTypeOrdinal, binding.enclosingTypeOrdinals);
        if (blocked.has_value())
        {
            report(*blocked, binding.syntax != nullptr ? binding.syntax->range() : SourceRange{});
        }
    }

    // Y LOS RECEPTORES DE EXPRESION, que es por donde se colaba.
    //
    // `Base.Reservada.Codigo()` nombra el tipo sin escribir un TypeSyntax: el receptor es una
    // expresion, y produce un ExpressionTypeNameBinding, no un TypeNameBinding. Como la
    // resolucion de nombres deja pasar `protected` a proposito --esperando esta comprobacion-- y
    // esta comprobacion solo miraba los TypeNameBinding, un tipo ajeno alcanzaba un anidado
    // protegido con solo llamar a un miembro estatico.
    //
    // Un binding de expresion puede tener VARIOS candidatos, porque el receptor todavia no se ha
    // resuelto a uno. Basta con que uno sea alcanzable para no decir nada: elegir entre ellos es
    // trabajo del resolvedor de miembros, y adelantarse aqui convertiria una ambiguedad en un
    // error de proteccion que no es.
    for (const ExpressionTypeNameBinding& binding : nameLookup_->expressionTypeBindings())
    {
        if (binding.candidateTypeOrdinals.empty())
        {
            continue;
        }
        std::optional<std::size_t> firstBlocked;
        bool anyReachable = false;
        for (const std::size_t candidate : binding.candidateTypeOrdinals)
        {
            const std::optional<std::size_t> blocked =
                firstUnreachableProtectedLink(candidate, binding.enclosingTypeOrdinals);
            if (!blocked.has_value())
            {
                anyReachable = true;
                break;
            }
            if (!firstBlocked.has_value())
            {
                firstBlocked = blocked;
            }
        }
        if (!anyReachable && firstBlocked.has_value())
        {
            report(
                *firstBlocked,
                binding.expression != nullptr ? binding.expression->range() : SourceRange{});
        }
    }
}

std::span<const ClassInheritanceInfo> ProjectInheritanceModel::classes() const noexcept
{
    return classes_;
}

const ClassInheritanceInfo* ProjectInheritanceModel::classInfo(std::size_t typeOrdinal) const noexcept
{
    if (typeOrdinal >= classInfoByType_.size() || !classInfoByType_[typeOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *classInfoByType_[typeOrdinal];
    return index < classes_.size() ? &classes_[index] : nullptr;
}

std::optional<std::size_t> ProjectInheritanceModel::baseTypeOrdinal(std::size_t typeOrdinal) const noexcept
{
    const ClassInheritanceInfo* info = classInfo(typeOrdinal);
    return info != nullptr ? info->baseTypeOrdinal : std::nullopt;
}

bool ProjectInheritanceModel::isClass(std::size_t typeOrdinal) const noexcept
{
    return classInfo(typeOrdinal) != nullptr;
}

bool ProjectInheritanceModel::isDerivedFrom(
    std::size_t derivedTypeOrdinal,
    std::size_t targetBaseTypeOrdinal,
    bool includeIdentity) const noexcept
{
    if (includeIdentity && derivedTypeOrdinal == targetBaseTypeOrdinal)
    {
        return isClass(derivedTypeOrdinal);
    }

    std::optional<std::size_t> current = this->baseTypeOrdinal(derivedTypeOrdinal);
    while (current.has_value())
    {
        if (*current == targetBaseTypeOrdinal)
        {
            return true;
        }
        current = this->baseTypeOrdinal(*current);
    }
    return false;
}

bool ProjectInheritanceModel::protectedNestedTypeIsAccessibleFrom(
    std::size_t typeOrdinal,
    std::span<const std::size_t> enclosingTypeOrdinals) const noexcept
{
    return !firstUnreachableProtectedLink(
        typeOrdinal, enclosingTypeOrdinals).has_value();
}

std::optional<std::size_t> ProjectInheritanceModel::inheritanceDistance(
    std::size_t derivedTypeOrdinal,
    std::size_t targetBaseTypeOrdinal) const noexcept
{
    if (derivedTypeOrdinal == targetBaseTypeOrdinal && isClass(derivedTypeOrdinal))
    {
        return 0;
    }

    std::size_t distance = 0;
    std::optional<std::size_t> current = derivedTypeOrdinal;
    while (current.has_value())
    {
        const ClassInheritanceInfo* info = classInfo(*current);
        if (info == nullptr || !info->baseTypeOrdinal.has_value())
        {
            return std::nullopt;
        }
        ++distance;
        current = info->baseTypeOrdinal;
        if (*current == targetBaseTypeOrdinal)
        {
            return distance;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> ProjectInheritanceModel::typeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:"))
    {
        return std::nullopt;
    }
    return nameLookup_->typeOrdinalForIdentity(typeKey.substr(5));
}

std::string ProjectInheritanceModel::typeKeyForOrdinal(std::size_t typeOrdinal) const
{
    if (nameLookup_ == nullptr || typeOrdinal >= nameLookup_->namedTypes().size())
    {
        return "<error>";
    }
    return "type:" + nameLookup_->namedTypes()[typeOrdinal].identity.key();
}

bool ProjectInheritanceModel::isBuilt() const noexcept
{
    return built_;
}

std::optional<std::size_t> ProjectInheritanceModel::objectRootTypeOrdinal() const
{
    // C8 "type root": the ordinal of NativeSharp.Core.Object, when the compilation has it in
    // view. Looked up once here rather than per class.
    //
    // Why the root is OPTIONAL rather than always present: it is a declared type like any other,
    // and a project that does not reference the standard library (or declare its own) has no
    // Object to derive from. Inventing one would change the vtable, the descriptor and the
    // constructor chain of every class in every such project for a facility it cannot use. This
    // follows exactly the convention NativeSharp.Core.Exception already set for typed
    // catch/throw -- see ProjectMethodBodyModel::exceptionRootTypeOrdinal.
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& candidate = nameLookup_->namedTypes()[ordinal];
        if (candidate.kind == NamedTypeDeclarationKind::Class &&
            candidate.identity.fullyQualifiedName() == "NativeSharp.Core.Object")
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

bool ProjectInheritanceModel::validateBaseClassDerivation(
    const NamedTypeDeclaration& type,
    const ClassInheritanceInfo& info,
    const NamedTypeDeclaration& candidateType,
    std::size_t candidateOrdinal,
    const ast::TypeSyntax* baseSyntax,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (candidateType.isSealed)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidBaseType,
            diagnostics::DiagnosticSeverity::Error,
            baseSyntax->range(),
            "class '" + type.identity.key() + "' cannot derive from sealed class '" +
                candidateType.identity.key() + "'");
        return false;
    }
    if (candidateType.isStatic)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidBaseType,
            diagnostics::DiagnosticSeverity::Error,
            baseSyntax->range(),
            "class '" + type.identity.key() + "' cannot derive from static class '" +
                candidateType.identity.key() + "'");
        return false;
    }
    if (candidateOrdinal == info.typeOrdinal)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InheritanceCycle,
            diagnostics::DiagnosticSeverity::Error,
            baseSyntax->range(),
            "class '" + type.identity.key() + "' cannot derive directly from itself");
        return false;
    }
    return true;
}

bool ProjectInheritanceModel::resolveBaseSpecification(
    const NamedTypeDeclaration& type,
    const ClassInheritanceInfo& info,
    const ast::TypeSyntax* baseSyntax,
    std::optional<std::size_t>& selectedBase,
    const ast::TypeSyntax*& selectedSyntax,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (baseSyntax == nullptr) return true;
    if (!baseSyntax->modifiers().empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidBaseType,
            diagnostics::DiagnosticSeverity::Error,
            baseSyntax->range(),
            "class base/interface specification must name a type directly; pointer and array modifiers are invalid");
        return false;
    }
    const std::optional<std::size_t> candidate = nameLookup_->resolvedTypeOrdinal(baseSyntax);
    if (!candidate.has_value() || *candidate >= nameLookup_->namedTypes().size()) return false;
    const NamedTypeDeclaration& candidateType = nameLookup_->namedTypes()[*candidate];
    if (candidateType.kind == NamedTypeDeclarationKind::Interface)
    {
        // Stage 36 interface obligations are owned by ProjectInterfaceModel.
        return true;
    }
    if (candidateType.kind != NamedTypeDeclarationKind::Class)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidBaseType,
            diagnostics::DiagnosticSeverity::Error,
            baseSyntax->range(),
            "class '" + type.identity.key() +
                "' may list only one class base followed by interfaces");
        return false;
    }
    if (!validateBaseClassDerivation(
            type, info, candidateType, *candidate, baseSyntax, diagnostics))
        return false;
    if (!selectedBase.has_value())
    {
        selectedBase = candidate;
        selectedSyntax = baseSyntax;
        return true;
    }
    if (*selectedBase == *candidate) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::PartialTypeConflict,
        diagnostics::DiagnosticSeverity::Error,
        baseSyntax->range(),
        "partial declarations of class '" + type.identity.key() +
            "' specify different base classes");
    if (selectedSyntax != nullptr)
        diagnostics.report(
            diagnostics::DiagnosticId::PreviousDeclaration,
            diagnostics::DiagnosticSeverity::Note,
            selectedSyntax->range(),
            "previous base-class specification is here");
    return false;
}

bool ProjectInheritanceModel::resolveClassBaseSpecifications(
    ClassInheritanceInfo& info,
    std::optional<std::size_t> objectRootOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[info.typeOrdinal];
    std::optional<std::size_t> selectedBase;
    const ast::TypeSyntax* selectedSyntax = nullptr;
    bool ok = true;
    for (const NamedTypeDeclarationPart& part : type.parts)
    {
        if (part.syntax == nullptr) continue;
        std::vector<const ast::TypeSyntax*> specifications;
        if (!part.syntax->typeSyntaxes().empty())
            specifications.assign(
                part.syntax->typeSyntaxes().begin(), part.syntax->typeSyntaxes().end());
        else if (part.syntax->typeSyntax() != nullptr)
            specifications.push_back(part.syntax->typeSyntax());
        for (const ast::TypeSyntax* baseSyntax : specifications)
            if (!resolveBaseSpecification(
                    type, info, baseSyntax, selectedBase, selectedSyntax, diagnostics))
                ok = false;
    }

    // C8 "type root": a class that named no base derives from Object -- except Object
    // itself, which is where the chain has to stop. Every other consumer of the hierarchy
    // (the vtable builder, the descriptor emitter, `base.` lookup, the constructor chain, the
    // RTTI walk) then sees an ordinary base class and needs no special case at all: the root
    // is a real class, not a compiler fiction.
    if (!selectedBase.has_value() && objectRootOrdinal.has_value() &&
        info.typeOrdinal != *objectRootOrdinal)
        selectedBase = objectRootOrdinal;

    info.baseTypeOrdinal = selectedBase;
    if (selectedBase.has_value()) info.baseTypeKey = typeKeyForOrdinal(*selectedBase);
    return ok;
}

bool ProjectInheritanceModel::resolveBaseSpecifications(
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (nameLookup_ == nullptr) return false;
    const std::optional<std::size_t> objectRootOrdinal = objectRootTypeOrdinal();
    bool ok = true;
    for (ClassInheritanceInfo& info : classes_)
    {
        // EN: A class of a restored API unit with a snapshot entry takes its base from it (plan 3.6).
        // ES: Una clase de una unidad de API restaurada con entrada en el snapshot toma de ella su base
        //     (plan 3.6).
        if (installSnapshotBase(info)) continue;
        if (!resolveClassBaseSpecifications(info, objectRootOrdinal, diagnostics)) ok = false;
        captureSnapshotBase(info);
    }
    return ok;
}

bool ProjectInheritanceModel::visitClass(
    std::size_t typeOrdinal,
    std::vector<VisitState>& states,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (typeOrdinal >= states.size())
    {
        return false;
    }
    if (states[typeOrdinal] == VisitState::Complete)
    {
        return true;
    }
    if (states[typeOrdinal] == VisitState::Failed)
    {
        return false;
    }
    if (states[typeOrdinal] == VisitState::Visiting)
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[typeOrdinal];
        diagnostics.report(
            diagnostics::DiagnosticId::InheritanceCycle,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "inheritance cycle reaches class '" + type.identity.key() + "'");
        states[typeOrdinal] = VisitState::Failed;
        return false;
    }

    if (typeOrdinal >= classInfoByType_.size() || !classInfoByType_[typeOrdinal].has_value())
    {
        return true;
    }
    ClassInheritanceInfo& info = classes_[*classInfoByType_[typeOrdinal]];

    states[typeOrdinal] = VisitState::Visiting;
    if (info.baseTypeOrdinal.has_value())
    {
        if (!visitClass(*info.baseTypeOrdinal, states, diagnostics))
        {
            states[typeOrdinal] = VisitState::Failed;
            return false;
        }
        const ClassInheritanceInfo* base = classInfo(*info.baseTypeOrdinal);
        info.depth = base != nullptr ? base->depth + 1 : 1;
    }
    else
    {
        info.depth = 0;
    }
    states[typeOrdinal] = VisitState::Complete;
    return true;
}
}
