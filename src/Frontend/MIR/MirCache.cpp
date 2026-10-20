#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include "MirConstantEvaluator.h"
#include "Internal/MirByteReader.h"
#include "MirVerifier.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Language/CoroutineSemantics.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <string_view>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
// NSC305-C04. Normalized stream of a MIR node tree (the implementation fingerprint input): the
// observable identity (kind + key strings + topology), not the full layout. Iterative (no hot
// recursion); a private body change moves the hash.
std::string normalizeMirNodeStream(const ProjectOopMirNode* node)
{
    std::string out;
    std::vector<const ProjectOopMirNode*> work;
    if (node != nullptr) work.push_back(node);
    while (!work.empty())
    {
        const ProjectOopMirNode* current = work.back();
        work.pop_back();
        out += std::to_string(static_cast<std::uint32_t>(current->kind));
        out.push_back('|');
        out += current->typeKey;
        out.push_back('|');
        out += current->stringValue;
        out.push_back('|');
        out += current->calleeName;
        out.push_back(';');
        for (const std::unique_ptr<ProjectOopMirNode>& child : current->children)
            work.push_back(child.get());
    }
    return out;
}

[[maybe_unused]] std::string mirImplementationFingerprint(const ProjectOopMirNode* body)
{
    const std::string material = "mir\n" + normalizeMirNodeStream(body);
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

// NSC305-C04. Relocatable codec for a MIR node tree (the C4 cache payload). The syntax pointer is
// re-linked on restore; all scalars, strings, optionals, vectors and children round-trip.
void appendMirU32(std::string& out, std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32U; shift += 8U)
        out.push_back(static_cast<char>((value >> shift) & 0xFFu));
}

void appendMirU64(std::string& out, std::uint64_t value)
{
    for (unsigned shift = 0; shift < 64U; shift += 8U)
        out.push_back(static_cast<char>((value >> shift) & 0xFFu));
}

void appendMirString(std::string& out, std::string_view value)
{
    appendMirU32(out, static_cast<std::uint32_t>(value.size()));
    out.append(value);
}

void appendMirStringVector(std::string& out, const std::vector<std::string>& values)
{
    appendMirU32(out, static_cast<std::uint32_t>(values.size()));
    for (const std::string& value : values) appendMirString(out, value);
}

void appendMirSizeVector(std::string& out, const std::vector<std::size_t>& values)
{
    appendMirU32(out, static_cast<std::uint32_t>(values.size()));
    for (const std::size_t value : values) appendMirU64(out, static_cast<std::uint64_t>(value));
}

void appendMirInt64Vector(std::string& out, const std::vector<std::int64_t>& values)
{
    appendMirU32(out, static_cast<std::uint32_t>(values.size()));
    for (const std::int64_t value : values)
        appendMirU64(out, static_cast<std::uint64_t>(value));
}

void appendMirU32Vector(std::string& out, const std::vector<std::uint32_t>& values)
{
    appendMirU32(out, static_cast<std::uint32_t>(values.size()));
    for (const std::uint32_t value : values) appendMirU32(out, value);
}

void appendMirNodeCoreFields(std::string& out, const ProjectOopMirNode& node)
{
    appendMirU32(out, static_cast<std::uint32_t>(node.kind));
    appendMirString(out, node.typeKey);
    appendMirString(out, node.elementTypeKey);
    appendMirString(out, node.stringValue);
    appendMirString(out, node.debugDisplayName);
    appendMirU32(out, static_cast<std::uint32_t>(node.operation));
    appendMirU64(out, node.immediate);
    appendMirU64(out, node.secondaryImmediate);
    appendMirU64(out, node.integerValue);
    appendMirU64(out, node.bitStorageSize);
    appendMirU64(out, node.alignment);
    appendMirU64(out, node.resultSuccessTag);
    appendMirU64(out, node.resultErrorTag);
    appendMirU64(out, node.resultReturnErrorTag);
    appendMirString(out, node.resultSourceCarrierTypeKey);
    appendMirString(out, node.resultSourceErrorTypeKey);
    appendMirString(out, node.resultReturnCarrierTypeKey);
    appendMirString(out, node.resultReturnErrorTypeKey);
    appendMirString(out, node.allocatorDeallocateCalleeName);
    appendMirString(out, node.calleeName);
    appendMirString(out, node.secondaryCalleeName);
    appendMirString(out, node.resultSuccessTestCalleeName);
    appendMirString(out, node.resultSuccessExtractorCalleeName);
    appendMirString(out, node.resultErrorExtractorCalleeName);
    appendMirString(out, node.resultErrorFactoryCalleeName);
    appendMirString(out, node.resultErrorConversionCalleeName);
}

void appendMirNodeShapeFields(std::string& out, const ProjectOopMirNode& node)
{
    const std::uint64_t floatingBits = [&]()
    {
        std::uint64_t bits = 0;
        static_assert(sizeof(double) == sizeof(std::uint64_t));
        std::memcpy(&bits, &node.floatingValue, sizeof(bits));
        return bits;
    }();
    appendMirU64(out, floatingBits);
    appendMirU64(out, static_cast<std::uint64_t>(node.trailingLengthRelativeOffset));
    appendMirU32(
        out,
        node.variableLengthArrayRank ? static_cast<std::uint32_t>(node.variableLengthArrayRank) : 0U);
    appendMirU32(
        out,
        node.nativeArrayRank ? static_cast<std::uint32_t>(node.nativeArrayRank) : 0U);
    appendMirU32(out, static_cast<std::uint32_t>(node.argumentCount));
    appendMirStringVector(out, node.argumentTypeKeys);
    appendMirStringVector(out, node.catchMatchedErrorTypeKeys);
    appendMirU32(out, static_cast<std::uint32_t>(node.ownership));
    appendMirU32(out, static_cast<std::uint32_t>(node.objectLifetimeOwnership));
    appendMirU32(out, static_cast<std::uint32_t>(node.storageOwnership));
    appendMirU32(out, static_cast<std::uint32_t>(node.captureMode));
}

void appendMirNodeFlags(std::string& out, const ProjectOopMirNode& node)
{
    // Boolean flags.
    std::uint32_t flags = 0U;
    auto setFlag = [&flags](std::uint32_t bit, bool value)
    {
        if (value) flags |= (1U << bit);
    };
    setFlag(0, node.booleanValue);
    setFlag(1, node.switchHasDefault);
    setFlag(2, node.hasTrailingLengthBounds);
    setFlag(3, node.referenceLocal);
    setFlag(4, node.referenceMutable);
    setFlag(5, node.staticLocal);
    setFlag(6, node.pinnedStorage);
    setFlag(7, node.referenceResult);
    setFlag(8, node.governingReference);
    setFlag(9, node.nullConditionalCall);
    setFlag(10, node.returnsAssignedValue);
    setFlag(11, node.mustTailCall);
    setFlag(12, node.automaticDestruction);
    setFlag(13, node.variableLengthArray);
    setFlag(14, node.placementConstructed);
    setFlag(15, node.stackPromotedObject);
    setFlag(16, node.volatileAccess);
    setFlag(17, node.allocationFailureReturnsNull);
    setFlag(18, node.zeroInitialize);
    setFlag(19, node.baseReceiver);
    setFlag(20, node.targetIsInterface);
    setFlag(21, node.dynamicDestruction);
    setFlag(22, node.valueTypeNoOpDeletion);
    setFlag(23, node.catchAll);
    setFlag(24, node.catchHasFilter);
    setFlag(25, node.catchExhaustive);
    setFlag(26, node.interfaceCleanupDispatch);
    setFlag(27, node.inlineAssemblyHasSideEffects);
    setFlag(28, node.inlineAssemblyClobbersMemory);
    setFlag(29, node.inlineAssemblyClobbersFlags);
    appendMirU32(out, flags);
}

void appendMirOptionalU64(std::string& out, const std::optional<std::size_t>& value)
{
    out.push_back(value.has_value() ? '\x01' : '\x00');
    if (value.has_value()) appendMirU64(out, static_cast<std::uint64_t>(*value));
}

template <typename T>
void appendMirOptionalEnum(std::string& out, const std::optional<T>& value)
{
    out.push_back(value.has_value() ? '\x01' : '\x00');
    if (value.has_value())
        appendMirU32(out, static_cast<std::uint32_t>(*value));
}

void appendMirNodeOptionals(std::string& out, const ProjectOopMirNode& node)
{
    // Optional ordinals / enum optionals (present byte + value).
    appendMirOptionalU64(out, node.memberOrdinal);
    appendMirOptionalU64(out, node.namespaceFunctionOrdinal);
    appendMirOptionalU64(out, node.namespaceStateOrdinal);
    appendMirOptionalU64(out, node.secondaryMemberOrdinal);
    appendMirOptionalU64(out, node.resultSuccessTestMemberOrdinal);
    appendMirOptionalU64(out, node.resultSuccessExtractorMemberOrdinal);
    appendMirOptionalU64(out, node.resultErrorExtractorMemberOrdinal);
    appendMirOptionalU64(out, node.resultErrorFactoryMemberOrdinal);
    appendMirOptionalU64(out, node.resultErrorConversionMemberOrdinal);
    appendMirOptionalU64(out, node.localOrdinal);
    appendMirOptionalU64(out, node.parameterOrdinal);
    appendMirOptionalU64(out, node.constructionOrdinal);
    appendMirOptionalU64(out, node.targetTypeOrdinal);
    appendMirOptionalU64(out, node.exceptionTypeOrdinal);
    appendMirOptionalU64(out, node.cleanupRegionOrdinal);
    appendMirOptionalU64(out, node.mirRegionOrdinal);
    appendMirOptionalU64(out, node.exceptionTargetRegionOrdinal);
    appendMirOptionalU64(out, node.targetScopeOrdinal);
    appendMirOptionalU64(out, node.disposeContractMemberOrdinal);
    appendMirOptionalU64(out, node.disposeImplementationMemberOrdinal);
    appendMirOptionalU64(out, node.virtualSlot);
    appendMirOptionalU64(out, node.interfaceSlot);
    appendMirOptionalU64(out, node.allocatorContextLocalOrdinal);
    appendMirOptionalU64(out, node.allocatorDeallocateMemberOrdinal);
    appendMirOptionalU64(out, node.allocatorTypeOrdinal);
    appendMirOptionalEnum(out, node.cleanupContinuationKind);
    appendMirOptionalEnum(out, node.borrowKind);
    appendMirOptionalEnum(out, node.localBorrowSourceKind);
    appendMirOptionalEnum(out, node.conversionKind);
    appendMirOptionalU64(out, node.bitOffset);
    appendMirOptionalU64(out, node.bitWidth);
}

void appendMirNodeVectors(std::string& out, const ProjectOopMirNode& node)
{
    // Vectors.
    appendMirSizeVector(out, node.cleanupRegionOrdinals);
    appendMirInt64Vector(out, node.switchLabels);
    {
        std::vector<std::uint32_t> segments;
        segments.reserve(node.collectionSegments.size());
        for (const ProjectCollectionSegmentKind segment : node.collectionSegments)
            segments.push_back(static_cast<std::uint32_t>(segment));
        appendMirU32Vector(out, segments);
    }
    appendMirSizeVector(out, node.argumentParameterOrdinals);
}

// Serializes one node's scalar/string/flag fields (children recurse iteratively below).
void appendMirNodeFields(std::string& out, const ProjectOopMirNode& node)
{
    appendMirNodeCoreFields(out, node);
    appendMirNodeShapeFields(out, node);
    appendMirNodeFlags(out, node);
    appendMirNodeOptionals(out, node);
    appendMirNodeVectors(out, node);
}

[[maybe_unused]] std::string serializeMirNodeTree(const ProjectOopMirNode* body)
{
    std::string out;
    // Iterative pre-order: each node writes its fields + a child-count marker; the marker's
    // sentinel distinguishes a node from a null child.
    std::vector<const ProjectOopMirNode*> work;
    if (body != nullptr) work.push_back(body);
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr)
        {
            appendMirU32(out, 0xFFFFFFFEu); // null child marker
            continue;
        }
        appendMirU32(out, 0xFFFFFFFFu); // node marker
        appendMirNodeFields(out, *node);
        appendMirU32(out, static_cast<std::uint32_t>(node->children.size()));
        for (auto it = node->children.rbegin(); it != node->children.rend(); ++it)
            work.push_back(it->get());
    }
    appendMirU32(out, 0xFFFFFFFEu); // end marker
    return out;
}

bool readMirNodeCoreFields(internal::MirByteReader& reader, ProjectOopMirNode& node)
{
    std::uint32_t u32 = 0;
    if (!reader.u32(u32)) return false;
    node.kind = static_cast<ProjectOopMirNodeKind>(u32);
    if (!reader.string(node.typeKey) || !reader.string(node.elementTypeKey) ||
        !reader.string(node.stringValue) || !reader.string(node.debugDisplayName))
        return false;
    if (!reader.u32(u32)) return false;
    node.operation = static_cast<lexer::TokenKind>(u32);
    if (!reader.u64(node.immediate) || !reader.u64(node.secondaryImmediate) ||
        !reader.u64(node.integerValue) || !reader.u64(node.bitStorageSize) ||
        !reader.u64(node.alignment))
        return false;
    if (!reader.u64(node.resultSuccessTag) || !reader.u64(node.resultErrorTag) ||
        !reader.u64(node.resultReturnErrorTag))
        return false;
    if (!reader.string(node.resultSourceCarrierTypeKey) ||
        !reader.string(node.resultSourceErrorTypeKey) ||
        !reader.string(node.resultReturnCarrierTypeKey) ||
        !reader.string(node.resultReturnErrorTypeKey))
        return false;
    if (!reader.string(node.allocatorDeallocateCalleeName) || !reader.string(node.calleeName) ||
        !reader.string(node.secondaryCalleeName) || !reader.string(node.resultSuccessTestCalleeName) ||
        !reader.string(node.resultSuccessExtractorCalleeName) ||
        !reader.string(node.resultErrorExtractorCalleeName) ||
        !reader.string(node.resultErrorFactoryCalleeName) ||
        !reader.string(node.resultErrorConversionCalleeName))
        return false;
    return true;
}

bool readMirNodeShapeFields(internal::MirByteReader& reader, ProjectOopMirNode& node)
{
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
    if (!reader.u64(u64)) return false;
    std::memcpy(&node.floatingValue, &u64, sizeof(u64));
    if (!reader.u64(u64)) return false;
    node.trailingLengthRelativeOffset = static_cast<std::int64_t>(u64);
    if (!reader.u32(u32)) return false;
    node.variableLengthArrayRank = u32;
    if (!reader.u32(u32)) return false;
    node.nativeArrayRank = u32;
    if (!reader.u32(u32)) return false;
    node.argumentCount = u32;
    if (!reader.stringVector(node.argumentTypeKeys) ||
        !reader.stringVector(node.catchMatchedErrorTypeKeys))
        return false;
    if (!reader.u32(u32)) return false;
    node.ownership = static_cast<sema::OwnershipKind>(u32);
    if (!reader.u32(u32)) return false;
    node.objectLifetimeOwnership = static_cast<sema::OwnershipKind>(u32);
    if (!reader.u32(u32)) return false;
    node.storageOwnership = static_cast<sema::OwnershipKind>(u32);
    if (!reader.u32(u32)) return false;
    node.captureMode = static_cast<language::CaptureMode>(u32);
    return true;
}

void applyMirNodeFlags(std::uint32_t flags, ProjectOopMirNode& node)
{
    const auto flag = [&](std::uint32_t bit) { return (flags & (1U << bit)) != 0U; };
    node.booleanValue = flag(0);
    node.switchHasDefault = flag(1);
    node.hasTrailingLengthBounds = flag(2);
    node.referenceLocal = flag(3);
    node.referenceMutable = flag(4);
    node.staticLocal = flag(5);
    node.pinnedStorage = flag(6);
    node.referenceResult = flag(7);
    node.governingReference = flag(8);
    node.nullConditionalCall = flag(9);
    node.returnsAssignedValue = flag(10);
    node.mustTailCall = flag(11);
    node.automaticDestruction = flag(12);
    node.variableLengthArray = flag(13);
    node.placementConstructed = flag(14);
    node.stackPromotedObject = flag(15);
    node.volatileAccess = flag(16);
    node.allocationFailureReturnsNull = flag(17);
    node.zeroInitialize = flag(18);
    node.baseReceiver = flag(19);
    node.targetIsInterface = flag(20);
    node.dynamicDestruction = flag(21);
    node.valueTypeNoOpDeletion = flag(22);
    node.catchAll = flag(23);
    node.catchHasFilter = flag(24);
    node.catchExhaustive = flag(25);
    node.interfaceCleanupDispatch = flag(26);
    node.inlineAssemblyHasSideEffects = flag(27);
    node.inlineAssemblyClobbersMemory = flag(28);
    node.inlineAssemblyClobbersFlags = flag(29);
}

bool readMirOptionalU64(
    internal::MirByteReader& reader,
    std::optional<std::size_t>& value)
{
    std::uint8_t present = 0;
    if (!reader.byte(present)) return false;
    if (!present) { value.reset(); return true; }
    std::uint64_t u64 = 0;
    if (!reader.u64(u64)) return false;
    value = static_cast<std::size_t>(u64);
    return true;
}

template <typename T>
bool readMirOptionalEnum(
    internal::MirByteReader& reader,
    std::optional<T>& value)
{
    std::uint8_t present = 0;
    if (!reader.byte(present)) return false;
    if (!present) { value.reset(); return true; }
    std::uint32_t u32 = 0;
    if (!reader.u32(u32)) return false;
    value = static_cast<T>(u32);
    return true;
}

bool readMirNodeOptionals(internal::MirByteReader& reader, ProjectOopMirNode& node)
{
    if (!readMirOptionalU64(reader, node.memberOrdinal) ||
        !readMirOptionalU64(reader, node.namespaceFunctionOrdinal) ||
        !readMirOptionalU64(reader, node.namespaceStateOrdinal) ||
        !readMirOptionalU64(reader, node.secondaryMemberOrdinal) ||
        !readMirOptionalU64(reader, node.resultSuccessTestMemberOrdinal) ||
        !readMirOptionalU64(reader, node.resultSuccessExtractorMemberOrdinal) ||
        !readMirOptionalU64(reader, node.resultErrorExtractorMemberOrdinal) ||
        !readMirOptionalU64(reader, node.resultErrorFactoryMemberOrdinal) ||
        !readMirOptionalU64(reader, node.resultErrorConversionMemberOrdinal) ||
        !readMirOptionalU64(reader, node.localOrdinal) ||
        !readMirOptionalU64(reader, node.parameterOrdinal) ||
        !readMirOptionalU64(reader, node.constructionOrdinal) ||
        !readMirOptionalU64(reader, node.targetTypeOrdinal) ||
        !readMirOptionalU64(reader, node.exceptionTypeOrdinal) ||
        !readMirOptionalU64(reader, node.cleanupRegionOrdinal) ||
        !readMirOptionalU64(reader, node.mirRegionOrdinal) ||
        !readMirOptionalU64(reader, node.exceptionTargetRegionOrdinal) ||
        !readMirOptionalU64(reader, node.targetScopeOrdinal) ||
        !readMirOptionalU64(reader, node.disposeContractMemberOrdinal) ||
        !readMirOptionalU64(reader, node.disposeImplementationMemberOrdinal) ||
        !readMirOptionalU64(reader, node.virtualSlot) ||
        !readMirOptionalU64(reader, node.interfaceSlot) ||
        !readMirOptionalU64(reader, node.allocatorContextLocalOrdinal) ||
        !readMirOptionalU64(reader, node.allocatorDeallocateMemberOrdinal) ||
        !readMirOptionalU64(reader, node.allocatorTypeOrdinal) ||
        !readMirOptionalEnum(reader, node.cleanupContinuationKind) ||
        !readMirOptionalEnum(reader, node.borrowKind) ||
        !readMirOptionalEnum(reader, node.localBorrowSourceKind) ||
        !readMirOptionalEnum(reader, node.conversionKind) ||
        !readMirOptionalU64(reader, node.bitOffset) ||
        !readMirOptionalU64(reader, node.bitWidth))
        return false;
    return true;
}

bool readMirSizeVector(
    internal::MirByteReader& reader,
    std::vector<std::size_t>& values)
{
    std::uint32_t count = 0;
    if (!reader.u32(count)) return false;
    values.clear();
    values.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::uint64_t value = 0;
        if (!reader.u64(value)) return false;
        values.push_back(static_cast<std::size_t>(value));
    }
    return true;
}

bool readMirInt64Vector(
    internal::MirByteReader& reader,
    std::vector<std::int64_t>& values)
{
    std::uint32_t count = 0;
    if (!reader.u32(count)) return false;
    values.clear();
    values.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::uint64_t value = 0;
        if (!reader.u64(value)) return false;
        values.push_back(static_cast<std::int64_t>(value));
    }
    return true;
}

bool readMirNodeVectors(internal::MirByteReader& reader, ProjectOopMirNode& node)
{
    if (!readMirSizeVector(reader, node.cleanupRegionOrdinals) ||
        !readMirInt64Vector(reader, node.switchLabels))
        return false;
    {
        std::uint32_t count = 0;
        if (!reader.u32(count)) return false;
        node.collectionSegments.clear();
        node.collectionSegments.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            std::uint32_t segment = 0;
            if (!reader.u32(segment)) return false;
            node.collectionSegments.push_back(static_cast<ProjectCollectionSegmentKind>(segment));
        }
    }
    return readMirSizeVector(reader, node.argumentParameterOrdinals);
}

// Reads one node's scalar/string/flag/optional/vector fields (symmetric to appendMirNodeFields).
bool readMirNodeFields(internal::MirByteReader& reader, ProjectOopMirNode& node)
{
    if (!readMirNodeCoreFields(reader, node) || !readMirNodeShapeFields(reader, node))
        return false;
    std::uint32_t flags = 0;
    if (!reader.u32(flags)) return false;
    applyMirNodeFlags(flags, node);
    if (!readMirNodeOptionals(reader, node)) return false;
    return readMirNodeVectors(reader, node);
}

bool readMirNodeRecord(
    internal::MirByteReader& reader,
    std::unique_ptr<ProjectOopMirNode>& out)
{
    std::uint32_t marker = 0;
    if (!reader.u32(marker)) return false;
    if (marker == 0xFFFFFFFEu)
    {
        out.reset();
        return true;
    }
    if (marker != 0xFFFFFFFFu) return false;
    auto node = std::make_unique<ProjectOopMirNode>();
    if (!readMirNodeFields(reader, *node)) return false;
    std::uint32_t childCount = 0;
    if (!reader.u32(childCount)) return false;
    node->children.resize(childCount);
    out = std::move(node);
    return true;
}

void discardMirNodeTree(std::unique_ptr<ProjectOopMirNode>& root)
{
    std::vector<std::unique_ptr<ProjectOopMirNode>> work;
    if (root != nullptr) work.push_back(std::move(root));
    while (!work.empty())
    {
        std::unique_ptr<ProjectOopMirNode> node = std::move(work.back());
        work.pop_back();
        for (std::unique_ptr<ProjectOopMirNode>& child : node->children)
            if (child != nullptr) work.push_back(std::move(child));
        node->children.clear();
    }
}

bool readMirNode(internal::MirByteReader& reader, std::unique_ptr<ProjectOopMirNode>& out)
{
    std::unique_ptr<ProjectOopMirNode> root;
    if (!readMirNodeRecord(reader, root)) return false;
    if (root == nullptr)
    {
        out.reset();
        return true;
    }
    std::vector<std::pair<ProjectOopMirNode*, std::size_t>> work;
    work.emplace_back(root.get(), 0U);
    while (!work.empty())
    {
        auto& [node, childIndex] = work.back();
        if (childIndex == node->children.size())
        {
            work.pop_back();
            continue;
        }
        std::unique_ptr<ProjectOopMirNode>& child = node->children[childIndex++];
        if (!readMirNodeRecord(reader, child))
        {
            discardMirNodeTree(root);
            return false;
        }
        if (child != nullptr) work.emplace_back(child.get(), 0U);
    }
    out = std::move(root);
    return true;
}

[[maybe_unused]] bool restoreMirNodeTree(
    std::string_view bytes, std::unique_ptr<ProjectOopMirNode>& body)
{
    internal::MirByteReader reader(bytes);
    return readMirNode(reader, body);
}

// NSC305-C04. Pre-order index of a node inside a MIR body (SIZE_MAX when absent); the C4 payload
// stores node pointers as these indices so they can be re-linked after the restore.
std::size_t nodePreorderIndex(const ProjectOopMirNode* body, const ProjectOopMirNode* target)
{
    if (body == nullptr || target == nullptr) return std::numeric_limits<std::size_t>::max();
    std::size_t index = 0;
    std::vector<const ProjectOopMirNode*> work;
    work.push_back(body);
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == target) return index;
        ++index;
        for (auto it = node->children.rbegin(); it != node->children.rend(); ++it)
            work.push_back(it->get());
    }
    return std::numeric_limits<std::size_t>::max();
}

ProjectOopMirNode* nodeByPreorderIndex(ProjectOopMirNode* body, std::size_t targetIndex)
{
    if (body == nullptr) return nullptr;
    std::size_t index = 0;
    std::vector<ProjectOopMirNode*> work;
    work.push_back(body);
    while (!work.empty())
    {
        ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (index == targetIndex) return node;
        ++index;
        for (auto it = node->children.rbegin(); it != node->children.rend(); ++it)
            work.push_back(it->get());
    }
    return nullptr;
}

// NSC305-C04. The MIR function's cache payload: the observable identity + the body node tree. The
// remaining function fields (ABI/effects/closure/exception flow) are the next serialization layer.
void appendMirFunctionIdentity(std::string& out, const ProjectOopMirFunction& function)
{
    appendMirString(out, function.mangledName);
    appendMirString(out, function.genericSpecializationKey);
    appendMirString(out, function.returnTypeKey);
    appendMirString(out, function.errorTypeKey);
    appendMirStringVector(out, function.parameterTypeKeys);
    appendMirStringVector(out, function.parameterNames);
    appendMirStringVector(out, function.captureTypeKeys);
    appendMirStringVector(out, function.captureNames);
    appendMirStringVector(out, function.generatorParameterTypeKeys);
    appendMirString(out, function.importDllName);
    appendMirString(out, function.cAbiName);
    appendMirString(out, function.debugDisplayName);
    appendMirU64(out, function.memberOrdinal ? static_cast<std::uint64_t>(function.memberOrdinal) : 0U);
    appendMirU64(out, function.declaringTypeOrdinal);
    appendMirU64(out, function.closureLayoutSize);
    appendMirU64(out, function.closureLayoutAlignment);
    appendMirU64(out, function.asyncFrameSize);
    appendMirU32(out, static_cast<std::uint32_t>(function.inliningPolicy));
    appendMirU32(out, static_cast<std::uint32_t>(function.callableTrait));
    appendMirU32(out, static_cast<std::uint32_t>(function.frameStorage));
    appendMirU32(out, static_cast<std::uint32_t>(function.returnOwnership));
    out.push_back(function.lambdaOrdinal.has_value() ? '\x01' : '\x00');
    if (function.lambdaOrdinal.has_value())
        appendMirU64(out, static_cast<std::uint64_t>(*function.lambdaOrdinal));
}

void appendMirFunctionFlags(std::string& out, const ProjectOopMirFunction& function)
{
    std::uint32_t flags = 0U;
    auto setFlag = [&flags](std::uint32_t bit, bool value)
    {
        if (value) flags |= (1U << bit);
    };
    setFlag(0, function.constructor);
    setFlag(1, function.destructor);
    setFlag(2, function.isStatic);
    setFlag(3, function.referenceReturn);
    setFlag(4, function.isNoHeap);
    setFlag(5, function.isExportedC);
    setFlag(6, function.isImportedC);
    setFlag(7, function.isVariadic);
    setFlag(8, function.isImportedNative);
    setFlag(9, function.isSyntheticImplicitConstructor);
    setFlag(10, function.importRequiresRuntimeBinding);
    setFlag(11, function.isLambda);
    setFlag(12, function.hasClosureEnvironment);
    setFlag(13, function.capturesThisCell);
    setFlag(14, function.isGeneratorConstructor);
    setFlag(15, function.isTransportedInlineBody);
    appendMirU32(out, flags);
}

void appendMirFunctionAbi(std::string& out, const ProjectOopMirFunction& function)
{
    appendMirU32(out, function.effects.bits());
    appendMirU32(out, static_cast<std::uint32_t>(function.abi.returnPassing));
    out.push_back(function.abi.hiddenReturnParameterOrdinal.has_value() ? '\x01' : '\x00');
    if (function.abi.hiddenReturnParameterOrdinal.has_value())
        appendMirU64(out, static_cast<std::uint64_t>(*function.abi.hiddenReturnParameterOrdinal));
    {
        std::vector<std::uint32_t> passing;
        passing.reserve(function.abi.parameterPassing.size());
        for (const ProjectOopMirAbiPassing p : function.abi.parameterPassing)
            passing.push_back(static_cast<std::uint32_t>(p));
        appendMirU32Vector(out, passing);
    }
    appendMirSizeVector(out, function.abi.parameterPhysicalOrdinals);
    appendMirU32(out, static_cast<std::uint32_t>(function.abi.physicalParameterCount));
}

template <typename Tuples>
void appendMirAsyncTuples(std::string& out, const Tuples& tuples)
{
    appendMirU32(out, static_cast<std::uint32_t>(tuples.size()));
    for (const auto& [ordinal, offset, typeKey] : tuples)
    {
        appendMirU64(out, static_cast<std::uint64_t>(ordinal));
        appendMirU64(out, offset);
        appendMirString(out, typeKey);
    }
}

void appendMirFunctionCaptures(std::string& out, const ProjectOopMirFunction& function)
{
    appendMirAsyncTuples(out, function.asyncFrameLocals);
    appendMirAsyncTuples(out, function.asyncFrameParameters);
    {
        std::vector<std::uint32_t> kinds;
        kinds.reserve(function.captureSourceKinds.size());
        for (const MethodLambdaCaptureSourceKind kind : function.captureSourceKinds)
            kinds.push_back(static_cast<std::uint32_t>(kind));
        appendMirU32Vector(out, kinds);
    }
    {
        std::vector<std::uint32_t> modes;
        modes.reserve(function.captureModes.size());
        for (const language::CaptureMode mode : function.captureModes)
            modes.push_back(static_cast<std::uint32_t>(mode));
        appendMirU32Vector(out, modes);
    }
    appendMirU32(out, static_cast<std::uint32_t>(function.captureOwnsValues.size()));
    for (const bool owns : function.captureOwnsValues) out.push_back(owns ? '\x01' : '\x00');
    appendMirSizeVector(out, function.capturedLocalOrdinals);
    appendMirSizeVector(out, function.capturedParameterOrdinals);
    appendMirSizeVector(out, function.localOrdinals);
}

void appendMirFunctionCleanupRegions(std::string& out, const ProjectOopMirFunction& function)
{
    // Cleanup regions + exception flow (node pointers stored as pre-order indices).
    appendMirU32(out, static_cast<std::uint32_t>(function.cleanupRegions.size()));
    for (const ProjectOopMirCleanupRegion& region : function.cleanupRegions)
    {
        appendMirU64(out, static_cast<std::uint64_t>(region.ordinal));
        appendMirU32(out, static_cast<std::uint32_t>(region.kind));
        appendMirU64(out, static_cast<std::uint64_t>(
            nodePreorderIndex(function.body.get(), region.action)));
        out.push_back(region.localOrdinal.has_value() ? '\x01' : '\x00');
        if (region.localOrdinal.has_value()) appendMirU64(out, *region.localOrdinal);
        out.push_back(region.parameterOrdinal.has_value() ? '\x01' : '\x00');
        if (region.parameterOrdinal.has_value()) appendMirU64(out, *region.parameterOrdinal);
        appendMirString(out, region.typeKey);
        appendMirU32(out, static_cast<std::uint32_t>(region.ownership));
        out.push_back(region.automaticDestruction ? '\x01' : '\x00');
    }
}

void appendMirFunctionExceptionFlow(std::string& out, const ProjectOopMirFunction& function)
{
    out.push_back(function.exceptionFlow.requiresPersonality ? '\x01' : '\x00');
    appendMirSizeVector(out, function.exceptionFlow.protectedRegionOrdinals);
    appendMirU32(out, static_cast<std::uint32_t>(function.exceptionFlow.catchRegionCount));
    appendMirU32(out, static_cast<std::uint32_t>(function.exceptionFlow.finallyRegionCount));
    appendMirU32(out, static_cast<std::uint32_t>(function.exceptionFlow.nonlocalExits.size()));
    for (const ProjectOopMirNonlocalExit& exit : function.exceptionFlow.nonlocalExits)
    {
        appendMirU32(out, static_cast<std::uint32_t>(exit.kind));
        appendMirU64(out, static_cast<std::uint64_t>(
            nodePreorderIndex(function.body.get(), exit.node)));
        appendMirSizeVector(out, exit.cleanupRegionOrdinals);
        out.push_back(exit.targetScopeOrdinal.has_value() ? '\x01' : '\x00');
        if (exit.targetScopeOrdinal.has_value()) appendMirU64(out, *exit.targetScopeOrdinal);
        out.push_back(exit.exceptionTargetRegionOrdinal.has_value() ? '\x01' : '\x00');
        if (exit.exceptionTargetRegionOrdinal.has_value())
            appendMirU64(out, *exit.exceptionTargetRegionOrdinal);
        out.push_back(exit.cleanupContinuationKind.has_value() ? '\x01' : '\x00');
        if (exit.cleanupContinuationKind.has_value())
            appendMirU32(out, static_cast<std::uint32_t>(*exit.cleanupContinuationKind));
    }
}

[[maybe_unused]] std::string serializeMirFunctionCore(const ProjectOopMirFunction& function)
{
    std::string out;
    appendMirFunctionIdentity(out, function);
    appendMirFunctionFlags(out, function);
    appendMirFunctionAbi(out, function);
    appendMirFunctionCaptures(out, function);
    appendMirFunctionCleanupRegions(out, function);
    appendMirFunctionExceptionFlow(out, function);
    out += serializeMirNodeTree(function.body.get());
    return out;
}

bool readMirFunctionIdentity(
    internal::MirByteReader& reader,
    ProjectOopMirFunction& function)
{
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
    if (!reader.string(function.mangledName) ||
        !reader.string(function.genericSpecializationKey) ||
        !reader.string(function.returnTypeKey) || !reader.string(function.errorTypeKey) ||
        !reader.stringVector(function.parameterTypeKeys) ||
        !reader.stringVector(function.parameterNames) ||
        !reader.stringVector(function.captureTypeKeys) ||
        !reader.stringVector(function.captureNames) ||
        !reader.stringVector(function.generatorParameterTypeKeys) ||
        !reader.string(function.importDllName) || !reader.string(function.cAbiName) ||
        !reader.string(function.debugDisplayName) || !reader.u64(u64))
        return false;
    function.memberOrdinal = static_cast<std::size_t>(u64);
    if (!reader.u64(function.declaringTypeOrdinal) || !reader.u64(function.closureLayoutSize) ||
        !reader.u64(function.closureLayoutAlignment) || !reader.u64(function.asyncFrameSize) ||
        !reader.u32(u32))
        return false;
    function.inliningPolicy = static_cast<InliningPolicy>(u32);
    if (!reader.u32(u32)) return false;
    function.callableTrait = static_cast<language::CallableTrait>(u32);
    if (!reader.u32(u32)) return false;
    function.frameStorage = static_cast<language::FrameStorage>(u32);
    if (!reader.u32(u32)) return false;
    function.returnOwnership = static_cast<sema::OwnershipKind>(u32);
    std::uint8_t lambdaPresent = 0;
    if (!reader.byte(lambdaPresent)) return false;
    if (lambdaPresent)
    {
        if (!reader.u64(u64)) return false;
        function.lambdaOrdinal = static_cast<std::size_t>(u64);
    }
    else
    {
        function.lambdaOrdinal.reset();
    }
    return true;
}

bool readMirFunctionFlags(
    internal::MirByteReader& reader,
    ProjectOopMirFunction& function)
{
    std::uint32_t flags = 0;
    if (!reader.u32(flags)) return false;
    const auto flag = [&](std::uint32_t bit) { return (flags & (1U << bit)) != 0U; };
    function.constructor = flag(0);
    function.destructor = flag(1);
    function.isStatic = flag(2);
    function.referenceReturn = flag(3);
    function.isNoHeap = flag(4);
    function.isExportedC = flag(5);
    function.isImportedC = flag(6);
    function.isVariadic = flag(7);
    function.isImportedNative = flag(8);
    function.isSyntheticImplicitConstructor = flag(9);
    function.importRequiresRuntimeBinding = flag(10);
    function.isLambda = flag(11);
    function.hasClosureEnvironment = flag(12);
    function.capturesThisCell = flag(13);
    function.isGeneratorConstructor = flag(14);
    function.isTransportedInlineBody = flag(15);
    return true;
}

bool readMirFunctionEffects(
    internal::MirByteReader& reader,
    ProjectOopMirFunction& function)
{
    std::uint32_t effectBits = 0;
    if (!reader.u32(effectBits)) return false;
    function.effects = sema::EffectSet{};
    for (const sema::Effect effect : {sema::Effect::Unsafe, sema::Effect::Throws,
                                      sema::Effect::Async, sema::Effect::Comptime,
                                      sema::Effect::NoHeap, sema::Effect::Generator})
    {
        if ((effectBits & static_cast<std::uint32_t>(effect)) != 0U)
            function.effects = function.effects.with(effect);
    }
    return true;
}

bool readMirFunctionAbi(
    internal::MirByteReader& reader,
    ProjectOopMirFunction& function)
{
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
    if (!reader.u32(u32)) return false;
    function.abi.returnPassing = static_cast<ProjectOopMirAbiPassing>(u32);
    std::uint8_t hiddenPresent = 0;
    if (!reader.byte(hiddenPresent)) return false;
    if (hiddenPresent != 0U)
    {
        if (!reader.u64(u64)) return false;
        function.abi.hiddenReturnParameterOrdinal = static_cast<std::size_t>(u64);
    }
    else
    {
        function.abi.hiddenReturnParameterOrdinal.reset();
    }
    if (!reader.u32(u32)) return false;
    std::vector<std::uint32_t> passing;
    passing.reserve(u32);
    for (std::uint32_t index = 0; index < u32; ++index)
    {
        std::uint32_t value = 0;
        if (!reader.u32(value)) return false;
        passing.push_back(value);
    }
    function.abi.parameterPassing.clear();
    function.abi.parameterPassing.reserve(passing.size());
    for (const std::uint32_t value : passing)
        function.abi.parameterPassing.push_back(static_cast<ProjectOopMirAbiPassing>(value));
    if (!readMirSizeVector(reader, function.abi.parameterPhysicalOrdinals)) return false;
    if (!reader.u32(u32)) return false;
    function.abi.physicalParameterCount = u32;
    return true;
}

template <typename Tuples>
bool readMirAsyncTuples(internal::MirByteReader& reader, Tuples& tuples)
{
    std::uint32_t count = 0;
    if (!reader.u32(count)) return false;
    tuples.clear();
    tuples.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::uint64_t ordinalValue = 0;
        std::uint64_t offset = 0;
        std::string typeKey;
        if (!reader.u64(ordinalValue)) return false;
        const std::size_t ordinal = static_cast<std::size_t>(ordinalValue);
        if (!reader.u64(offset) || !reader.string(typeKey)) return false;
        tuples.emplace_back(ordinal, offset, std::move(typeKey));
    }
    return true;
}

bool readMirFunctionCaptures(
    internal::MirByteReader& reader,
    ProjectOopMirFunction& function)
{
    if (!readMirAsyncTuples(reader, function.asyncFrameLocals) ||
        !readMirAsyncTuples(reader, function.asyncFrameParameters))
        return false;
    std::uint32_t count = 0;
    if (!reader.u32(count)) return false;
    function.captureSourceKinds.clear();
    function.captureSourceKinds.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::uint32_t kind = 0;
        if (!reader.u32(kind)) return false;
        function.captureSourceKinds.push_back(static_cast<MethodLambdaCaptureSourceKind>(kind));
    }
    if (!reader.u32(count)) return false;
    function.captureModes.clear();
    function.captureModes.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::uint32_t mode = 0;
        if (!reader.u32(mode)) return false;
        function.captureModes.push_back(static_cast<language::CaptureMode>(mode));
    }
    if (!reader.u32(count)) return false;
    function.captureOwnsValues.clear();
    function.captureOwnsValues.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::uint8_t owns = 0;
        if (!reader.byte(owns)) return false;
        function.captureOwnsValues.push_back(owns != 0U);
    }
    return readMirSizeVector(reader, function.capturedLocalOrdinals) &&
        readMirSizeVector(reader, function.capturedParameterOrdinals) &&
        readMirSizeVector(reader, function.localOrdinals);
}

bool readMirCleanupRegion(
    internal::MirByteReader& reader,
    ProjectOopMirCleanupRegion& region,
    std::size_t& actionIndex)
{
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
    if (!reader.u64(u64)) return false;
    region.ordinal = static_cast<std::size_t>(u64);
    if (!reader.u32(u32)) return false;
    region.kind = static_cast<ProjectOopMirCleanupKind>(u32);
    if (!reader.u64(u64)) return false;
    actionIndex = static_cast<std::size_t>(u64);
    std::uint8_t present = 0;
    if (!reader.byte(present)) return false;
    if (present != 0U)
    {
        if (!reader.u64(u64)) return false;
        region.localOrdinal = static_cast<std::size_t>(u64);
    }
    if (!reader.byte(present)) return false;
    if (present != 0U)
    {
        if (!reader.u64(u64)) return false;
        region.parameterOrdinal = static_cast<std::size_t>(u64);
    }
    if (!reader.string(region.typeKey)) return false;
    if (!reader.u32(u32)) return false;
    region.ownership = static_cast<sema::OwnershipKind>(u32);
    if (!reader.byte(present)) return false;
    region.automaticDestruction = present != 0U;
    return true;
}

bool readMirFunctionCleanupRegions(
    internal::MirByteReader& reader,
    ProjectOopMirFunction& function,
    std::vector<std::size_t>& regionActionIndices)
{
    // Cleanup regions + exception flow (node pointers as pre-order indices, re-linked after body).
    std::uint32_t count = 0;
    if (!reader.u32(count)) return false;
    function.cleanupRegions.clear();
    function.cleanupRegions.reserve(count);
    regionActionIndices.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        ProjectOopMirCleanupRegion region;
        std::size_t actionIndex = 0;
        if (!readMirCleanupRegion(reader, region, actionIndex)) return false;
        regionActionIndices.push_back(actionIndex);
        function.cleanupRegions.push_back(std::move(region));
    }
    return true;
}

bool readMirNonlocalExit(
    internal::MirByteReader& reader,
    ProjectOopMirNonlocalExit& exit,
    std::size_t& nodeIndex)
{
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
    if (!reader.u32(u32)) return false;
    exit.kind = static_cast<ProjectOopMirNonlocalExitKind>(u32);
    if (!reader.u64(u64)) return false;
    nodeIndex = static_cast<std::size_t>(u64);
    if (!readMirSizeVector(reader, exit.cleanupRegionOrdinals)) return false;
    std::uint8_t present = 0;
    if (!reader.byte(present)) return false;
    if (present != 0U)
    {
        if (!reader.u64(u64)) return false;
        exit.targetScopeOrdinal = static_cast<std::size_t>(u64);
    }
    if (!reader.byte(present)) return false;
    if (present != 0U)
    {
        if (!reader.u64(u64)) return false;
        exit.exceptionTargetRegionOrdinal = static_cast<std::size_t>(u64);
    }
    if (!reader.byte(present)) return false;
    if (present != 0U)
    {
        if (!reader.u32(u32)) return false;
        exit.cleanupContinuationKind = static_cast<MethodCleanupContinuationKind>(u32);
    }
    return true;
}

bool readMirFunctionExceptionFlow(
    internal::MirByteReader& reader,
    ProjectOopMirFunction& function,
    std::vector<std::size_t>& exitNodeIndices)
{
    std::uint8_t requiresPersonality = 0;
    if (!reader.byte(requiresPersonality)) return false;
    function.exceptionFlow.requiresPersonality = requiresPersonality != 0U;
    if (!readMirSizeVector(reader, function.exceptionFlow.protectedRegionOrdinals)) return false;
    std::uint32_t u32 = 0;
    if (!reader.u32(u32)) return false;
    function.exceptionFlow.catchRegionCount = u32;
    if (!reader.u32(u32)) return false;
    function.exceptionFlow.finallyRegionCount = u32;
    if (!reader.u32(u32)) return false;
    function.exceptionFlow.nonlocalExits.clear();
    function.exceptionFlow.nonlocalExits.reserve(u32);
    exitNodeIndices.reserve(u32);
    for (std::uint32_t index = 0; index < u32; ++index)
    {
        ProjectOopMirNonlocalExit exit;
        std::size_t nodeIndex = 0;
        if (!readMirNonlocalExit(reader, exit, nodeIndex)) return false;
        exitNodeIndices.push_back(nodeIndex);
        function.exceptionFlow.nonlocalExits.push_back(std::move(exit));
    }
    return true;
}

[[maybe_unused]] bool restoreMirFunctionCore(
    std::string_view bytes, ProjectOopMirFunction& function)
{
    internal::MirByteReader reader(bytes);
    if (!readMirFunctionIdentity(reader, function) ||
        !readMirFunctionFlags(reader, function) ||
        !readMirFunctionEffects(reader, function) ||
        !readMirFunctionAbi(reader, function) ||
        !readMirFunctionCaptures(reader, function))
        return false;

    std::vector<std::size_t> regionActionIndices;
    std::vector<std::size_t> exitNodeIndices;
    if (!readMirFunctionCleanupRegions(reader, function, regionActionIndices) ||
        !readMirFunctionExceptionFlow(reader, function, exitNodeIndices))
        return false;
    if (!restoreMirNodeTree(reader.remaining(), function.body)) return false;
    for (std::size_t i = 0; i < function.cleanupRegions.size(); ++i)
        function.cleanupRegions[i].action =
            nodeByPreorderIndex(function.body.get(), regionActionIndices[i]);
    for (std::size_t i = 0; i < function.exceptionFlow.nonlocalExits.size(); ++i)
        function.exceptionFlow.nonlocalExits[i].node =
            nodeByPreorderIndex(function.body.get(), exitNodeIndices[i]);
    return true;
}

// NSC-LIMIT-009. Solo se llama sobre claves de identidad, de modo que delega en el codificador
// que conserva el `+`. Antes aplastaba `+` y `.` al mismo `_` y el descriptor, la vtable y el mapa
// de interfaces de `N.Outer+Item` chocaban con los de `N.Outer.Item`.
}
// NSC305-C04. MIR cache consumer: persist/validate the built functions by implementation fingerprint.
void ProjectOopMirModel::setMirCacheRoot(std::filesystem::path root)
{
    mirCache_.setRoot(std::move(root));
}

void ProjectOopMirModel::storeMirFunctions()
{
    if (!mirCache_.enabled()) return;
    for (const ProjectOopMirFunction& function : functions_)
    {
        const std::string implementationFingerprint =
            mirImplementationFingerprint(function.body.get());
        const std::string payload = serializeMirFunctionCore(function);
        std::string error;
        (void)mirCache_.store(implementationFingerprint, payload, error);
    }
}

void ProjectOopMirModel::restoreMirFunctions()
{
    if (!mirCache_.enabled()) return;
    for (const ProjectOopMirFunction& function : functions_)
    {
        const std::string implementationFingerprint =
            mirImplementationFingerprint(function.body.get());
        std::string payload;
        bool hit = false;
        std::string error;
        if (!mirCache_.restore(implementationFingerprint, payload, hit, error) || !hit) continue;
        ProjectOopMirFunction restored;
        if (restoreMirFunctionCore(payload, restored))
        {
            // The payload round-trips; the caller decides whether to swap the built function.
            (void)restored;
        }
    }
}

std::string ProjectOopMirModel::serializeMirNodeTreeForTesting(
    const ProjectOopMirNode* root)
{
    return serializeMirNodeTree(root);
}

bool ProjectOopMirModel::restoreMirNodeTreeForTesting(
    std::string_view bytes,
    std::unique_ptr<ProjectOopMirNode>& root)
{
    return restoreMirNodeTree(bytes, root);
}

}
