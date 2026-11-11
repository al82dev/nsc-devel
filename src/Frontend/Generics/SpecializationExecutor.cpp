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
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Lexer/Keyword.h"
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
// NSC305-P01. Times one specialization materialization and, on completion, records the
// `generics.specialization-waves` sub-phase and attributes its exclusive wall time to the
// template that produced it. RAII keeps every early-return path (budget, pack and associated
// rollback) accounted for without changing control flow.
using internal::SpecializationWaveTimer;
#if 0 // INSTRUMENTATION CODE
using internal::genericNodeDetail;
using internal::genericTypeDetail;
using internal::genericUnitDetail;
using internal::GenericWorkProgress;
#endif // INSTRUMENTATION CODE
// NSC305-G09. Merges a produced specialization clone into its reserved slot (see the header).
void ProjectGenericModel::commitTypeSpecialization(
    TemplateState& state,
    ast::AstNode* clone,
    const std::string& key,
    const std::string& specializedName,
    std::size_t ordinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    clone->setText(specializedName);
    clone->clearGenericParameters();
    // Keep the specialization prefix first because downstream demand/debug readers use it as the
    // discriminator. Source aggregate metadata follows a reserved delimiter: a comma cannot be used
    // because it is valid inside a multi-argument specialization key. Record derives, layout, union
    // and borrowed-view policy belong to the template and must survive monomorphization.
    std::string specializationMetadata = "stage44-specialization:" + key;
    if (!state.syntax->auxiliaryText().empty())
        specializationMetadata += ";nsc-meta:" + state.syntax->auxiliaryText();
    clone->setAuxiliaryText(std::move(specializationMetadata));
    state.parent->addChild(clone);
    specializations_[ordinal].specializedSyntax = clone;
    specializationBySyntax_[clone] = ordinal;
    specializations_[ordinal].state = SpecializationState::Ready;
    registerAssociatedItems(
        clone,
        namespaceAppend(state.record.namespaceName, specializedName),
        &diagnostics);
}

// NSC305-G09. Reserves a slot + ordinal for a type specialization (see the header).
std::size_t ProjectGenericModel::reserveTypeSpecialization(
    const TemplateState& state,
    const std::string& key,
    const std::string& specializedName,
    const std::vector<std::string>& identities)
{
    const std::size_t ordinal = specializations_.size();
    specializationPlanner_.commit(key, ordinal);
    specializations_.push_back(GenericSpecializationRecord{
        state.record.kind,
        state.syntax,
        nullptr,
        state.record.canonicalKey,
        key,
        specializedName,
        identities,
        ordinal,
        state.ordinal,
        nullptr,
        currentInstantiationRange_,
        currentOverloadGroup_,
        InstantiationContext{
            currentInstantiationProjectOrdinal_,
            currentInstantiationNamespace_,
            currentInstantiationRange_,
            currentOverloadGroup_,
            activePackSubstitutions_,
            activeAssociatedProjections_}});
    specializations_.back().state = SpecializationState::Building;
    return ordinal;
}

// NSC305-G09. Clones a type specialization on the worker thread (the parallel half) and captures the
// causal errors in the product; the committer later reports + links + processes them.
ProjectGenericModel::SpecializationProduct ProjectGenericModel::cloneTypeSpecializationProduct(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    const std::string& specializedName,
    const SpecializationContext& callerContext)
{
    SpecializationProduct product;
    product.arena = std::make_unique<ast::AstArena>();
    auto* const previousIdentities = workerResolvedTypeIdentities_;
    auto* const previousOrigins = workerResolvedTypeOrigins_;
    const SpecializationContext* const previousContext = workerSpecializationContext_;
    workerResolvedTypeIdentities_ = &product.resolvedTypeIdentities;
    workerResolvedTypeOrigins_ = &product.resolvedTypeOrigins;
    workerSpecializationContext_ = &callerContext;
    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    prepareSpecializationSubstitutions(state, arguments, substitutions);
    packMaterializationError_.clear();
    packMaterializationErrorRange_ = {};
    activeAssociatedProjections_.clear();
    associatedProjectionError_.clear();
    associatedProjectionErrorRange_ = {};

    product.clone = cloneNode(
        *product.arena, state.syntax, substitutions, specializedName, state.record.name);
    product.packError = packMaterializationError_;
    product.packErrorRange = packMaterializationErrorRange_;
    product.associatedError = associatedProjectionError_;
    product.associatedErrorRange = associatedProjectionErrorRange_;
    packMaterializationError_ = callerContext.packError;
    packMaterializationErrorRange_ = callerContext.packErrorRange;
    activePackSubstitutions_ = callerContext.packSubstitutions;
    activeAssociatedProjections_ = callerContext.associatedProjections;
    associatedProjectionError_ = callerContext.associatedError;
    associatedProjectionErrorRange_ = callerContext.associatedErrorRange;
    workerResolvedTypeIdentities_ = previousIdentities;
    workerResolvedTypeOrigins_ = previousOrigins;
    workerSpecializationContext_ = previousContext;
    return product;
}

// NSC305-G09. Executes a reserved type specialization (see the header).
ProjectGenericModel::SpecializationProduct ProjectGenericModel::executeTypeSpecializationProduct(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    const std::string& key,
    const std::string& specializedName,
    std::size_t ordinal,
    const SpecializationContext& callerContext,
    diagnostics::DiagnosticEngine& diagnostics)
{
    SpecializationProduct product =
        cloneTypeSpecializationProduct(state, arguments, specializedName, callerContext);
    product.ordinal = ordinal;
    commitTypeSpecializationProduct(
        state, product, key, specializedName, callerContext, diagnostics);
    return product;
}

// NSC305-G09. The serial commit half of a specialization (see the header).
bool ProjectGenericModel::commitTypeSpecializationProduct(
    TemplateState& state,
    SpecializationProduct& product,
    const std::string& key,
    const std::string& specializedName,
    const SpecializationContext& callerContext,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!product.packError.empty() || !product.associatedError.empty())
    {
        diagnostics.report(
            product.associatedError.empty()
                ? diagnostics::DiagnosticId::InvalidArgumentCount
                : diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            product.associatedError.empty() ? product.packErrorRange : product.associatedErrorRange,
            product.associatedError.empty() ? product.packError : product.associatedError);
        return false;
    }
    if (product.clone == nullptr || product.arena == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            callerContext.range,
            "generic specialization worker produced no clone");
        return false;
    }
    state.unit->arena().absorb(std::move(*product.arena));
    product.arena.reset();
    for (auto& [syntax, identity] : product.resolvedTypeIdentities)
        resolvedTypeIdentityBySyntax_[syntax] = std::move(identity);
    for (auto& [syntax, origin] : product.resolvedTypeOrigins)
        resolvedTypeOriginBySyntax_[syntax] = std::move(origin);
    {
        const GenericTelemetry::Scope commit(telemetry_, "generics.rewrite-commit");
        commitTypeSpecialization(
            state, product.clone, key, specializedName, product.ordinal, diagnostics);
    }

    // The C3 payload is the clone as committed, before processNode: a restore commits it and runs
    // processNode itself, as this path does, instead of processing an already processed tree.
    std::string specializationPayload;
    if (templateCache_.enabled()) specializationPayload = serializeAstNodeTree(product.clone);
    GenericLookupScope declarationScope;
    declarationScope.importedNamespaces.write() = state.record.declarationImports;
    declarationScope.aliases.write().insert(
        state.record.declarationAliases.begin(), state.record.declarationAliases.end());
    processNode(
        *state.unit, product.clone, state.record.namespaceName, specializedName,
        std::move(declarationScope), false, diagnostics);
    // NSC305-C03. Persist the specialized clone (the C3 cache payload) under the specialization key.
    // Nothing is stored once the compilation has reported an error.
    if (templateCache_.enabled() && !diagnostics.hasErrors())
    {
        std::string error;
        (void)templateCache_.store(key, specializationPayload, error);
    }
    // processNode may recursively materialize another generic and therefore replace all four
    // current-instantiation fields. They describe a call stack, not global "last call" state:
    // restore the caller before returning so a sibling specialization (notably the next overload
    // candidate) cannot inherit the nested library context.
    currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
    currentInstantiationNamespace_ = callerContext.namespaceName;
    currentInstantiationRange_ = callerContext.range;
    currentOverloadGroup_ = callerContext.overloadGroup;
    activePackSubstitutions_ = callerContext.packSubstitutions;
    product.success = true;
    return true;
}

bool ProjectGenericModel::executeTypeSpecialization(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    const std::string& key,
    const std::string& specializedName,
    std::size_t ordinal,
    const SpecializationContext& callerContext,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ProjectGenericModel::SpecializationProduct product = executeTypeSpecializationProduct(
        state, arguments, key, specializedName, ordinal, callerContext, diagnostics);
    return product.success;
}

// NSC305-G09. Deterministic pre-reserve for the parallel wave: compute key/name, dedup, enter the
// budget and reserve the ordinal. Returns SIZE_MAX on a hit or error (the caller handles it).
std::size_t ProjectGenericModel::reserveTypeDemand(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    std::string& key,
    std::string& specializedName,
    std::vector<std::string>& identities,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<std::string> spellings;
    key = computeSpecializationKey(state.record, arguments, spellings, identities);
    specializedName = typeSpecializedName(state.record, arguments.size(), spellings, key);
    if (specializationPlanner_.request(key))
    {
        telemetry_.noteSpecializationHit();
        ++instantiationCacheHitCount_;
        return std::numeric_limits<std::size_t>::max();
    }
    if (state.unit == nullptr || state.parent == nullptr || state.syntax == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            state.syntax != nullptr ? state.syntax->range() : SourceRange{},
            "Stage 44 generic type template has no materialization parent");
        return std::numeric_limits<std::size_t>::max();
    }
    std::string budgetError;
    if (!instantiationBudget_.enter(key, currentLogicalDepth_, budgetError))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            currentInstantiationRange_,
            budgetError);
        return std::numeric_limits<std::size_t>::max();
    }
    const std::size_t ordinal =
        reserveTypeSpecialization(state, key, specializedName, identities);
    // A wave reservation consumes the monotonic instantiation count and validates the demand's
    // logical depth, but it is not a recursive call frame. Keeping every sibling on activeStack()
    // makes nested method materialization see the wave breadth as causal depth and makes the
    // forward execute order pop unrelated siblings. The planner's Building slot prevents a
    // recursive type demand from reserving the same key again.
    instantiationBudget_.leave();
    return ordinal;
}

// NSC305-G09. Rewrites the target syntax after a specialization resolves (shared by the hit and the
// execute paths).
void ProjectGenericModel::rewriteSpecializedSyntax(
    ast::TypeSyntax* syntax,
    const GenericTemplateRecord& record,
    const std::string& key,
    const std::string& specializedName)
{
    {
        std::string canonical = "generic:" + key;
        appendTypeModifiers(canonical, syntax);
        resolvedTypeIdentityBySyntax_[syntax] = std::move(canonical);
        resolvedTypeOriginBySyntax_[syntax] = DeclaredTypeOrigin{
            record.projectOrdinal, record.namespaceName};
        syntax->setOriginProjectOrdinal(record.projectOrdinal);
        syntax->setOriginNamespace(record.namespaceName);
    }
    if (!syntax->typeArguments().empty())
    {
        auto& arguments = rewrittenTypeArgumentsBySyntax_[syntax];
        if (arguments.empty())
            arguments.assign(syntax->typeArguments().begin(), syntax->typeArguments().end());
    }
    const std::string originalBaseName = syntax->baseName();
    if (const std::size_t separator = originalBaseName.find_last_of('.');
        separator != std::string::npos)
    {
        syntax->setBaseName(originalBaseName.substr(0, separator + 1) + specializedName);
    }
    else
    {
        syntax->setBaseName(specializedName);
    }
    syntax->clearTypeArguments();
}

// NSC305-G09. Executes a pre-reserved type demand (the apply half): C3 restore + execute + merge +
// rewrite, using the ordinal/key/name computed by reserveTypeDemand.
std::string ProjectGenericModel::executeTypeDemand(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* syntax,
    TemplateState& state,
    const std::string& namespaceName,
    std::size_t ordinal,
    const std::string& key,
    const std::string& specializedName,
    std::span<ast::TypeSyntax* const> arguments,
    const SpecializationContext& callerContext,
    diagnostics::DiagnosticEngine& diagnostics,
    SpecializationProduct* preClonedProduct)
{
    currentInstantiationProjectOrdinal_ = unit.projectOrdinal();
    currentInstantiationNamespace_ = namespaceName;
    currentInstantiationRange_ = syntax->range();

    // NSC305-C03. Restore the specialized clone from the cache (skip the materialization + re-link).
    if (templateCache_.enabled())
    {
        std::string payload;
        bool hit = false;
        std::string error;
        if (templateCache_.restore(key, payload, hit, error) && hit)
        {
            CachePayloadReader reader(payload);
            ast::AstNode* clone = deserializeAstNodeTree(state.unit->arena(), reader);
            if (clone != nullptr)
            {
                commitTypeSpecialization(
                    state, clone, key, specializedName, ordinal, diagnostics);
                GenericLookupScope declarationScope;
                declarationScope.importedNamespaces.write() = state.record.declarationImports;
                declarationScope.aliases.write().insert(
                    state.record.declarationAliases.begin(), state.record.declarationAliases.end());
                processNode(
                    *state.unit, clone, state.record.namespaceName, specializedName,
                    std::move(declarationScope), false, diagnostics);
                currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
                currentInstantiationNamespace_ = callerContext.namespaceName;
                currentInstantiationRange_ = callerContext.range;
                currentOverloadGroup_ = callerContext.overloadGroup;
                activePackSubstitutions_ = callerContext.packSubstitutions;
                telemetry_.noteSpecializationHit();
                ++instantiationCacheHitCount_;
                // The demanding syntax names the specialization, as on the cold path below.
                rewriteSpecializedSyntax(syntax, state.record, key, specializedName);
                return specializedName;
            }
        }
    }

    SpecializationWaveTimer wave(telemetry_, state.record.canonicalKey);
    if (preClonedProduct != nullptr)
    {
        if (!commitTypeSpecializationProduct(
                state, *preClonedProduct, key, specializedName, callerContext, diagnostics))
        {
            specializationPlanner_.rollback(key);
            specializations_.pop_back();
            currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
            currentInstantiationNamespace_ = callerContext.namespaceName;
            currentInstantiationRange_ = callerContext.range;
            currentOverloadGroup_ = callerContext.overloadGroup;
            return {};
        }
    }
    else if (!executeTypeSpecialization(
                 state, arguments, key, specializedName, ordinal, callerContext, diagnostics))
    {
        specializationPlanner_.rollback(key);
        specializations_.pop_back();
        currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
        currentInstantiationNamespace_ = callerContext.namespaceName;
        currentInstantiationRange_ = callerContext.range;
        currentOverloadGroup_ = callerContext.overloadGroup;
        return {};
    }

    rewriteSpecializedSyntax(syntax, state.record, key, specializedName);
    return specializedName;
}

bool ProjectGenericModel::restoreTypeSpecializationFromCache(
    TemplateState& state,
    const std::string& key,
    const std::string& specializedName,
    const std::vector<std::string>& identities,
    const SpecializationContext& callerContext,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!templateCache_.enabled()) return false;
    std::string payload;
    bool hit = false;
    std::string error;
    if (!templateCache_.restore(key, payload, hit, error) || !hit) return false;

    CachePayloadReader reader(payload);
    ast::AstNode* clone = deserializeAstNodeTree(state.unit->arena(), reader);
    if (clone == nullptr) return false;
    const std::size_t ordinal =
        reserveTypeSpecialization(state, key, specializedName, identities);
    commitTypeSpecialization(
        state, clone, key, specializedName, ordinal, diagnostics);
    // The clone body still needs its child demands (types/calls) resolved; those recurse
    // through the same C3 restore, so this is a walk + cache hits, not a re-clone.
    GenericLookupScope declarationScope;
    declarationScope.importedNamespaces.write() = state.record.declarationImports;
    declarationScope.aliases.write().insert(
        state.record.declarationAliases.begin(), state.record.declarationAliases.end());
    processNode(
        *state.unit, clone, state.record.namespaceName, specializedName,
        std::move(declarationScope), false, diagnostics);
    currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
    currentInstantiationNamespace_ = callerContext.namespaceName;
    currentInstantiationRange_ = callerContext.range;
    currentOverloadGroup_ = callerContext.overloadGroup;
    activePackSubstitutions_ = callerContext.packSubstitutions;
    telemetry_.noteSpecializationHit();
    ++instantiationCacheHitCount_;
    return true;
}

std::string ProjectGenericModel::ensureTypeSpecialization(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string* outSpecializationKey)
{
    SpecializationContext callerContext;
    callerContext.save(*this);
#if 0 // INSTRUMENTATION CODE
    const GenericWorkProgress progress(
        telemetry_, "generics.type-specialization",
        state.record.canonicalKey + " arguments=" + std::to_string(arguments.size()));
#endif // INSTRUMENTATION CODE

    std::vector<std::string> spellings;
    std::vector<std::string> identities;
    const std::string key = computeSpecializationKey(
        state.record, arguments, spellings, identities);
    const std::string specializedName =
        typeSpecializedName(state.record, arguments.size(), spellings, key);
    if (const auto found = specializationPlanner_.request(key))
    {
        telemetry_.noteSpecializationHit();
        ++instantiationCacheHitCount_;
        if (outSpecializationKey != nullptr) *outSpecializationKey = key;
        return specializations_[*found].specializedName;
    }

    // NSC305-C03. Restore the specialized clone from the cache (skip the materialization + re-link).
    if (restoreTypeSpecializationFromCache(
            state, key, specializedName, identities, callerContext, diagnostics))
    {
        if (outSpecializationKey != nullptr) *outSpecializationKey = key;
        return specializedName;
    }

    if (state.unit == nullptr || state.parent == nullptr || state.syntax == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            state.syntax != nullptr ? state.syntax->range() : SourceRange{},
            "Stage 44 generic type template has no materialization parent");
        return {};
    }
    std::string budgetError;
    if (!instantiationBudget_.enter(key, currentLogicalDepth_, budgetError))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            currentInstantiationRange_,
            budgetError);
        return {};
    }
    SpecializationWaveTimer wave(telemetry_, state.record.canonicalKey);

    const std::size_t ordinal = reserveTypeSpecialization(state, key, specializedName, identities);

    if (!executeTypeSpecialization(
            state, arguments, key, specializedName, ordinal, callerContext, diagnostics))
    {
        specializationPlanner_.rollback(key);
        specializations_.pop_back();
        currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
        currentInstantiationNamespace_ = callerContext.namespaceName;
        currentInstantiationRange_ = callerContext.range;
        currentOverloadGroup_ = callerContext.overloadGroup;
        instantiationBudget_.leave();
        return {};
    }
    instantiationBudget_.leave();
    if (outSpecializationKey != nullptr) *outSpecializationKey = key;
    return specializedName;
}

// EN: Returns the physical name for a type specialization. Imported metadata wins for the exact
//     canonical key so a reconstructed public source cannot rename an already published type.
// ES: Devuelve el nombre físico de una especialización de tipo. La metadata importada prevalece
//     para la clave canónica exacta, evitando que una fuente pública reconstruida renombre un tipo
//     ya publicado.
std::string ProjectGenericModel::typeSpecializedName(
    const GenericTemplateRecord& record,
    std::size_t argumentCount,
    const std::vector<std::string>& spellings,
    const std::string& key) const
{
    const auto published = importedTypeSpecializationNamesByKey_.find(key);
    if (published != importedTypeSpecializationNamesByKey_.end())
    {
        return published->second;
    }
    std::string specializedName = record.name + "__g" + std::to_string(argumentCount);
    for (const std::string& spelling : spellings)
    {
        specializedName += "__" + sanitize(spelling);
    }
    specializedName += "__h" + stableHashSuffix(key);
    return specializedName;
}

std::string ProjectGenericModel::methodSpecializedName(
    const TemplateState& state,
    std::size_t argumentCount,
    const std::vector<std::string>& spellings,
    const std::string& key,
    std::string_view sharedName) const
{
    // ID 132, segunda mitad. `sharedName` no esta vacio cuando VARIAS plantillas comparten nombre
    // y aridad generica: todas se especializan con EL MISMO nombre, y a partir de ahi dejan de ser
    // un problema de genericos y pasan a ser SOBRECARGAS ORDINARIAS, que este compilador ya
    // resuelve por lista de parametros desde siempre.
    //
    // Es la unica forma honesta de hacerlo en esta pasada: elegir aqui exigiria los TIPOS de los
    // argumentos, y el modelo semantico --el unico que los conoce-- corre despues. En vez de
    // adivinar, se materializan todas y se deja elegir a quien sabe.
    //
    // La CLAVE sigue llevando la identidad de cada plantilla (con su `#N`), asi que la cache
    // sigue distinguiendolas; lo unico compartido es el nombre emitido.
    if (!sharedName.empty()) return std::string(sharedName);
    std::string specializedName = state.record.name + "__g" + std::to_string(argumentCount);
    for (const std::string& spelling : spellings)
        specializedName += "__" + sanitize(spelling);
    specializedName += "__h" + stableHashSuffix(key);
    return specializedName;
}

bool ProjectGenericModel::beginMethodSpecialization(
    const TemplateState& state,
    const std::string& key,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (state.unit == nullptr || state.parent == nullptr || state.syntax == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            state.syntax != nullptr ? state.syntax->range() : SourceRange{},
            "Stage 44 generic method template has no materialization parent");
        return false;
    }
    std::string budgetError;
    if (instantiationBudget_.enter(key, budgetError)) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidCall,
        diagnostics::DiagnosticSeverity::Error,
        currentInstantiationRange_,
        budgetError);
    return false;
}

std::size_t ProjectGenericModel::reserveMethodSpecialization(
    TemplateState& state,
    const std::string& key,
    const std::string& specializedName,
    const std::vector<std::string>& identities)
{
    const std::size_t ordinal = specializations_.size();
    specializationPlanner_.commit(key, ordinal);
    specializations_.push_back(GenericSpecializationRecord{
        GenericTemplateKind::Method, state.syntax, nullptr, state.record.canonicalKey,
        key, specializedName, identities, ordinal, state.ordinal, nullptr,
        currentInstantiationRange_, currentOverloadGroup_,
        InstantiationContext{
            currentInstantiationProjectOrdinal_, currentInstantiationNamespace_,
            currentInstantiationRange_, currentOverloadGroup_,
            activePackSubstitutions_, activeAssociatedProjections_}});
    specializations_.back().state = SpecializationState::Building;
    return ordinal;
}

ast::AstNode* ProjectGenericModel::cloneMethodSpecialization(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    const SpecializationContext& callerContext,
    std::string& packError,
    SourceRange& packErrorRange,
    std::string& associatedError,
    SourceRange& associatedErrorRange)
{
    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    prepareSpecializationSubstitutions(state, arguments, substitutions);
    packMaterializationError_.clear();
    packMaterializationErrorRange_ = {};
    activeAssociatedProjections_.clear();
    associatedProjectionError_.clear();
    associatedProjectionErrorRange_ = {};
    ast::AstNode* clone = cloneNode(state.unit->arena(), state.syntax, substitutions, "", "");
    packError = packMaterializationError_;
    packErrorRange = packMaterializationErrorRange_;
    associatedError = associatedProjectionError_;
    associatedErrorRange = associatedProjectionErrorRange_;
    packMaterializationError_ = callerContext.packError;
    packMaterializationErrorRange_ = callerContext.packErrorRange;
    activePackSubstitutions_ = callerContext.packSubstitutions;
    activeAssociatedProjections_ = callerContext.associatedProjections;
    associatedProjectionError_ = callerContext.associatedError;
    associatedProjectionErrorRange_ = callerContext.associatedErrorRange;
    return clone;
}

bool ProjectGenericModel::reportMethodSpecializationError(
    const std::string& key,
    const std::string& packError,
    const SourceRange& packErrorRange,
    const std::string& associatedError,
    const SourceRange& associatedErrorRange,
    const SpecializationContext& callerContext,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (packError.empty() && associatedError.empty()) return false;
    specializationPlanner_.rollback(key);
    specializations_.pop_back();
    diagnostics.report(
        associatedError.empty()
            ? diagnostics::DiagnosticId::InvalidArgumentCount
            : diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        associatedError.empty() ? packErrorRange : associatedErrorRange,
        associatedError.empty() ? packError : associatedError);
    currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
    currentInstantiationNamespace_ = callerContext.namespaceName;
    currentInstantiationRange_ = callerContext.range;
    currentOverloadGroup_ = callerContext.overloadGroup;
    instantiationBudget_.leave();
    return true;
}

void ProjectGenericModel::commitMethodSpecialization(
    TemplateState& state,
    ast::AstNode* clone,
    const std::string& key,
    const std::string& specializedName,
    std::size_t ordinal)
{
    const GenericTelemetry::Scope commit(telemetry_, "generics.rewrite-commit");
    clone->setText(specializedName);
    clone->clearGenericParameters();
    std::string specializationMetadata = "stage44-specialization:" + key;
    if (!state.syntax->auxiliaryText().empty())
        specializationMetadata += ";nsc-meta:" + state.syntax->auxiliaryText();
    clone->setAuxiliaryText(std::move(specializationMetadata));
    state.parent->addChild(clone);
    specializations_[ordinal].specializedSyntax = clone;
    specializationBySyntax_[clone] = ordinal;
    specializations_[ordinal].state = SpecializationState::Ready;
}

std::string ProjectGenericModel::completeMethodSpecialization(
    TemplateState& state,
    ast::AstNode* clone,
    std::span<ast::TypeSyntax* const> arguments,
    const std::string& key,
    const std::string& specializedName,
    std::size_t ordinal,
    const SpecializationContext& callerContext,
    diagnostics::DiagnosticEngine& diagnostics)
{
    commitMethodSpecialization(state, clone, key, specializedName, ordinal);
    if (state.parent != nullptr && !state.parent->genericParameters().empty())
    {
        std::vector<ast::AstNode*> closedOwners;
        for (const GenericSpecializationRecord& specialization : specializations_)
            if (specialization.templateSyntax == state.parent && specialization.specializedSyntax != nullptr)
                closedOwners.push_back(const_cast<ast::AstNode*>(specialization.specializedSyntax));
        std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
        prepareSpecializationSubstitutions(state, arguments, substitutions);
        for (ast::AstNode* owner : closedOwners)
        {
            const ast::AstNode* source = nullptr;
            for (const ast::AstNode* member : owner->children())
                if (member != nullptr && isGenericMethodTemplateNode(member) &&
                    member->range().begin().fileId() == state.syntax->range().begin().fileId() &&
                    member->range().begin().offset() == state.syntax->range().begin().offset())
                {
                    source = member;
                    break;
                }
            if (source == nullptr) continue;
            ast::AstNode* closedMethod = cloneNode(state.unit->arena(), source, substitutions, "", "");
            closedMethod->setText(specializedName);
            closedMethod->clearGenericParameters();
            closedMethod->setAuxiliaryText(clone->auxiliaryText());
            owner->addChild(closedMethod);
            GenericLookupScope memberScope;
            memberScope.importedNamespaces.write() = state.record.declarationImports;
            memberScope.aliases.write().insert(
                state.record.declarationAliases.begin(), state.record.declarationAliases.end());
            processNode(*state.unit, closedMethod, state.record.namespaceName, owner->text(),
                        std::move(memberScope), false, diagnostics);
        }
    }
    GenericLookupScope declarationScope;
    if (!state.declarationImports.empty())
        declarationScope.importedNamespaces.write() = state.declarationImports;
    if (!state.declarationAliases.empty())
        declarationScope.aliases.write() = state.declarationAliases;
    if (state.parent == nullptr || state.parent->genericParameters().empty())
        processNode(
            *state.unit, clone, state.record.namespaceName, state.record.ownerName,
            std::move(declarationScope), false, diagnostics);
    currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
    currentInstantiationNamespace_ = callerContext.namespaceName;
    currentInstantiationRange_ = callerContext.range;
    currentOverloadGroup_ = callerContext.overloadGroup;
    activePackSubstitutions_ = callerContext.packSubstitutions;
    instantiationBudget_.leave();
    return specializedName;
}

std::string ProjectGenericModel::ensureMethodSpecialization(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string_view sharedName,
    ast::AstNode* closedOwner,
    std::string_view closedOwnerKey)
{
    SpecializationContext callerContext;
    callerContext.save(*this);
#if 0 // INSTRUMENTATION CODE
    const GenericWorkProgress progress(
        telemetry_, "generics.method-specialization",
        state.record.canonicalKey + " arguments=" + std::to_string(arguments.size()));
#endif // INSTRUMENTATION CODE

    std::vector<std::string> spellings;
    std::vector<std::string> identities;
    std::string key = computeSpecializationKey(
        state.record, arguments, spellings, identities);
    const bool hasClosedGenericOwner = closedOwner != nullptr && !closedOwnerKey.empty() &&
        state.parent != nullptr && !state.parent->genericParameters().empty() &&
        std::ranges::any_of(specializations_, [&](const GenericSpecializationRecord& owner) {
            return (owner.specializedSyntax == closedOwner || owner.specializedSyntax == nullptr) &&
                owner.templateSyntax == state.parent &&
                owner.specializationKey == closedOwnerKey;
        });
    if (hasClosedGenericOwner) key += "@owner:" + std::string(closedOwnerKey);
    if (const auto found = specializationPlanner_.request(key))
    {
        telemetry_.noteSpecializationHit();
        ++instantiationCacheHitCount_;
        return specializations_[*found].specializedName;
    }

    if (!beginMethodSpecialization(state, key, diagnostics)) return {};
    SpecializationWaveTimer wave(telemetry_, state.record.canonicalKey);

    const std::string specializedName = methodSpecializedName(
        state, arguments.size(), spellings, key, sharedName);

    const std::size_t ordinal =
        reserveMethodSpecialization(state, key, specializedName, identities);

    std::string packError;
    SourceRange packErrorRange{};
    std::string associatedError;
    SourceRange associatedErrorRange{};
    TemplateState closedState = state;
    if (hasClosedGenericOwner)
    {
        closedState.syntax = nullptr;
        for (ast::AstNode* member : closedOwner->children())
            if (member != nullptr && isGenericMethodTemplateNode(member) &&
                member->range().begin() == state.syntax->range().begin())
            {
                closedState.syntax = member;
                break;
            }
        if (closedState.syntax == nullptr)
        {
            const auto imported = specializationPlanner_.request(std::string(closedOwnerKey));
            if (imported.has_value() && *imported < specializations_.size())
            {
                const GenericSpecializationRecord& owner = specializations_[*imported];
                const GenericTemplateRecord& ownerTemplate = templateStates_[owner.templateOrdinal].record;
                std::unordered_map<std::string, const ast::TypeSyntax*> ownerSubstitutions;
                bool supported = owner.typeArguments.size() == ownerTemplate.parameters.size();
                for (std::size_t i = 0; supported && i < owner.typeArguments.size(); ++i)
                {
                    const std::string& identity = owner.typeArguments[i];
                    std::string baseIdentity = identity;
                    std::string spelling;
                    std::size_t dynamicArrayDepth = 0;
                    while (!baseIdentity.starts_with("const:") && baseIdentity.ends_with("[]"))
                    {
                        baseIdentity.resize(baseIdentity.size() - 2U);
                        ++dynamicArrayDepth;
                    }
                    lexer::TokenKind kind = lexer::TokenKind::Identifier;
                    if (baseIdentity.starts_with("const:"))
                    {
                        const std::size_t separator = baseIdentity.find('=', 6U);
                        if (separator == std::string::npos) { supported = false; break; }
                        spelling = language::constGenericArgumentMarker(
                            baseIdentity.substr(6U, separator - 6U), baseIdentity.substr(separator + 1U));
                    }
                    else if (baseIdentity.starts_with("builtin:"))
                    {
                        spelling = baseIdentity.substr(8U);
                        kind = lexer::keywordKind(spelling);
                        if (kind == lexer::TokenKind::Identifier) { supported = false; break; }
                    }
                    else if (baseIdentity.starts_with("type:"))
                        spelling = baseIdentity.substr(5U);
                    else if (baseIdentity.starts_with("generic:"))
                    {
                        const auto nestedOrdinal = specializationPlanner_.request(baseIdentity.substr(8U));
                        if (!nestedOrdinal.has_value() || *nestedOrdinal >= specializations_.size())
                        {
                            supported = false;
                            break;
                        }
                        const GenericSpecializationRecord& nested = specializations_[*nestedOrdinal];
                        if (nested.templateOrdinal >= templateStates_.size() || nested.kind == GenericTemplateKind::Method)
                        {
                            supported = false;
                            break;
                        }
                        const GenericTemplateRecord& nestedTemplate = templateStates_[nested.templateOrdinal].record;
                        spelling = nestedTemplate.moduleName + "::" + qualifiedName(
                            nestedTemplate.namespaceName, appendOwner(nestedTemplate.ownerName, nested.specializedName));
                    }
                    else
                    {
                        spelling = baseIdentity;
                        kind = lexer::keywordKind(spelling);
                        if (kind == lexer::TokenKind::Identifier)
                        {
                            const GenericSpecializationRecord* matched = nullptr;
                            for (const GenericSpecializationRecord& nested : specializations_)
                            {
                                if (nested.templateOrdinal >= templateStates_.size() || nested.kind == GenericTemplateKind::Method)
                                    continue;
                                const GenericTemplateRecord& nestedTemplate = templateStates_[nested.templateOrdinal].record;
                                std::string written = nestedTemplate.name + "<";
                                bool comparable = !nested.typeArguments.empty();
                                for (std::size_t j = 0; comparable && j < nested.typeArguments.size(); ++j)
                                {
                                    if (j != 0) written += ',';
                                    const std::string& item = nested.typeArguments[j];
                                    if (item.starts_with("const:")) written += "const(" + item.substr(6U) + ")";
                                    else if (lexer::keywordKind(item) != lexer::TokenKind::Identifier) written += item;
                                    else if (item.starts_with("type:"))
                                    {
                                        const std::string_view key(item.data() + 5U, item.size() - 5U);
                                        const std::size_t separator = key.find_last_of(".+:");
                                        const std::string_view terminal = separator == std::string_view::npos
                                            ? key : key.substr(separator + 1U);
                                        std::unordered_set<std::string> identities;
                                        for (const DeclaredTypeIdentity& declared : declaredTypeIdentities_)
                                        {
                                            const std::size_t dot = declared.sourceName.find_last_of('.');
                                            const std::string_view candidate = dot == std::string::npos
                                                ? std::string_view(declared.sourceName)
                                                : std::string_view(declared.sourceName).substr(dot + 1U);
                                            if (candidate == terminal) identities.insert(declared.identityKey);
                                        }
                                        if (identities.size() == 1U && identities.contains(std::string(key)))
                                            written.append(terminal);
                                        else comparable = false;
                                    }
                                    else comparable = false;
                                }
                                written += ">@origin-module[" + std::to_string(nestedTemplate.moduleName.size()) +
                                    "]=" + nestedTemplate.moduleName + ";namespace[" +
                                    std::to_string(nestedTemplate.namespaceName.size()) + "]=" + nestedTemplate.namespaceName;
                                if (!comparable || written != baseIdentity) continue;
                                if (matched != nullptr) { matched = nullptr; break; }
                                matched = &nested;
                            }
                            if (matched == nullptr) { supported = false; break; }
                            const GenericTemplateRecord& nestedTemplate = templateStates_[matched->templateOrdinal].record;
                            spelling = nestedTemplate.moduleName + "::" + qualifiedName(
                                nestedTemplate.namespaceName,
                                appendOwner(nestedTemplate.ownerName, matched->specializedName));
                        }
                    }
                    ast::TypeSyntax* argument = state.unit->arena().make<ast::TypeSyntax>(
                        state.syntax->range(), kind, spelling);
                    for (std::size_t depth = 0; depth < dynamicArrayDepth; ++depth)
                        argument->addModifier(ast::TypeModifierSyntax{
                            ast::TypeModifierSyntaxKind::DynamicArray, nullptr, state.syntax->range(), 0U});
                    if (identity.starts_with("type:")) recordResolvedTypeIdentity(argument, identity);
                    else if (identity.starts_with("generic:")) recordResolvedTypeIdentity(argument, identity);
                    else if (kind == lexer::TokenKind::Identifier && !identity.starts_with("const:"))
                        recordResolvedTypeIdentity(argument, "type:" + spelling);
                    ownerSubstitutions.emplace(ownerTemplate.parameters[i], argument);
                }
                if (supported)
                    closedState.syntax = cloneNode(state.unit->arena(), state.syntax, ownerSubstitutions, "", "");
            }
        }
        if (closedState.syntax == nullptr)
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidType,
                               diagnostics::DiagnosticSeverity::Error, currentInstantiationRange_,
                               "generic method declaration is missing from its closed owner");
            specializationPlanner_.rollback(key);
            specializations_.pop_back();
            instantiationBudget_.leave();
            return {};
        }
        closedState.parent = closedOwner;
        closedState.record.ownerName = closedOwner->text();
    }
    TemplateState& materializationState = hasClosedGenericOwner ? closedState : state;
    ast::AstNode* clone = cloneMethodSpecialization(
        materializationState, arguments, callerContext, packError, packErrorRange,
        associatedError, associatedErrorRange);
    if (reportMethodSpecializationError(
            key, packError, packErrorRange, associatedError,
            associatedErrorRange, callerContext, diagnostics))
        return {};
    return completeMethodSpecialization(
        materializationState, clone, arguments, key, specializedName, ordinal, callerContext, diagnostics);
}
}
