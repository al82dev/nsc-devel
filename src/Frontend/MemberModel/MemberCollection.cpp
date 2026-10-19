#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "../Internal/EnumInitializerResolver.h"
#include "../Internal/AnonymousMemberPromotionVisitState.h"
#include "../Internal/EnumValue.h"
#include "../Internal/EnumSuccessor.h"

#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{

bool ProjectMemberModel::isPartialEventMerge(
    const ClassMemberSymbol& symbol,
    const ClassMemberSymbol& previous) noexcept
{
    // NS304-T12-F05: a partial event (and its synthesized backing storage) merges;
    // the add/remove accessors merge in the ordinary method path (F02).
    return symbol.syntax != nullptr && previous.syntax != nullptr &&
        ((symbol.syntax->auxiliaryText().find("partial-event") != std::string::npos &&
          previous.syntax->auxiliaryText().find("partial-event") != std::string::npos) ||
         (symbol.syntax->auxiliaryText().find("event-backing:") != std::string::npos &&
          previous.syntax->auxiliaryText().find("event-backing:") != std::string::npos));
}

bool ProjectMemberModel::isPartialMethodMerge(
    const ClassMemberSymbol& symbol,
    const ClassMemberSymbol& previous) noexcept
{
    return (symbol.kind == ClassMemberKind::Method ||
            symbol.kind == ClassMemberKind::Constructor) &&
        symbol.syntax != nullptr && previous.syntax != nullptr &&
        symbol.syntax->auxiliaryText().find("partial-method") != std::string::npos &&
        previous.syntax->auxiliaryText().find("partial-method") != std::string::npos;
}

void ProjectMemberModel::mergePartialMethod(
    const ClassMemberSymbol& symbol,
    const ClassMemberSymbol& previous,
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // NS304-T12-F02: a partial method signature and its defining body merge.
    // Attach the body to the signature (which must carry no body) and drop the
    // defining declaration.
    if (previous.syntax->first() == nullptr && symbol.syntax->first() != nullptr)
        const_cast<ast::AstNode*>(previous.syntax)->setFirst(symbol.syntax->first());
    else if (previous.syntax->first() != nullptr && symbol.syntax->first() != nullptr)
        reportDuplicate(
            symbol,
            previous,
            diagnostics,
            "partial method '" + symbol.name +
                "' has more than one defining body in type '" +
                type.identity.fullyQualifiedName() + "'");
}

bool ProjectMemberModel::controlledSpecializationsMayCoexist(
    const ClassMemberSymbol& symbol,
    const ClassMemberSymbol& previous) const
{
    if (symbol.kind != ClassMemberKind::Method || genericModel_ == nullptr ||
        !symbol.genericTemplateOrdinal.has_value() ||
        !previous.genericTemplateOrdinal.has_value() ||
        *symbol.genericTemplateOrdinal >= genericModel_->templates().size() ||
        *previous.genericTemplateOrdinal >= genericModel_->templates().size())
        return false;
    return genericModel_->templates()[*symbol.genericTemplateOrdinal]
            .specializationFamilyKey ==
        genericModel_->templates()[*previous.genericTemplateOrdinal]
            .specializationFamilyKey;
}

bool ProjectMemberModel::memberConflictsWithPrevious(
    const ClassMemberSymbol& symbol,
    const ClassMemberSymbol& previous,
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const bool partialEventMerge = isPartialEventMerge(symbol, previous);
    if ((symbol.kind == ClassMemberKind::Field ||
         symbol.kind == ClassMemberKind::Event ||
         previous.kind == ClassMemberKind::Field ||
         previous.kind == ClassMemberKind::Event) &&
        !partialEventMerge)
    {
        reportDuplicate(
            symbol,
            previous,
            diagnostics,
            "member name '" + symbol.name +
                "' conflicts with an existing member in class '" +
                type.identity.fullyQualifiedName() + "'");
        return true;
    }
    if (partialEventMerge) return true;
    if (symbol.kind != previous.kind || symbol.signatureKey != previous.signatureKey ||
        (symbol.kind == ClassMemberKind::Constructor &&
         symbol.isStatic != previous.isStatic))
        return false;
    if (isPartialMethodMerge(symbol, previous))
    {
        mergePartialMethod(symbol, previous, type, diagnostics);
        return true;
    }
    // F06 validates the complete declaration family after semantic constraint
    // normalization. Let its closed candidates coexist here so the ordinary
    // overload resolver can later apply that proven partial order.
    if (controlledSpecializationsMayCoexist(symbol, previous)) return false;
    const std::string category = symbol.kind == ClassMemberKind::Constructor
        ? "constructor"
        : symbol.kind == ClassMemberKind::Destructor ? "destructor" : "method";
    reportDuplicate(
        symbol,
        previous,
        diagnostics,
        "duplicate " + category + " signature '" + symbol.signatureKey +
            "' in type '" + type.identity.fullyQualifiedName() +
            (symbol.kind == ClassMemberKind::Method && !symbol.isUserConversion
                ? "'; return type is not part of overload identity" : "'"));
    return true;
}

bool ProjectMemberModel::isDuplicateMember(
    const ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    const std::unordered_map<std::string, std::vector<std::size_t>>& byName,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const auto sameName = byName.find(symbol.name);
    if (sameName == byName.end()) return false;
    for (const std::size_t previousOrdinal : sameName->second)
    {
        if (previousOrdinal >= members_.size()) continue;
        if (memberConflictsWithPrevious(
                symbol, members_[previousOrdinal], type, diagnostics))
            return true;
    }
    return false;
}

void ProjectMemberModel::appendCollectedMember(
    ClassMemberSymbol symbol,
    std::size_t typeOrdinal,
    const ast::AstNode* memberNode,
    std::unordered_map<std::string, std::vector<std::size_t>>& byName)
{
    const std::size_t memberOrdinal = members_.size();
    members_.push_back(std::move(symbol));
    membersByType_[typeOrdinal].push_back(memberOrdinal);
    if (members_[memberOrdinal].kind == ClassMemberKind::Constructor &&
        !members_[memberOrdinal].isStatic)
        constructorsByType_[typeOrdinal].push_back(memberOrdinal);
    membersByName_[typeOrdinal][members_[memberOrdinal].name].push_back(memberOrdinal);
    memberBySyntax_.emplace(memberNode, memberOrdinal);
    byName[members_[memberOrdinal].name].push_back(memberOrdinal);
}

void ProjectMemberModel::collectTypeMember(
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    std::size_t partOrdinal,
    const NamedTypeDeclarationPart& part,
    const ast::AstNode* memberNode,
    std::size_t declarationOrdinal,
    const ProjectNameLookup& nameLookup,
    std::unordered_map<std::string, std::vector<std::size_t>>& byName,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ClassMemberSymbol symbol;
    initializeMemberIdentity(
        symbol, typeOrdinal, partOrdinal, declarationOrdinal, part, memberNode);
    applyMemberGenericSpecialization(symbol, memberNode);
    applyMemberOrigin(symbol, part, memberNode);
    const bool isAsyncMethod = initializeMemberDeclaredType(
        symbol, type, memberNode, nameLookup, diagnostics);
    initializeMemberPointerAndOwnership(symbol, memberNode);
    initializeMemberLifetimeContract(symbol, memberNode);
    validateMemberReferenceContract(symbol, diagnostics);
    validateMemberFieldLifetime(symbol, type, diagnostics);
    validateMemberViewField(symbol, type, nameLookup, diagnostics);
    initializeMemberEffects(symbol, memberNode, isAsyncMethod);
    initializeMemberStorageModifiers(symbol, type, memberNode);
    validateVolatileMember(symbol, nameLookup, diagnostics);
    validateReadonlyStructMember(symbol, type, diagnostics);
    validateStaticOwnedMember(symbol, diagnostics);
    initializeMemberDispatchAndInterop(symbol, type, memberNode, diagnostics);
    applyExplicitMemberLayout(symbol, type, memberNode, diagnostics);
    initializeGeneratedMemberMetadata(symbol, memberNode);
    initializeAccessorMetadata(symbol);
    validateEventMember(symbol, nameLookup, diagnostics);
    if (symbol.kind == ClassMemberKind::Method ||
        symbol.kind == ClassMemberKind::Constructor)
        finalizeCallableMember(symbol, type, memberNode, nameLookup, diagnostics);
    else if (symbol.kind == ClassMemberKind::Destructor)
        symbol.signatureKey = symbol.name + "()";
    else symbol.signatureKey = symbol.name;
    validateInlineArraySignature(symbol, diagnostics);
    validateCAbiMember(symbol, type, memberNode, nameLookup, diagnostics);
    validateUserOperator(symbol, type, diagnostics);
    validateUserConversion(symbol, type, diagnostics);
    if (isDuplicateMember(symbol, type, byName, diagnostics)) return;
    appendCollectedMember(std::move(symbol), typeOrdinal, memberNode, byName);
}

bool ProjectMemberModel::collectTypeMembers(
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::unordered_map<std::string, std::vector<std::size_t>> byName;
    std::unordered_map<std::string, const ast::AstNode*> propertyDeclarations;
    std::unordered_map<std::string, const ast::AstNode*> ordinaryDeclarations;
    validateTypeLifetimeConstraints(type, diagnostics);
    for (std::size_t partOrdinal = 0; partOrdinal < type.parts.size(); ++partOrdinal)
        collectTypePartMembers(
            typeOrdinal,
            type,
            partOrdinal,
            type.parts[partOrdinal],
            nameLookup,
            byName,
            propertyDeclarations,
            ordinaryDeclarations,
            diagnostics);
    return !diagnostics.hasErrors();
}
}
