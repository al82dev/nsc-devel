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
    return graphOk && !diagnostics.hasErrors();
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
    const std::string_view identity = typeKey.substr(5);
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        if (nameLookup_->namedTypes()[ordinal].identity.key() == identity)
        {
            return ordinal;
        }
    }
    return std::nullopt;
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

bool ProjectInheritanceModel::resolveBaseSpecifications(
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (nameLookup_ == nullptr)
    {
        return false;
    }

    bool ok = true;
    for (ClassInheritanceInfo& info : classes_)
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[info.typeOrdinal];
        std::optional<std::size_t> selectedBase;
        const ast::TypeSyntax* selectedSyntax = nullptr;

        for (const NamedTypeDeclarationPart& part : type.parts)
        {
            if (part.syntax == nullptr)
            {
                continue;
            }
            std::vector<const ast::TypeSyntax*> specifications;
            if (!part.syntax->typeSyntaxes().empty())
            {
                specifications.assign(
                    part.syntax->typeSyntaxes().begin(),
                    part.syntax->typeSyntaxes().end());
            }
            else if (part.syntax->typeSyntax() != nullptr)
            {
                specifications.push_back(part.syntax->typeSyntax());
            }

            for (const ast::TypeSyntax* baseSyntax : specifications)
            {
                if (baseSyntax == nullptr)
                {
                    continue;
                }
                if (!baseSyntax->modifiers().empty())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidBaseType,
                        diagnostics::DiagnosticSeverity::Error,
                        baseSyntax->range(),
                        "class base/interface specification must name a type directly; pointer and array modifiers are invalid");
                    ok = false;
                    continue;
                }

                const std::optional<std::size_t> candidate = nameLookup_->resolvedTypeOrdinal(baseSyntax);
                if (!candidate.has_value())
                {
                    ok = false;
                    continue;
                }
                if (*candidate >= nameLookup_->namedTypes().size())
                {
                    ok = false;
                    continue;
                }
                const NamedTypeDeclarationKind candidateKind = nameLookup_->namedTypes()[*candidate].kind;
                if (candidateKind == NamedTypeDeclarationKind::Interface)
                {
                    // Stage 36 interface obligations are owned by ProjectInterfaceModel.
                    continue;
                }
                if (candidateKind != NamedTypeDeclarationKind::Class)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidBaseType,
                        diagnostics::DiagnosticSeverity::Error,
                        baseSyntax->range(),
                        "class '" + type.identity.key() + "' may list only one class base followed by interfaces");
                    ok = false;
                    continue;
                }
            if (nameLookup_->namedTypes()[*candidate].isSealed)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidBaseType,
                    diagnostics::DiagnosticSeverity::Error,
                    baseSyntax->range(),
                    "class '" + type.identity.key() + "' cannot derive from sealed class '" +
                        nameLookup_->namedTypes()[*candidate].identity.key() + "'");
                ok = false;
                continue;
            }
            if (*candidate == info.typeOrdinal)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InheritanceCycle,
                    diagnostics::DiagnosticSeverity::Error,
                    baseSyntax->range(),
                    "class '" + type.identity.key() + "' cannot derive directly from itself");
                ok = false;
                continue;
            }

            if (!selectedBase.has_value())
            {
                selectedBase = candidate;
                selectedSyntax = baseSyntax;
            }
            else if (*selectedBase != *candidate)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::PartialTypeConflict,
                    diagnostics::DiagnosticSeverity::Error,
                    baseSyntax->range(),
                    "partial declarations of class '" + type.identity.key() +
                        "' specify different base classes");
                if (selectedSyntax != nullptr)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::PreviousDeclaration,
                        diagnostics::DiagnosticSeverity::Note,
                        selectedSyntax->range(),
                        "previous base-class specification is here");
                }
                ok = false;
            }
            }
        }

        info.baseTypeOrdinal = selectedBase;
        if (selectedBase.has_value())
        {
            info.baseTypeKey = typeKeyForOrdinal(*selectedBase);
        }
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
