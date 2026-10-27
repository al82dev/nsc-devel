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
    processNode(
        *state.unit, product.clone, state.record.namespaceName, specializedName,
        GenericLookupScope{}, false, diagnostics);
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
    specializedName = state.record.name + "__g" + std::to_string(arguments.size());
    for (const std::string& spelling : spellings) specializedName += "__" + sanitize(spelling);
    specializedName += "__h" + stableHashSuffix(key);
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
                processNode(
                    *state.unit, clone, state.record.namespaceName, specializedName,
                    GenericLookupScope{}, false, diagnostics);
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
    processNode(
        *state.unit, clone, state.record.namespaceName, specializedName,
        GenericLookupScope{}, false, diagnostics);
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
    std::string specializedName = state.record.name + "__g" + std::to_string(arguments.size());
    for (const std::string& spelling : spellings) specializedName += "__" + sanitize(spelling);
    specializedName += "__h" + stableHashSuffix(key);
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
    const std::string& key,
    const std::string& specializedName,
    std::size_t ordinal,
    const SpecializationContext& callerContext,
    diagnostics::DiagnosticEngine& diagnostics)
{
    commitMethodSpecialization(state, clone, key, specializedName, ordinal);
    processNode(
        *state.unit, clone, state.record.namespaceName, state.record.ownerName,
        GenericLookupScope{}, false, diagnostics);
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
    std::string_view sharedName)
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
    const std::string key = computeSpecializationKey(
        state.record, arguments, spellings, identities);
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
    ast::AstNode* clone = cloneMethodSpecialization(
        state, arguments, callerContext, packError, packErrorRange,
        associatedError, associatedErrorRange);
    if (reportMethodSpecializationError(
            key, packError, packErrorRange, associatedError,
            associatedErrorRange, callerContext, diagnostics))
        return {};
    return completeMethodSpecialization(
        state, clone, key, specializedName, ordinal, callerContext, diagnostics);
}
}
