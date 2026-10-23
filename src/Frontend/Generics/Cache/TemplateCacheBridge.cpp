#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../../Internal/CloneDepthGuard.h"
#include "../../Cache/CachePayloadReader.h"
#include "../../Internal/EnumConstResolver.h"
#include "../../Internal/SpecializationWaveTimer.h"
#include "../../Internal/InterfaceFingerprint.h"
#include "../../Internal/ImplementationFingerprint.h"
#include "../../Internal/LocatedNode.h"
#include "../../Internal/MirFunctionKey.h"
#include "../../Internal/MirCacheRecord.h"
#include "../../Internal/ProjectGenericDependencyFrame.h"
#include "../../Internal/ProjectGenericBodyFrame.h"
#include "../../Internal/ProjectGenericAliasFrame.h"
#include "../../Internal/ProjectGenericChildWork.h"
#include "../../Internal/ProjectGenericCollectFrame.h"
#include "../../Internal/ProjectGenericProcessFrame.h"
#include "../../Internal/ProjectGenericReservedDemand.h"
#include "../../Internal/TypeWork.h"
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
#include "../GenericModelSupport.h"
#include "../Support/TemplateClassification.h"
#include "../Support/TemplateSpelling.h"
#include "../Support/ConstGenericArguments.h"
#include "../Support/GenericPatterns.h"
#include "TemplateCatalogCodec.h"
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
// NSC305-C02. Warm-build restore of one template record: on a cache hit, deserialize the metadata,
// link the walked syntax node and push the TemplateState (skipping the record construction). The
// walk still finds the node + parent (the parse already built them); the metadata + dependency
// scan result are restored.
bool ProjectGenericModel::tryRestoreTemplateRecord(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    ast::AstNode* parent,
    std::string_view currentNamespace,
    std::string_view ownerName)
{
    if (!templateCache_.enabled()) return false;
    const std::string fingerprint = computeTemplateSourceFingerprint(unit, node);
    const std::string key = templateCacheKey(fingerprint);
    std::string payload;
    bool hit = false;
    std::string error;
    if (!templateCache_.restore(key, payload, hit, error) || !hit) return false;
    GenericTemplateRecord record;
    if (!internal::deserializeTemplateRecord(payload, record)) return false;

    // The payload owns reusable metadata, but these fields belong to the current syntax graph.
    // Rebind them exactly as the cold collector does before publishing the restored state.
    record.syntax = node;
    record.projectOrdinal = unit.projectOrdinal();
    record.sourceOrdinal = unit.sourceOrdinal();
    record.moduleName = std::string(unit.moduleName());
    record.namespaceName = std::string(currentNamespace);
    record.ownerName = std::string(ownerName);
    record.name = node->text();
    record.parameters = node->genericParameters();
    record.parameterSyntaxes = node->genericParameterSyntaxes();
    record.constraints = node->genericConstraints();
    record.extensionSyntaxes.clear();
    if (const auto extensions = traitImplExtensions_.find(node);
        extensions != traitImplExtensions_.end())
    {
        record.extensionSyntaxes = extensions->second;
    }

    if (isGenericTypeTemplateNode(node))
    {
        record.kind = genericTypeTemplateKind(node);
        record.accessibility = genericTemplateAccessibility(node, false);
        record.qualifiedTypeName = appendOwner(record.ownerName, record.name);
        record.canonicalKey = record.moduleName + "::" +
            qualifiedName(record.namespaceName, record.qualifiedTypeName) +
            "`" + genericAritySpelling(record);
        record.specializationFamilyKey.clear();
        record.isControlledSpecialization = false;
        record.isControlledSpecializationFamily = false;
    }
    else if (isGenericMethodTemplateNode(node))
    {
        record.kind = GenericTemplateKind::Method;
        record.accessibility = genericTemplateAccessibility(
            node, node->kind() == ast::AstNodeKind::MethodDeclaration);
        record.qualifiedTypeName = record.ownerName;
        const std::string callableScope = record.ownerName.empty()
            ? record.namespaceName
            : qualifiedName(record.namespaceName, record.ownerName);
        record.canonicalKey = record.moduleName + "::" +
            (callableScope.empty() ? std::string{} : callableScope + ".") +
            record.name + "``" + genericAritySpelling(record);
        record.specializationFamilyKey = methodSpecializationFamilyKey(record, node);
        record.isControlledSpecialization =
            node->auxiliaryText() == "controlled-specialization";
        record.isControlledSpecializationFamily = false;
    }
    else
    {
        return false;
    }
    record.sourceFingerprint = fingerprint;
    record.c1CacheKey = key;
    telemetry_.noteTemplate(static_cast<std::size_t>(record.kind));
    templateStates_.push_back(
        TemplateState{std::move(record), &unit, node, parent, templateStates_.size(), {}});
    return true;
}

// NSC305-C01. Persists the collected template records to the C1 cache (the relocatable metadata
// payload). The AST subtree restore is a later layer; this stores the record skeleton.
void ProjectGenericModel::storeTemplateRecords()
{
    if (!templateCache_.enabled()) return;
    for (const TemplateState& state : templateStates_)
    {
        const std::string payload = serializeCatalogEntry(state.record);
        std::string error;
        (void)templateCache_.store(state.record.c1CacheKey, payload, error);
    }
}

// NSC305-C04. Stores a MIR cache record under the implementation fingerprint key (the C5 stable
// partition key). No-op when the cache is disabled.
void ProjectGenericModel::storeMirCacheRecord(
    std::string_view mangledName,
    std::string_view genericSpecializationKey,
    std::string_view returnTypeKey,
    std::string_view errorTypeKey,
    const std::vector<std::string>& parameterTypeKeys,
    std::string_view interfaceFingerprint,
    std::string_view implementationFingerprint)
{
    if (!templateCache_.enabled()) return;
    MirCacheRecord record;
    record.key.mangledName = std::string(mangledName);
    record.key.genericSpecializationKey = std::string(genericSpecializationKey);
    record.key.returnTypeKey = std::string(returnTypeKey);
    record.key.errorTypeKey = std::string(errorTypeKey);
    record.key.parameterTypeKeys = parameterTypeKeys;
    record.interfaceFingerprint = std::string(interfaceFingerprint);
    record.implementationFingerprint = std::string(implementationFingerprint);
    const std::string payload = serializeMirCacheRecord(record);
    std::string error;
    (void)templateCache_.store(record.implementationFingerprint, payload, error);
}

// NSC305-C04. Restores a MIR cache record by implementation fingerprint. Returns false on miss or
// disabled cache.
bool ProjectGenericModel::restoreMirCacheRecord(
    std::string_view implementationFingerprint,
    std::string& mangledName,
    std::string& genericSpecializationKey,
    std::string& returnTypeKey,
    std::string& errorTypeKey,
    std::vector<std::string>& parameterTypeKeys,
    std::string& interfaceFingerprint)
{
    if (!templateCache_.enabled()) return false;
    std::string payload;
    bool hit = false;
    std::string error;
    if (!templateCache_.restore(
            std::string(implementationFingerprint), payload, hit, error) ||
        !hit)
    {
        return false;
    }
    MirCacheRecord record;
    if (!internal::restoreMirCacheRecord(payload, record)) return false;
    mangledName = std::move(record.key.mangledName);
    genericSpecializationKey = std::move(record.key.genericSpecializationKey);
    returnTypeKey = std::move(record.key.returnTypeKey);
    errorTypeKey = std::move(record.key.errorTypeKey);
    parameterTypeKeys = std::move(record.key.parameterTypeKeys);
    interfaceFingerprint = std::move(record.interfaceFingerprint);
    return true;
}

// NSC305-C01/C02. Warm-build restore: look up the C1/C2 cache and count a hit when the FULL catalog
// entry (record + syntax AST) round-trips into the unit arena. The AST splice into the unit tree is
// a later layer; this validates the persisted snapshot.
void ProjectGenericModel::restoreTemplateRecords()
{
    if (!templateCache_.enabled()) return;
    for (const TemplateState& state : templateStates_)
    {
        std::string payload;
        bool hit = false;
        std::string error;
        if (templateCache_.restore(state.record.c1CacheKey, payload, hit, error) && hit)
        {
            GenericTemplateRecord restored;
            if (restoreCatalogEntry(state.unit->arena(), payload, restored))
            {
                telemetry_.noteSpecializationHit();
                ++instantiationCacheHitCount_;
            }
        }
    }
}
}
