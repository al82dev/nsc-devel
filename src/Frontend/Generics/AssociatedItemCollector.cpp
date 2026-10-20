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
void ProjectGenericModel::collectAssociatedItemNodes(
    ast::AstNode* node,
    const std::string& parentNamespace,
    const std::string& parentOwner,
    std::vector<LocatedNode>& traits,
    std::vector<LocatedNode>& owners)
{
    if (node == nullptr) return;
    std::string currentNamespace = parentNamespace;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(parentNamespace, node->text());
    std::string currentOwner = parentOwner;
    if (node->kind() == ast::AstNodeKind::ClassDeclaration ||
        node->kind() == ast::AstNodeKind::StructDeclaration ||
        node->kind() == ast::AstNodeKind::InterfaceDeclaration)
        currentOwner = namespaceAppend(parentOwner, node->text());
    if (node->kind() == ast::AstNodeKind::InterfaceDeclaration &&
        (node->auxiliaryText() == "ns3-trait" ||
         node->auxiliaryText().find("nsc-meta:ns3-trait") != std::string::npos))
        traits.push_back(LocatedNode{nullptr, node, currentNamespace, currentOwner});
    else if (node->kind() == ast::AstNodeKind::ClassDeclaration ||
             node->kind() == ast::AstNodeKind::StructDeclaration)
        owners.push_back(LocatedNode{nullptr, node, currentNamespace, currentOwner});
    for (ast::AstNode* child : node->children())
        collectAssociatedItemNodes(child, currentNamespace, currentOwner, traits, owners);
}

void ProjectGenericModel::collectAssociatedItemUnits(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    std::vector<LocatedNode>& traits,
    std::vector<LocatedNode>& owners)
{
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        if (unit == nullptr) continue;
        const std::size_t traitBegin = traits.size();
        const std::size_t ownerBegin = owners.size();
        collectAssociatedItemNodes(unit->syntaxTree(), "", "", traits, owners);
        for (std::size_t index = traitBegin; index < traits.size(); ++index)
            traits[index].unit = unit.get();
        for (std::size_t index = ownerBegin; index < owners.size(); ++index)
            owners[index].unit = unit.get();
    }
}

bool ProjectGenericModel::indexTraitAssociatedItems(
    const std::vector<LocatedNode>& traits,
    std::unordered_map<std::string, const ast::AstNode*>& traitTypes,
    std::unordered_map<std::string, const ast::AstNode*>& traitConstants,
    std::unordered_map<std::string, const LocatedNode*>& traitsByName,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    for (const LocatedNode& trait : traits)
    {
        const std::string traitName = namespaceAppend(trait.namespaceName, trait.ownerName);
        traitsByName.emplace(traitName, &trait);
        std::unordered_set<std::string> localNames;
        for (const ast::AstNode* item : trait.syntax->children())
        {
            if (item == nullptr ||
                (item->kind() != ast::AstNodeKind::AssociatedTypeDeclaration &&
                 item->kind() != ast::AstNodeKind::AssociatedConstantDeclaration))
                continue;
            if (!localNames.insert(item->text()).second)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::DuplicateSymbol,
                    diagnostics::DiagnosticSeverity::Error,
                    item->range(),
                    "trait '" + traitName + "' declares associated item '" +
                        item->text() + "' more than once");
                ok = false;
                continue;
            }
            const std::string key = traitName + "." + item->text();
            if (item->kind() == ast::AstNodeKind::AssociatedTypeDeclaration)
                traitTypes.emplace(key, item);
            else
                traitConstants.emplace(key, item);
        }
    }
    return ok;
}

void ProjectGenericModel::collectDirectAssociatedItemKeys(
    const LocatedNode& owner,
    const std::string& ownerName,
    std::unordered_set<std::string>& directTypeKeys,
    std::unordered_set<std::string>& directConstantKeys)
{
    for (const ast::AstNode* item : owner.syntax->children())
    {
        if (item == nullptr) continue;
        const std::string key = ownerName + "." + item->text();
        if (item->kind() == ast::AstNodeKind::AssociatedTypeDeclaration &&
            item->typeSyntax() != nullptr)
            directTypeKeys.insert(key);
        else if (item->kind() == ast::AstNodeKind::AssociatedConstantDeclaration &&
                 item->first() != nullptr)
            directConstantKeys.insert(key);
    }
}

std::string ProjectGenericModel::associatedTraitName(
    const LocatedNode& owner,
    const ast::TypeSyntax* base,
    const std::unordered_map<std::string, const LocatedNode*>& traitsByName) const
{
    std::string traitName = base->baseName();
    if (!traitsByName.contains(traitName) &&
        traitName.find('.') == std::string::npos &&
        traitName.find("::") == std::string::npos &&
        !owner.namespaceName.empty())
    {
        const std::string qualified = namespaceAppend(owner.namespaceName, traitName);
        if (traitsByName.contains(qualified)) traitName = qualified;
    }
    return traitName;
}

std::unordered_map<std::string, const ast::TypeSyntax*>
ProjectGenericModel::associatedTraitSubstitutions(
    const LocatedNode& trait,
    const ast::TypeSyntax* base)
{
    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    const auto& traitParameters = trait.syntax->genericParameters();
    const std::size_t substitutedCount = std::min(
        traitParameters.size(), base->typeArguments().size());
    for (std::size_t index = 0; index < substitutedCount; ++index)
        substitutions.emplace(traitParameters[index], base->typeArguments()[index]);
    return substitutions;
}

bool ProjectGenericModel::inheritAssociatedTypes(
    const LocatedNode& owner,
    const std::string& ownerName,
    const std::string& traitName,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    const std::unordered_map<std::string, const ast::AstNode*>& traitTypes,
    const std::unordered_set<std::string>& directTypeKeys,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    const std::string traitPrefix = traitName + ".";
    for (const auto& [key, item] : traitTypes)
    {
        if (!key.starts_with(traitPrefix) || item == nullptr ||
            item->typeSyntax() == nullptr)
            continue;
        const std::string ownerKey = ownerName + key.substr(traitName.size());
        const ast::TypeSyntax* defaultType = item->typeSyntax();
        if (!substitutions.empty() && owner.unit != nullptr)
            defaultType = cloneType(owner.unit->arena(), item->typeSyntax(), substitutions);
        const auto [position, inserted] = associatedTypes_.emplace(ownerKey, defaultType);
        if (!inserted && position->second != defaultType &&
            !directTypeKeys.contains(ownerKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                owner.syntax->range(),
                "associated type '" + ownerKey +
                    "' has competing inherited defaults; define it explicitly");
            ok = false;
        }
    }
    return ok;
}

bool ProjectGenericModel::inheritAssociatedConstants(
    const LocatedNode& owner,
    const std::string& ownerName,
    const std::string& traitName,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    const std::unordered_map<std::string, const ast::AstNode*>& traitConstants,
    const std::unordered_set<std::string>& directConstantKeys,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    const std::string traitPrefix = traitName + ".";
    for (const auto& [key, item] : traitConstants)
    {
        if (!key.starts_with(traitPrefix) || item == nullptr || item->first() == nullptr)
            continue;
        const std::string ownerKey = ownerName + key.substr(traitName.size());
        const ast::AstNode* defaultConstant = item;
        if (!substitutions.empty() && owner.unit != nullptr)
            defaultConstant = cloneNode(owner.unit->arena(), item, substitutions, "", "");
        const auto [position, inserted] =
            associatedConstants_.emplace(ownerKey, defaultConstant);
        if (!inserted && position->second != defaultConstant &&
            !directConstantKeys.contains(ownerKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                owner.syntax->range(),
                "associated constant '" + ownerKey +
                    "' has competing inherited defaults; define it explicitly");
            ok = false;
        }
    }
    return ok;
}

bool ProjectGenericModel::validateAssociatedConstantRequirement(
    const LocatedNode& owner,
    const std::string& ownerName,
    const std::string& traitName,
    const ast::AstNode* requirement,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::string ownerKey = ownerName + "." + requirement->text();
    const auto definition = associatedConstants_.find(ownerKey);
    if (definition == associatedConstants_.end())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            owner.syntax->range(),
            "type '" + ownerName + "' does not define required associated constant '" +
                requirement->text() + "' from trait '" + traitName + "'");
        return false;
    }

    bool ok = true;
    const ast::AstNode* value = definition->second;
    const ast::TypeSyntax* requiredType = requirement->typeSyntax();
    if (requiredType != nullptr && !substitutions.empty() && owner.unit != nullptr)
        requiredType = cloneType(owner.unit->arena(), requiredType, substitutions);
    if (value != nullptr && requiredType != nullptr && value->typeSyntax() != nullptr &&
        typeSpelling(value->typeSyntax()) != typeSpelling(requiredType))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            value->range(),
            "associated constant '" + ownerKey + "' has type '" +
                typeSpelling(value->typeSyntax()) + "', but trait '" + traitName +
                "' requires '" + typeSpelling(requiredType) + "'");
        ok = false;
    }
    if (value != nullptr && value->first() != nullptr &&
        !ProjectIntegerEvaluator(nullptr).evaluate(value->first()).has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            value->first()->range(),
            "associated constant '" + ownerKey +
                "' must have a compile-time constant value");
        ok = false;
    }
    return ok;
}

bool ProjectGenericModel::validateAssociatedRequirements(
    const LocatedNode& owner,
    const std::string& ownerName,
    const std::string& traitName,
    const LocatedNode& trait,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    for (const ast::AstNode* requirement : trait.syntax->children())
    {
        if (requirement == nullptr) continue;
        const std::string ownerKey = ownerName + "." + requirement->text();
        if (requirement->kind() == ast::AstNodeKind::AssociatedTypeDeclaration)
        {
            if (associatedTypes_.contains(ownerKey)) continue;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                owner.syntax->range(),
                "type '" + ownerName + "' does not define required associated type '" +
                    requirement->text() + "' from trait '" + traitName + "'");
            ok = false;
        }
        else if (requirement->kind() == ast::AstNodeKind::AssociatedConstantDeclaration &&
                 !validateAssociatedConstantRequirement(
                     owner, ownerName, traitName, requirement, substitutions, diagnostics))
            ok = false;
    }
    return ok;
}

bool ProjectGenericModel::collectOwnerAssociatedItems(
    const LocatedNode& owner,
    const std::unordered_map<std::string, const ast::AstNode*>& traitTypes,
    const std::unordered_map<std::string, const ast::AstNode*>& traitConstants,
    const std::unordered_map<std::string, const LocatedNode*>& traitsByName,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::string ownerName = namespaceAppend(owner.namespaceName, owner.ownerName);
    std::unordered_set<std::string> directTypeKeys;
    std::unordered_set<std::string> directConstantKeys;
    collectDirectAssociatedItemKeys(
        owner, ownerName, directTypeKeys, directConstantKeys);
    bool ok = registerAssociatedItems(owner.syntax, ownerName, &diagnostics);
    for (const ast::TypeSyntax* base : owner.syntax->typeSyntaxes())
    {
        if (base == nullptr) continue;
        const std::string traitName = associatedTraitName(owner, base, traitsByName);
        const auto traitRecord = traitsByName.find(traitName);
        if (traitRecord == traitsByName.end()) continue;
        const auto substitutions = associatedTraitSubstitutions(*traitRecord->second, base);
        if (!inheritAssociatedTypes(
                owner, ownerName, traitName, substitutions, traitTypes,
                directTypeKeys, diagnostics))
            ok = false;
        if (!inheritAssociatedConstants(
                owner, ownerName, traitName, substitutions, traitConstants,
                directConstantKeys, diagnostics))
            ok = false;
        if (!validateAssociatedRequirements(
                owner, ownerName, traitName, *traitRecord->second,
                substitutions, diagnostics))
            ok = false;
    }
    return ok;
}

bool ProjectGenericModel::visitAssociatedProjection(
    const std::string& key,
    std::unordered_map<std::string, unsigned>& projectionState,
    diagnostics::DiagnosticEngine& diagnostics)
{
    unsigned& state = projectionState[key];
    if (state == 2U) return true;
    if (state == 1U)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            associatedTypes_.at(key)->range(),
            "associated type normalization cycle reaches '" + key + "'");
        return false;
    }
    state = 1U;
    const ast::TypeSyntax* definition = associatedTypes_.at(key);
    if (definition != nullptr && associatedTypes_.contains(definition->baseName()) &&
        !visitAssociatedProjection(definition->baseName(), projectionState, diagnostics))
        return false;
    state = 2U;
    return true;
}

bool ProjectGenericModel::validateAssociatedProjections(
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    std::unordered_map<std::string, unsigned> projectionState;
    for (const auto& [key, definition] : associatedTypes_)
    {
        if (definition != nullptr &&
            !visitAssociatedProjection(key, projectionState, diagnostics))
            ok = false;
    }
    return ok;
}

bool ProjectGenericModel::collectAssociatedItems(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<LocatedNode> traits;
    std::vector<LocatedNode> owners;
    collectAssociatedItemUnits(units, traits, owners);

    std::unordered_map<std::string, const ast::AstNode*> traitTypes;
    std::unordered_map<std::string, const ast::AstNode*> traitConstants;
    std::unordered_map<std::string, const LocatedNode*> traitsByName;
    bool ok = indexTraitAssociatedItems(
        traits, traitTypes, traitConstants, traitsByName, diagnostics);
    for (const LocatedNode& owner : owners)
        if (!collectOwnerAssociatedItems(
                owner, traitTypes, traitConstants, traitsByName, diagnostics))
            ok = false;
    if (!validateAssociatedProjections(diagnostics)) ok = false;
    return ok;
}
}
