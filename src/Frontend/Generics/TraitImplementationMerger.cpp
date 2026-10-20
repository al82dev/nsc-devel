#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../Internal/CloneDepthGuard.h"
#include "../Cache/CachePayloadReader.h"
#include "../Internal/EnumConstResolver.h"
#include "../Internal/SpecializationWaveTimer.h"
#include "../Internal/InterfaceFingerprint.h"
#include "../Internal/ImplementationFingerprint.h"
#include "../Internal/LocatedNode.h"
#include "../Internal/MirFunctionKey.h"
#include "../Internal/MirCacheRecord.h"
#include "../Internal/ProjectGenericDependencyFrame.h"
#include "../Internal/ProjectGenericBodyFrame.h"
#include "../Internal/ProjectGenericAliasFrame.h"
#include "../Internal/ProjectGenericChildWork.h"
#include "../Internal/ProjectGenericCollectFrame.h"
#include "../Internal/ProjectGenericProcessFrame.h"
#include "../Internal/ProjectGenericReservedDemand.h"
#include "../Internal/TypeWork.h"
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
#include "GenericModelSupport.h"
#include "Support/TemplateClassification.h"
#include "Support/TemplateSpelling.h"
#include "Support/ConstGenericArguments.h"
#include "Support/GenericPatterns.h"
#include "Cache/TemplateCatalogCodec.h"
namespace nativesharp::frontend
{
using internal::appendOwner;
using internal::computeTemplateSourceFingerprint;
using internal::constArgumentExpression;
using internal::constGenericParameterTypeKey;
using internal::decodeConstGenericArgument;
using internal::deserializeAstNodeTree;
using internal::enclosingTypeSpellings;
using internal::genericArityAccepts;
using internal::genericAritySpelling;
using internal::genericTemplateAccessibility;
using internal::genericTypeTemplateKind;
using internal::isGenericMethodTemplateNode;
using internal::isGenericTypeTemplateNode;
using internal::isNamedTypeDeclaration;
using internal::isStructuralGenericSurface;
using internal::methodSpecializationFamilyKey;
using internal::normalizeConstExpressionArgument;
using internal::qualifiedName;
using internal::receiverSourceSpelling;
using internal::restoreCatalogEntry;
using internal::serializeAstNodeTree;
using internal::serializeCatalogEntry;
using internal::serializeMirCacheRecord;
using internal::splitDotted;
using internal::stableHashSuffix;
using internal::templateCacheKey;
using internal::templateSourceSpelling;
using internal::trailingPackIndex;
using internal::valueParameterCount;
using internal::CachePayloadReader;
using internal::EnumConstResolver;
using internal::MirCacheRecord;
using internal::SpecializationWaveTimer;
#if 0 // INSTRUMENTATION CODE
using internal::genericNodeDetail;
using internal::genericTypeDetail;
using internal::genericUnitDetail;
using internal::GenericWorkProgress;
#endif // INSTRUMENTATION CODE
void ProjectGenericModel::collectTraitImplementationNodes(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& parentNamespace,
    std::vector<LocatedNode>& templates,
    std::vector<LocatedNode>& implementations)
{
    if (node == nullptr) return;
    std::string currentNamespace = parentNamespace;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(parentNamespace, node->text());
    if (node->kind() == ast::AstNodeKind::ClassDeclaration &&
        node->auxiliaryText() == "ns3-trait-impl" && !node->genericParameters().empty())
        implementations.push_back(LocatedNode{&unit, node, currentNamespace, {}});
    else if (node->kind() == ast::AstNodeKind::ClassDeclaration &&
             !node->genericParameters().empty())
        templates.push_back(LocatedNode{&unit, node, currentNamespace, {}});
    for (ast::AstNode* child : node->children())
        collectTraitImplementationNodes(
            unit, child, currentNamespace, templates, implementations);
}

std::string ProjectGenericModel::genericParameterShape(const ast::AstNode* node) const
{
    std::string shape;
    for (const ast::GenericParameterSyntax& parameter : node->genericParameterSyntaxes())
    {
        shape += std::to_string(static_cast<unsigned>(parameter.kind));
        shape += ':';
        shape += parameter.name;
        shape += ':';
        shape += typeSpelling(parameter.constantType);
        shape += ';';
    }
    return shape;
}

std::string ProjectGenericModel::genericConstraintShape(const ast::AstNode* node) const
{
    std::string shape;
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        shape += clause.parameterName + ':';
        shape += clause.requiresClass ? 'C' : '-';
        shape += clause.requiresValueType ? 'V' : '-';
        shape += clause.requiresDefaultConstructor ? 'N' : '-';
        shape += clause.requiresCopy ? 'P' : '-';
        shape += clause.requiresMove ? 'M' : '-';
        shape += clause.requiresTrackedLifetime ? 'L' : '-';
        shape += clause.requiresUnmanaged ? 'U' : '-';
        shape += clause.requiresNotNull ? 'R' : '-';
        shape += '=' + typeSpelling(clause.equalityConstraint);
        for (const ast::TypeSyntax* type : clause.typeConstraints)
            shape += ',' + typeSpelling(type);
        shape += ';';
    }
    return shape;
}

ast::AstNode* ProjectGenericModel::findTraitImplementationOwner(
    const std::vector<LocatedNode>& templates,
    const LocatedNode& implementation) const
{
    for (const LocatedNode& candidate : templates)
    {
        if (candidate.unit->projectOrdinal() == implementation.unit->projectOrdinal() &&
            candidate.namespaceName == implementation.namespaceName &&
            candidate.syntax->text() == implementation.syntax->text() &&
            candidate.syntax->genericParameters().size() ==
                implementation.syntax->genericParameters().size())
            return candidate.syntax;
    }
    return nullptr;
}

bool ProjectGenericModel::traitImplementationHasDirectTarget(
    const ast::AstNode* implementation,
    const ast::TypeSyntax* target)
{
    if (target == nullptr ||
        target->typeArguments().size() != implementation->genericParameters().size())
        return false;
    for (std::size_t index = 0; index < target->typeArguments().size(); ++index)
    {
        const ast::TypeSyntax* argument = target->typeArguments()[index];
        if (argument == nullptr || !argument->typeArguments().empty() ||
            !argument->modifiers().empty() ||
            argument->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None ||
            argument->baseName() != implementation->genericParameters()[index])
            return false;
    }
    return true;
}

bool ProjectGenericModel::mergeTraitImplementation(
    const std::vector<LocatedNode>& templates,
    const LocatedNode& implementation,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ast::AstNode* impl = implementation.syntax;
    const ast::TypeSyntax* target = impl->second() == nullptr
        ? nullptr : impl->second()->typeSyntax();
    ast::AstNode* owner = findTraitImplementationOwner(templates, implementation);
    if (owner == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidBaseType,
            diagnostics::DiagnosticSeverity::Error,
            impl->range(),
            "sealed trait coherence rejects this generic impl: target class '" +
                impl->text() + "' is not declared by this module and namespace");
        return false;
    }
    if (!traitImplementationHasDirectTarget(impl, target) ||
        genericParameterShape(owner) != genericParameterShape(impl) ||
        genericConstraintShape(owner) != genericConstraintShape(impl))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidBaseType,
            diagnostics::DiagnosticSeverity::Error,
            impl->range(),
            "a conditional trait impl must repeat the target class generic parameters and "
            "where constraints exactly");
        return false;
    }

    const std::string trait = typeSpelling(impl->typeSyntax());
    const ast::TypeSyntax* duplicate = nullptr;
    for (const ast::TypeSyntax* base : owner->typeSyntaxes())
        if (typeSpelling(base) == trait)
        {
            duplicate = base;
            break;
        }
    if (duplicate != nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            impl->typeSyntax()->range(),
            "duplicate trait impl for '" + trait + "' on generic class '" + owner->text() + "'");
        diagnostics.report(
            diagnostics::DiagnosticId::PreviousDeclaration,
            diagnostics::DiagnosticSeverity::Note,
            duplicate->range(),
            "previous trait implementation is here");
        return false;
    }
    owner->addTypeSyntax(impl->typeSyntax());
    if (owner->typeSyntax() == nullptr) owner->setTypeSyntax(impl->typeSyntax());
    for (ast::AstNode* member : impl->children()) owner->addChild(member);
    traitImplExtensions_[owner].push_back(impl);
    return true;
}

bool ProjectGenericModel::mergeTraitImplementations(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<LocatedNode> templates;
    std::vector<LocatedNode> implementations;
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
        if (unit != nullptr)
            collectTraitImplementationNodes(
                *unit, unit->syntaxTree(), "", templates, implementations);
    bool ok = true;
    for (const LocatedNode& implementation : implementations)
        if (!mergeTraitImplementation(templates, implementation, diagnostics)) ok = false;
    return ok;
}

bool ProjectGenericModel::registerAssociatedItems(
    const ast::AstNode* owner,
    std::string_view ownerName,
    diagnostics::DiagnosticEngine* diagnostics)
{
    if (owner == nullptr || ownerName.empty()) return true;
    bool ok = true;
    for (const ast::AstNode* item : owner->children())
    {
        if (item == nullptr) continue;
        const bool associatedType =
            item->kind() == ast::AstNodeKind::AssociatedTypeDeclaration;
        const bool associatedConstant =
            item->kind() == ast::AstNodeKind::AssociatedConstantDeclaration;
        if ((!associatedType && !associatedConstant) ||
            (associatedType && item->typeSyntax() == nullptr) ||
            (associatedConstant && item->first() == nullptr))
            continue;

        const std::string key = std::string(ownerName) + "." + item->text();
        const ast::AstNode* previous = nullptr;
        if (associatedType)
        {
            const auto [position, inserted] = associatedTypes_.emplace(key, item->typeSyntax());
            if (inserted || position->second == item->typeSyntax()) continue;
            for (const ast::AstNode* candidate : owner->children())
            {
                if (candidate != nullptr && candidate != item &&
                    candidate->kind() == ast::AstNodeKind::AssociatedTypeDeclaration &&
                    candidate->text() == item->text() &&
                    candidate->typeSyntax() == position->second)
                {
                    previous = candidate;
                    break;
                }
            }
        }
        else
        {
            const auto [position, inserted] = associatedConstants_.emplace(key, item);
            if (inserted || position->second == item) continue;
            previous = position->second;
        }

        if (diagnostics != nullptr)
        {
            diagnostics->report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                item->range(),
                "duplicate associated item '" + key + "'");
            if (previous != nullptr)
            {
                diagnostics->report(
                    diagnostics::DiagnosticId::PreviousDeclaration,
                    diagnostics::DiagnosticSeverity::Note,
                    previous->range(),
                    "previous associated item definition is here");
            }
        }
        ok = false;
    }
    return ok;
}
}
