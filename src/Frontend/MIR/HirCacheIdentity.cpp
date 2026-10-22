#include "HirCacheIdentity.h"

#include "../Cache/CachePayloadWriter.h"

#include "nativesharp/Frontend/ProjectOopHirFunction.h"

#include <bit>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
using internal::CachePayloadWriter;

template <typename Value>
void appendOptionalSize(std::string& out, const std::optional<Value>& value)
{
    CachePayloadWriter::byte(out, value.has_value() ? 1U : 0U);
    CachePayloadWriter::u64(out, value.has_value() ? static_cast<std::uint64_t>(*value) : 0U);
}

template <typename Value>
void appendEnumVector(std::string& out, const std::vector<Value>& values)
{
    CachePayloadWriter::u64(out, values.size());
    for (const Value value : values)
        CachePayloadWriter::u32(out, static_cast<std::uint32_t>(value));
}

void appendBoolVector(std::string& out, const std::vector<bool>& values)
{
    CachePayloadWriter::u64(out, values.size());
    for (const bool value : values) CachePayloadWriter::byte(out, value ? 1U : 0U);
}

void appendSyntaxIdentity(std::string& out, const ast::AstNode* syntax)
{
    CachePayloadWriter::byte(out, syntax != nullptr ? 1U : 0U);
    if (syntax == nullptr) return;
    const SourceRange range = syntax->range();
    CachePayloadWriter::u32(out, range.begin().fileId().value());
    CachePayloadWriter::u32(out, range.begin().offset());
    CachePayloadWriter::u32(out, range.end().fileId().value());
    CachePayloadWriter::u32(out, range.end().offset());
}

void appendNodeOrdinals(std::string& out, const ProjectOopHirNode& node)
{
    appendOptionalSize(out, node.memberOrdinal);
    appendOptionalSize(out, node.namespaceFunctionOrdinal);
    appendOptionalSize(out, node.namespaceStateOrdinal);
    appendOptionalSize(out, node.secondaryMemberOrdinal);
    appendOptionalSize(out, node.resultSuccessTestMemberOrdinal);
    appendOptionalSize(out, node.resultSuccessExtractorMemberOrdinal);
    appendOptionalSize(out, node.resultErrorExtractorMemberOrdinal);
    appendOptionalSize(out, node.resultErrorFactoryMemberOrdinal);
    appendOptionalSize(out, node.resultErrorConversionMemberOrdinal);
    appendOptionalSize(out, node.localOrdinal);
    appendOptionalSize(out, node.parameterOrdinal);
    appendOptionalSize(out, node.constructionOrdinal);
    appendOptionalSize(out, node.targetTypeOrdinal);
    appendOptionalSize(out, node.exceptionTypeOrdinal);
    appendOptionalSize(out, node.cleanupRegionOrdinal);
    appendOptionalSize(out, node.hirRegionOrdinal);
    appendOptionalSize(out, node.exceptionTargetRegionOrdinal);
    appendOptionalSize(out, node.targetScopeOrdinal);
    appendOptionalSize(out, node.disposeContractMemberOrdinal);
    appendOptionalSize(out, node.disposeImplementationMemberOrdinal);
}

void appendNodeControl(std::string& out, const ProjectOopHirNode& node)
{
    CachePayloadWriter::sizeVector(out, node.cleanupRegionOrdinals);
    appendOptionalSize(out, node.cleanupContinuationKind);
    appendOptionalSize(out, node.borrowKind);
    appendOptionalSize(out, node.localBorrowSourceKind);
    CachePayloadWriter::u32(out, static_cast<std::uint32_t>(node.captureMode));
    CachePayloadWriter::byte(out, node.conversion.has_value() ? 1U : 0U);
    if (node.conversion.has_value())
    {
        CachePayloadWriter::u32(
            out, static_cast<std::uint32_t>(node.conversion->kind));
        CachePayloadWriter::u32(out, static_cast<std::uint32_t>(node.conversion->rank));
        CachePayloadWriter::string(out, node.conversion->sourceTypeKey);
        CachePayloadWriter::string(out, node.conversion->targetTypeKey);
        appendOptionalSize(out, node.conversion->userDefinedMemberOrdinal);
    }
    CachePayloadWriter::int64Vector(out, node.switchLabels);
    CachePayloadWriter::byte(out, node.switchHasDefault ? 1U : 0U);
}

void appendNodeValues(std::string& out, const ProjectOopHirNode& node)
{
    CachePayloadWriter::u64(out, node.integerValue);
    CachePayloadWriter::u64(out, node.secondaryIntegerValue);
    CachePayloadWriter::byte(out, node.hasTrailingLengthBounds ? 1U : 0U);
    CachePayloadWriter::u64(
        out, static_cast<std::uint64_t>(node.trailingLengthRelativeOffset));
    appendOptionalSize(out, node.allocatorContextLocalOrdinal);
    appendOptionalSize(out, node.allocatorDeallocateMemberOrdinal);
    appendOptionalSize(out, node.allocatorTypeOrdinal);
    CachePayloadWriter::u64(out, node.resultSuccessTag);
    CachePayloadWriter::u64(out, node.resultErrorTag);
    CachePayloadWriter::u64(out, node.resultReturnErrorTag);
    CachePayloadWriter::string(out, node.resultSourceCarrierTypeKey);
    CachePayloadWriter::string(out, node.resultSourceErrorTypeKey);
    CachePayloadWriter::string(out, node.resultReturnCarrierTypeKey);
    CachePayloadWriter::string(out, node.resultReturnErrorTypeKey);
    appendOptionalSize(out, node.bitOffset);
    appendOptionalSize(out, node.bitWidth);
    CachePayloadWriter::u64(out, node.bitStorageSize);
    CachePayloadWriter::u64(out, node.bitStorageAlignment);
    CachePayloadWriter::u64(out, std::bit_cast<std::uint64_t>(node.floatingValue));
    CachePayloadWriter::byte(out, node.booleanValue ? 1U : 0U);
    appendEnumVector(out, node.collectionSegments);
}

void appendNodeFlags(std::string& out, const ProjectOopHirNode& node)
{
    const bool values[] = {
        node.referenceLocal, node.referenceMutable, node.staticLocal, node.pinnedStorage,
        node.referenceResult, node.governingReference, node.nullConditionalCall,
        node.returnsAssignedValue, node.mustTailCall, node.automaticDestruction,
        node.variableLengthArray, node.placementConstructed, node.stackPromotedObject,
        node.volatileAccess, node.baseReceiver, node.interfaceDispatch,
        node.targetIsInterface, node.dynamicDestruction, node.valueTypeNoOpDeletion,
        node.catchAll, node.catchHasFilter, node.catchExhaustive,
        node.interfaceCleanupDispatch};
    for (const bool value : values) CachePayloadWriter::byte(out, value ? 1U : 0U);
    CachePayloadWriter::u32(out, static_cast<std::uint32_t>(node.ownership));
    CachePayloadWriter::u32(out, static_cast<std::uint32_t>(node.objectLifetimeOwnership));
    CachePayloadWriter::u32(out, static_cast<std::uint32_t>(node.storageOwnership));
    CachePayloadWriter::u64(out, node.variableLengthArrayRank);
    CachePayloadWriter::u64(out, node.nativeArrayRank);
    CachePayloadWriter::u64(out, node.argumentCount);
}

void appendNode(std::string& out, const ProjectOopHirNode& node)
{
    CachePayloadWriter::u32(out, static_cast<std::uint32_t>(node.kind));
    appendSyntaxIdentity(out, node.syntax);
    CachePayloadWriter::string(out, node.typeKey);
    CachePayloadWriter::string(out, node.elementTypeKey);
    CachePayloadWriter::string(out, node.stringValue);
    CachePayloadWriter::string(out, node.debugDisplayName);
    CachePayloadWriter::u32(out, static_cast<std::uint32_t>(node.operation));
    appendNodeOrdinals(out, node);
    appendNodeControl(out, node);
    appendNodeValues(out, node);
    appendNodeFlags(out, node);
    CachePayloadWriter::stringVector(out, node.catchMatchedErrorTypeKeys);
    CachePayloadWriter::stringVector(out, node.argumentTypeKeys);
    CachePayloadWriter::sizeVector(out, node.argumentParameterOrdinals);
    CachePayloadWriter::u64(out, node.children.size());
}

void appendNodeTree(std::string& out, const ProjectOopHirNode* root)
{
    CachePayloadWriter::byte(out, root != nullptr ? 1U : 0U);
    if (root == nullptr) return;
    std::vector<const ProjectOopHirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopHirNode* node = work.back();
        work.pop_back();
        appendNode(out, *node);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

void appendFunctionFlags(std::string& out, const ProjectOopHirFunction& function)
{
    const bool values[] = {
        function.constructor, function.destructor, function.isStatic,
        function.referenceReturn, function.isNoHeap, function.isAsyncPoll,
        function.isExportedC, function.isImportedC, function.isVariadic,
        function.isImportedNative, function.isTransportedInlineBody,
        function.isSyntheticImplicitConstructor, function.importRequiresRuntimeBinding,
        function.isLambda, function.hasClosureEnvironment, function.isGeneratorConstructor,
        function.capturesThisCell};
    for (const bool value : values) CachePayloadWriter::byte(out, value ? 1U : 0U);
    CachePayloadWriter::u32(out, static_cast<std::uint32_t>(function.inliningPolicy));
    CachePayloadWriter::u32(out, static_cast<std::uint32_t>(function.callableTrait));
    CachePayloadWriter::u32(out, static_cast<std::uint32_t>(function.frameStorage));
    CachePayloadWriter::u32(out, static_cast<std::uint32_t>(function.returnOwnership));
    CachePayloadWriter::u64(out, function.effects.bits());
}

void appendFunctionVectors(std::string& out, const ProjectOopHirFunction& function)
{
    CachePayloadWriter::stringVector(out, function.captureTypeKeys);
    CachePayloadWriter::stringVector(out, function.captureNames);
    appendEnumVector(out, function.captureSourceKinds);
    appendEnumVector(out, function.captureModes);
    appendBoolVector(out, function.captureOwnsValues);
    CachePayloadWriter::sizeVector(out, function.capturedLocalOrdinals);
    CachePayloadWriter::stringVector(out, function.generatorParameterTypeKeys);
    CachePayloadWriter::sizeVector(out, function.capturedParameterOrdinals);
    CachePayloadWriter::stringVector(out, function.parameterNames);
    CachePayloadWriter::stringVector(out, function.parameterTypeKeys);
    appendEnumVector(out, function.parameterPassKinds);
    appendEnumVector(out, function.parameterOwnerships);
    CachePayloadWriter::sizeVector(out, function.localOrdinals);
}
} // namespace

std::string serializeHirCacheIdentity(const ProjectOopHirFunction& function)
{
    std::string out;
    CachePayloadWriter::string(out, "nsc-hir-cache-identity-v1");
    CachePayloadWriter::u64(out, function.memberOrdinal);
    appendOptionalSize(out, function.namespaceFunctionOrdinal);
    CachePayloadWriter::u64(out, function.declaringTypeOrdinal);
    appendFunctionFlags(out, function);
    CachePayloadWriter::string(out, function.importDllName);
    CachePayloadWriter::string(out, function.cAbiName);
    appendOptionalSize(out, function.lambdaOrdinal);
    CachePayloadWriter::u64(out, function.closureLayoutSize);
    CachePayloadWriter::u64(out, function.closureLayoutAlignment);
    CachePayloadWriter::string(out, function.genericSpecializationKey);
    CachePayloadWriter::string(out, function.mangledName);
    CachePayloadWriter::string(out, function.debugDisplayName);
    CachePayloadWriter::string(out, function.returnTypeKey);
    CachePayloadWriter::string(out, function.errorTypeKey);
    appendFunctionVectors(out, function);
    appendNodeTree(out, function.body.get());
    return out;
}
} // namespace nativesharp::frontend
