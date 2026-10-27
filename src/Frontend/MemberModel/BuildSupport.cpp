#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "../Internal/EnumInitializerResolver.h"
#include "../Internal/AnonymousMemberPromotionVisitState.h"
#include "../Internal/EnumValue.h"
#include "../Internal/EnumSuccessor.h"
#include "../Internal/IndexShadowCheckSwitch.h"
#include "../Internal/IndexShadowMismatch.h"

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
void ProjectMemberModel::initializeBuildState(
    const ProjectNameLookup& nameLookup,
    const ProjectGenericModel* genericModel)
{
    nameLookup_ = &nameLookup;
    genericModel_ = genericModel;
    membersByType_.resize(nameLookup.namedTypes().size());
    membersDeclaredByType_.clear();
    membersDeclaredByTypeIndexBuilt_ = false;
    constructorsByType_.resize(nameLookup.namedTypes().size());
    promotedMembersByType_.resize(nameLookup.namedTypes().size());
    membersByName_.resize(nameLookup.namedTypes().size());
    transitiveOwnershipCache_.assign(nameLookup.namedTypes().size(), 0);
    typeOrdinalByKey_.reserve(nameLookup.namedTypes().size());
    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
        typeOrdinalByKey_.emplace(
            nameLookup.namedTypes()[typeOrdinal].identity.key(), typeOrdinal);
}

void ProjectMemberModel::collectDeclaredTypeMembers(
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics)
{
    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
    {
        const NamedTypeDeclaration& type = nameLookup.namedTypes()[typeOrdinal];
        if (type.kind == NamedTypeDeclarationKind::Delegate)
        {
            validateDelegateType(type, nameLookup, diagnostics);
            continue;
        }
        if (type.kind == NamedTypeDeclarationKind::Enum)
        {
            collectEnumMembers(typeOrdinal, type, diagnostics);
            continue;
        }
        if (type.kind != NamedTypeDeclarationKind::Class &&
            type.kind != NamedTypeDeclarationKind::Interface &&
            type.kind != NamedTypeDeclarationKind::Struct)
            continue;
        collectTypeMembers(typeOrdinal, type, nameLookup, diagnostics);
    }
}

void ProjectMemberModel::collectExternalTraitImplMembers(
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NS304-T08-F05: register the trait-impl methods over external targets (the module owns the
    // trait, not the type). Each part targets an external type by name; its methods join that
    // type's member surface.
    for (const NamedTypeDeclarationPart& part : nameLookup.externalTraitImplParts())
    {
        if (part.syntax == nullptr) continue;
        const std::string targetName = part.syntax->text();
        std::optional<std::size_t> targetOrdinal;
        for (std::size_t ordinal = 0; ordinal < nameLookup.namedTypes().size(); ++ordinal)
        {
            if (nameLookup.namedTypes()[ordinal].identity.name() == targetName)
            {
                targetOrdinal = ordinal;
                break;
            }
        }
        if (!targetOrdinal.has_value()) continue;
        NamedTypeDeclaration extended = nameLookup.namedTypes()[*targetOrdinal];
        extended.parts.push_back(part);
        collectTypeMembers(*targetOrdinal, extended, nameLookup, diagnostics);
    }
}

void ProjectMemberModel::validateCopyType(
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (type.kind != NamedTypeDeclarationKind::Struct || type.syntax == nullptr ||
        type.syntax->auxiliaryText().find("copy") == std::string::npos)
        return;
    for (const std::size_t memberOrdinal : membersByType_[typeOrdinal])
    {
        if (memberOrdinal >= members_.size()) continue;
        const ClassMemberSymbol& field = members_[memberOrdinal];
        if (field.kind == ClassMemberKind::Destructor)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                field.range,
                "copy type '" + type.identity.key() +
                    "' cannot declare a destructor; destructible values are move-only");
            continue;
        }
        if (field.kind != ClassMemberKind::Field || field.isStatic || field.isConst) continue;
        // EN: Freeze 3.0 P3: a generic `copy` struct is Copy only when its type arguments allow it.
        //     `KeyValuePair<Guard, int>` keeps a move-only field, so that closed type is simply
        //     move-only (typeIsImplicitlyCopyable already derives this from the fields) instead of an
        //     error; `KeyValuePair<int, int>` stays Copy. An `owned` field is never Copy and is still
        //     rejected. Specializations are recognized by the `__g` arity marker in their name, as
        //     elsewhere in the compiler.
        // ES: Congelamiento 3.0 P3: un struct `copy` generico es Copy solo si sus argumentos de tipo
        //     lo permiten. `KeyValuePair<Guard, int>` guarda un campo move-only, asi que ese tipo
        //     cerrado es simplemente move-only (typeIsImplicitlyCopyable ya lo deriva de los campos)
        //     en vez de un error; `KeyValuePair<int, int>` sigue siendo Copy. Un campo `owned` nunca
        //     es Copy y se sigue rechazando. Las especializaciones se reconocen por la marca de aridad
        //     `__g` en su nombre, como en el resto del compilador.
        const bool specialization = type.identity.name().find("__g") != std::string::npos;
        if (specialization && field.declaredOwnership != sema::OwnershipKind::Owned) continue;
        if (field.declaredOwnership == sema::OwnershipKind::Owned ||
            !typeIsImplicitlyCopyable(field.declaredTypeKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                field.range,
                "copy type '" + type.identity.key() + "' contains non-Copy field '" +
                    field.name + "' of type '" + field.declaredTypeKey + "'");
        }
    }
}

void ProjectMemberModel::validateCopyTypes(
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // T06-F01 validates the opt-in only after every field has a canonical type key. Merely
    // spelling `copy` must never turn an owning or recursively move-only representation into a
    // bitwise-copyable value; the declaration is rejected at its source instead.
    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
        validateCopyType(typeOrdinal, nameLookup.namedTypes()[typeOrdinal], diagnostics);
}

void ProjectMemberModel::validateRawUnion(
    std::size_t typeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (const std::size_t memberOrdinal : membersByType_[typeOrdinal])
    {
        if (memberOrdinal >= members_.size()) continue;
        const ClassMemberSymbol& member = members_[memberOrdinal];
        if (member.kind != ClassMemberKind::Field || member.isStatic) continue;
        if (member.declaredOwnership == sema::OwnershipKind::Owned)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                member.range,
                "owned field '" + member.name +
                    "' is not permitted in a raw union because the union has no discriminant "
                    "that identifies the active member");
        }
        else if (typeTransitivelyOwnsStorage(member.declaredTypeKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                member.range,
                "field '" + member.name + "' of type '" + member.declaredTypeKey +
                    "' is not permitted in a raw union because it transitively contains owned "
                    "storage and the union has no discriminant that identifies the active member");
        }
    }
}

void ProjectMemberModel::validateRawUnions(
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // Raw unions carry no active-member discriminant.  It is therefore unsafe for any
    // alternative to own storage, including ownership hidden inside an inline struct or fixed
    // array of structs: writing through that nested owner would run release-before-store against
    // whichever overlapping alternative supplied the current bits.  Validate the completed type
    // graph here, after every member has been collected, so declaration order, anonymous
    // aggregates and closed generic specializations cannot bypass the rule.
    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
    {
        const NamedTypeDeclaration& type = nameLookup.namedTypes()[typeOrdinal];
        const bool isRawUnion = type.kind == NamedTypeDeclarationKind::Struct &&
            type.syntax != nullptr &&
            type.syntax->auxiliaryText().find("union") != std::string::npos &&
            type.syntax->auxiliaryText().find("variant-payload") == std::string::npos;
        if (isRawUnion) validateRawUnion(typeOrdinal, diagnostics);
    }
}

std::string ProjectMemberModel::importedCSignature(const ClassMemberSymbol& member)
{
    std::string key = member.declaredTypeKey + "(";
    for (const MethodParameterSymbol& parameter : member.parameters)
    {
        key += parameter.typeKey;
        key += ',';
    }
    if (member.isVariadic) key += "...";
    key += ')';
    return key;
}

void ProjectMemberModel::markRuntimeBoundImports(
    const std::unordered_map<std::string, std::vector<std::string>>& librariesBySymbol,
    diagnostics::DiagnosticEngine& diagnostics)
{
    for (ClassMemberSymbol& member : members_)
    {
        if (!member.isImportedC) continue;
        const auto found = librariesBySymbol.find(member.name);
        if (found == librariesBySymbol.end() || found->second.size() < 2) continue;
        if (member.importDllName.empty())
        {
            // The archive side of an ambiguous name: nothing downstream can rename it, so
            // this is the one combination the triple cannot rescue.
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidExternCSignature,
                diagnostics::DiagnosticSeverity::Error,
                member.range,
                "imported C symbol '" + member.name +
                    "' is declared both without a library (resolved from a static archive by "
                    "name) and from a DLL; a statically resolved symbol cannot share its name");
            continue;
        }
        member.importRequiresRuntimeBinding = true;
    }
}

void ProjectMemberModel::validateImportedCIdentities(
    diagnostics::DiagnosticEngine& diagnostics)
{
    // C1.2 "import identity". The identity of an imported C function is the TRIPLE
    //
    //     (library, external symbol, signature)
    //
    // never the symbol name alone. Before this pass the compiler keyed imports by symbol name
    // only, and the omission was not academic: declaring `a.dll!Init` and `b.dll!Init` in one
    // project produced TWO LLVM declarations asking for the same name, LLVM silently renamed the
    // second to `Init.1`, and the link failed with "undefined symbol: Init.1" (measured). The
    // triple is enforced here in two halves:
    //
    //   * SAME (library, symbol), DIFFERENT signature -> a hard conflict. Two names for one
    //     foreign entry point cannot both be right, and picking either one silently would
    //     miscompile every call site of the other.
    //   * SAME symbol, DIFFERENT libraries -> legal, and the member is flagged
    //     `importRequiresRuntimeBinding`. PE/COFF import libraries key an import by its symbol
    //     NAME, so two libraries claiming one name cannot both be resolved statically -- that
    //     import binds at first call through LoadLibrary/GetProcAddress instead (see
    //     ProjectOopLlvmLowerer::defineRuntimeBoundImports). The static-by-name path is untouched
    //     for every import whose name only one library claims, which is all of them in practice.
    //
    // A `extern C { ... }` declaration with no `from` participates with an EMPTY library: its
    // symbol is resolved by the linker out of an archive, where nothing can be renamed, so a name
    // it claims can never also be claimed dynamically.
    std::unordered_map<
        std::string,
        std::pair<const ClassMemberSymbol*, std::string>> importsByLibraryAndSymbol;
    std::unordered_map<std::string, std::vector<std::string>> librariesBySymbol;
    collectImportedCLibraries(
        importsByLibraryAndSymbol, librariesBySymbol, diagnostics);
    markRuntimeBoundImports(librariesBySymbol, diagnostics);
}

void ProjectMemberModel::scanMemberBodies(
    diagnostics::DiagnosticEngine& diagnostics)
{
    for (const ClassMemberSymbol& member : members_)
    {
        if ((member.kind != ClassMemberKind::Method &&
             member.kind != ClassMemberKind::Constructor &&
             member.kind != ClassMemberKind::Destructor) ||
            member.syntax == nullptr || member.syntax->first() == nullptr)
            continue;
        const auto iterator = memberBySyntax_.find(member.syntax);
        if (iterator == memberBySyntax_.end()) continue;
        scanMethodBody(
            member.syntax->first(),
            member.declaringTypeOrdinal,
            iterator->second,
            diagnostics);
    }
}

void ProjectMemberModel::scanCompilationUnits(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        if (unit != nullptr && unit->syntaxTree() != nullptr)
            scanInvalidThisContexts(unit->syntaxTree(), diagnostics);
    }
}


bool ProjectMemberModel::bindPrecompiledPhysicalNames(
    std::span<const PrecompiledMemberIdentity> identities,
    std::span<const std::pair<std::string, std::string>> typeKeyAliases,
    std::string& error)
{
    error.clear();
    if (!built_ || nameLookup_ == nullptr)
    {
        error = "precompiled physical names require a completed member model";
        return false;
    }

    // Replace the consumer-local generated name of a closed specialization with the producer's
    // published name only while comparing semantic signatures.  The replacement is deliberately
    // longest-first so a nested closed type cannot be partially consumed by an outer alias.
    std::vector<std::pair<std::string, std::string>> aliases(
        typeKeyAliases.begin(), typeKeyAliases.end());
    std::ranges::sort(aliases, [](const auto& left, const auto& right) {
        return left.first.size() > right.first.size();
    });
    // Only effective aliases (non-empty and different from the published key) change a key.
    // They are filtered once, after the sort, so the effective ones keep their sorted order;
    // without any effective alias canonicalize returns its input unchanged.
    std::erase_if(aliases, [](const auto& alias) {
        return alias.first.empty() || alias.first == alias.second;
    });
    const auto canonicalize = [&](std::string value) {
        for (const auto& [localTypeKey, publishedTypeKey] : aliases)
        {
            std::size_t cursor = 0;
            while ((cursor = value.find(localTypeKey, cursor)) != std::string::npos)
            {
                value.replace(cursor, localTypeKey.size(), publishedTypeKey);
                cursor += publishedTypeKey.size();
            }
        }
        return value;
    };

    const PrecompiledMemberIdentityIndex identityIndex(identities);
    // canonicalize is a pure function of its input, so the canonical key of each declaring type
    // is computed once and reused by its other members.
    std::unordered_map<std::size_t, std::string> declaringTypeKeys;

    for (ClassMemberSymbol& member : members_)
    {
        if (!member.isPrecompiled || member.isImportedC || member.isAbstract ||
            (member.kind != ClassMemberKind::Method && member.kind != ClassMemberKind::Constructor))
        {
            continue;
        }
        if (member.declaringTypeOrdinal >= nameLookup_->namedTypes().size())
        {
            error = "precompiled member '" + member.signatureKey + "' has an invalid declaring type";
            return false;
        }

        auto declaringTypeEntry = declaringTypeKeys.find(member.declaringTypeOrdinal);
        if (declaringTypeEntry == declaringTypeKeys.end())
        {
            declaringTypeEntry = declaringTypeKeys.emplace(
                member.declaringTypeOrdinal,
                canonicalize("type:" + nameLookup_->namedTypes()[member.declaringTypeOrdinal].identity.key()))
                .first;
        }
        const std::string& declaringTypeKey = declaringTypeEntry->second;
        if constexpr (internal::kIndexShadowCheckEnabled)
        {
            if (declaringTypeKey != canonicalize(
                    "type:" + nameLookup_->namedTypes()[member.declaringTypeOrdinal].identity.key()))
                internal::reportIndexShadowMismatch("bindPrecompiledPhysicalNames", declaringTypeKey);
        }
        const std::string signatureKey = canonicalize(member.signatureKey);
        const PrecompiledMemberIdentityMatch match = identityIndex.find(
            member.kind, declaringTypeKey, member.name, signatureKey);
        if (match.kind == PrecompiledMemberIdentityMatchKind::Missing)
        {
            error = "precompiled callable '" + declaringTypeKey + "." + signatureKey +
                "' has no physical identity in NSMETA";
            return false;
        }
        if (match.kind == PrecompiledMemberIdentityMatchKind::Ambiguous)
        {
            error = "precompiled callable '" + declaringTypeKey + "." + signatureKey +
                "' has more than one published physical identity";
            return false;
        }
        member.precompiledMangledName = match.identity->mangledName;
        member.parameterEscapeSummary = match.identity->parameterEscapeSummary;
        if (!member.parameterEscapeSummary.empty() &&
            member.parameterEscapeSummary.size() != member.parameters.size())
        {
            error = "precompiled callable '" + declaringTypeKey + "." + signatureKey +
                "' has an invalid escape-summary parameter count";
            return false;
        }
    }
    return true;
}
}
