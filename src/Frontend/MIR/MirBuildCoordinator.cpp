#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include "MirConstantEvaluator.h"

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

class MirByteReader final
{
public:
    explicit MirByteReader(std::string_view bytes) : bytes_(bytes) {}

    bool u32(std::uint32_t& out)
    {
        out = 0;
        for (unsigned shift = 0; shift < 32U; shift += 8U)
        {
            std::uint8_t value = 0;
            if (!byte(value)) return false;
            out |= static_cast<std::uint32_t>(value) << shift;
        }
        return true;
    }
    bool u64(std::uint64_t& out)
    {
        out = 0;
        for (unsigned shift = 0; shift < 64U; shift += 8U)
        {
            std::uint8_t value = 0;
            if (!byte(value)) return false;
            out |= static_cast<std::uint64_t>(value) << shift;
        }
        return true;
    }
    bool string(std::string& out)
    {
        std::uint32_t size = 0;
        if (!u32(size)) return false;
        if (position_ + size > bytes_.size()) return false;
        out.assign(bytes_.data() + position_, size);
        position_ += size;
        return true;
    }
    bool stringVector(std::vector<std::string>& out)
    {
        std::uint32_t count = 0;
        if (!u32(count)) return false;
        out.clear();
        out.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            std::string value;
            if (!string(value)) return false;
            out.push_back(std::move(value));
        }
        return true;
    }

    bool byte(std::uint8_t& out)
    {
        if (position_ >= bytes_.size()) return false;
        out = static_cast<std::uint8_t>(bytes_[position_++]);
        return true;
    }
    std::string_view remaining() const noexcept { return bytes_.substr(position_); }

private:
    std::string_view bytes_;
    std::size_t position_ = 0;
};

// Serializes one node's scalar/string/flag fields (children recurse iteratively below).
void appendMirNodeFields(std::string& out, const ProjectOopMirNode& node)
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
    const std::uint64_t floatingBits = [&]()
    {
        std::uint64_t bits = 0;
        static_assert(sizeof(double) == sizeof(std::uint64_t));
        std::memcpy(&bits, &node.floatingValue, sizeof(bits));
        return bits;
    }();
    appendMirU64(out, floatingBits);
    appendMirU64(out, static_cast<std::uint64_t>(node.trailingLengthRelativeOffset));
    appendMirU32(out, node.variableLengthArrayRank ? static_cast<std::uint32_t>(node.variableLengthArrayRank) : 0U);
    appendMirU32(out, node.nativeArrayRank ? static_cast<std::uint32_t>(node.nativeArrayRank) : 0U);
    appendMirU32(out, static_cast<std::uint32_t>(node.argumentCount));
    appendMirStringVector(out, node.argumentTypeKeys);
    appendMirStringVector(out, node.catchMatchedErrorTypeKeys);
    appendMirU32(out, static_cast<std::uint32_t>(node.ownership));
    appendMirU32(out, static_cast<std::uint32_t>(node.objectLifetimeOwnership));
    appendMirU32(out, static_cast<std::uint32_t>(node.storageOwnership));
    appendMirU32(out, static_cast<std::uint32_t>(node.captureMode));
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

    // Optional ordinals / enum optionals (present byte + value).
    const auto appendOptU64 = [&](const std::optional<std::size_t>& value)
    {
        out.push_back(value.has_value() ? '\x01' : '\x00');
        if (value.has_value()) appendMirU64(out, static_cast<std::uint64_t>(*value));
    };
    const auto appendOptEnum = [&](const auto& value)
    {
        out.push_back(value.has_value() ? '\x01' : '\x00');
        if (value.has_value())
            appendMirU32(out, static_cast<std::uint32_t>(*value));
    };
    appendOptU64(node.memberOrdinal);
    appendOptU64(node.namespaceFunctionOrdinal);
    appendOptU64(node.namespaceStateOrdinal);
    appendOptU64(node.secondaryMemberOrdinal);
    appendOptU64(node.resultSuccessTestMemberOrdinal);
    appendOptU64(node.resultSuccessExtractorMemberOrdinal);
    appendOptU64(node.resultErrorExtractorMemberOrdinal);
    appendOptU64(node.resultErrorFactoryMemberOrdinal);
    appendOptU64(node.resultErrorConversionMemberOrdinal);
    appendOptU64(node.localOrdinal);
    appendOptU64(node.parameterOrdinal);
    appendOptU64(node.constructionOrdinal);
    appendOptU64(node.targetTypeOrdinal);
    appendOptU64(node.exceptionTypeOrdinal);
    appendOptU64(node.cleanupRegionOrdinal);
    appendOptU64(node.mirRegionOrdinal);
    appendOptU64(node.exceptionTargetRegionOrdinal);
    appendOptU64(node.targetScopeOrdinal);
    appendOptU64(node.disposeContractMemberOrdinal);
    appendOptU64(node.disposeImplementationMemberOrdinal);
    appendOptU64(node.virtualSlot);
    appendOptU64(node.interfaceSlot);
    appendOptU64(node.allocatorContextLocalOrdinal);
    appendOptU64(node.allocatorDeallocateMemberOrdinal);
    appendOptU64(node.allocatorTypeOrdinal);
    appendOptEnum(node.cleanupContinuationKind);
    appendOptEnum(node.borrowKind);
    appendOptEnum(node.localBorrowSourceKind);
    appendOptEnum(node.conversionKind);
    appendOptU64(node.bitOffset);
    appendOptU64(node.bitWidth);

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
        appendMirU32(out, 0xFFFFFFFFu); // node marker
        appendMirNodeFields(out, *node);
        appendMirU32(out, static_cast<std::uint32_t>(node->children.size()));
        for (auto it = node->children.rbegin(); it != node->children.rend(); ++it)
            work.push_back(it->get());
    }
    appendMirU32(out, 0xFFFFFFFEu); // end marker
    return out;
}

// Reads one node's scalar/string/flag/optional/vector fields (symmetric to appendMirNodeFields).
bool readMirNodeFields(MirByteReader& reader, ProjectOopMirNode& node)
{
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
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
    if (!reader.u32(u32)) return false;
    const auto flag = [&](std::uint32_t bit) { return (u32 & (1U << bit)) != 0U; };
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

    const auto readOptU64 = [&](std::optional<std::size_t>& value) -> bool
    {
        std::uint8_t present = 0;
        if (!reader.byte(present)) return false;
        if (!present) { value.reset(); return true; }
        if (!reader.u64(u64)) return false;
        value = static_cast<std::size_t>(u64);
        return true;
    };
    const auto readOptEnum = [&](auto& value) -> bool
    {
        std::uint8_t present = 0;
        if (!reader.byte(present)) return false;
        if (!present) { value.reset(); return true; }
        if (!reader.u32(u32)) return false;
        value = static_cast<std::remove_reference_t<decltype(*value)>>(u32);
        return true;
    };
    if (!readOptU64(node.memberOrdinal) || !readOptU64(node.namespaceFunctionOrdinal) ||
        !readOptU64(node.namespaceStateOrdinal) || !readOptU64(node.secondaryMemberOrdinal) ||
        !readOptU64(node.resultSuccessTestMemberOrdinal) ||
        !readOptU64(node.resultSuccessExtractorMemberOrdinal) ||
        !readOptU64(node.resultErrorExtractorMemberOrdinal) ||
        !readOptU64(node.resultErrorFactoryMemberOrdinal) ||
        !readOptU64(node.resultErrorConversionMemberOrdinal) || !readOptU64(node.localOrdinal) ||
        !readOptU64(node.parameterOrdinal) || !readOptU64(node.constructionOrdinal) ||
        !readOptU64(node.targetTypeOrdinal) || !readOptU64(node.exceptionTypeOrdinal) ||
        !readOptU64(node.cleanupRegionOrdinal) || !readOptU64(node.mirRegionOrdinal) ||
        !readOptU64(node.exceptionTargetRegionOrdinal) || !readOptU64(node.targetScopeOrdinal) ||
        !readOptU64(node.disposeContractMemberOrdinal) ||
        !readOptU64(node.disposeImplementationMemberOrdinal) || !readOptU64(node.virtualSlot) ||
        !readOptU64(node.interfaceSlot) || !readOptU64(node.allocatorContextLocalOrdinal) ||
        !readOptU64(node.allocatorDeallocateMemberOrdinal) || !readOptU64(node.allocatorTypeOrdinal) ||
        !readOptEnum(node.cleanupContinuationKind) || !readOptEnum(node.borrowKind) ||
        !readOptEnum(node.localBorrowSourceKind) || !readOptEnum(node.conversionKind) ||
        !readOptU64(node.bitOffset) || !readOptU64(node.bitWidth))
        return false;

    const auto readSizeVector = [&](std::vector<std::size_t>& values) -> bool
    {
        if (!reader.u32(u32)) return false;
        values.clear();
        values.reserve(u32);
        for (std::uint32_t index = 0; index < u32; ++index)
        {
            if (!reader.u64(u64)) return false;
            values.push_back(static_cast<std::size_t>(u64));
        }
        return true;
    };
    const auto readInt64Vector = [&](std::vector<std::int64_t>& values) -> bool
    {
        if (!reader.u32(u32)) return false;
        values.clear();
        values.reserve(u32);
        for (std::uint32_t index = 0; index < u32; ++index)
        {
            if (!reader.u64(u64)) return false;
            values.push_back(static_cast<std::int64_t>(u64));
        }
        return true;
    };
    if (!readSizeVector(node.cleanupRegionOrdinals) || !readInt64Vector(node.switchLabels))
        return false;
    {
        if (!reader.u32(u32)) return false;
        node.collectionSegments.clear();
        node.collectionSegments.reserve(u32);
        for (std::uint32_t index = 0; index < u32; ++index)
        {
            std::uint32_t segment = 0;
            if (!reader.u32(segment)) return false;
            node.collectionSegments.push_back(static_cast<ProjectCollectionSegmentKind>(segment));
        }
    }
    return readSizeVector(node.argumentParameterOrdinals);
}

bool readMirNode(MirByteReader& reader, std::unique_ptr<ProjectOopMirNode>& out)
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
    for (std::unique_ptr<ProjectOopMirNode>& child : node->children)
    {
        if (!readMirNode(reader, child)) return false;
    }
    out = std::move(node);
    return true;
}

[[maybe_unused]] bool restoreMirNodeTree(
    std::string_view bytes, std::unique_ptr<ProjectOopMirNode>& body)
{
    MirByteReader reader(bytes);
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
[[maybe_unused]] std::string serializeMirFunctionCore(const ProjectOopMirFunction& function)
{
    std::string out;
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
    const auto appendAsyncTuples = [&](const auto& tuples)
    {
        appendMirU32(out, static_cast<std::uint32_t>(tuples.size()));
        for (const auto& [ordinal, offset, typeKey] : tuples)
        {
            appendMirU64(out, static_cast<std::uint64_t>(ordinal));
            appendMirU64(out, offset);
            appendMirString(out, typeKey);
        }
    };
    appendAsyncTuples(function.asyncFrameLocals);
    appendAsyncTuples(function.asyncFrameParameters);
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
    out += serializeMirNodeTree(function.body.get());
    return out;
}

[[maybe_unused]] bool restoreMirFunctionCore(
    std::string_view bytes, ProjectOopMirFunction& function)
{
    MirByteReader reader(bytes);
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
    if (!reader.u32(u32)) return false;
    const auto flag = [&](std::uint32_t bit) { return (u32 & (1U << bit)) != 0U; };
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
    {
        if (!reader.u32(u32)) return false;
        std::vector<std::uint32_t> passing;
        passing.reserve(u32);
        for (std::uint32_t index = 0; index < u32; ++index)
        {
            std::uint32_t p = 0;
            if (!reader.u32(p)) return false;
            passing.push_back(p);
        }
        function.abi.parameterPassing.clear();
        function.abi.parameterPassing.reserve(passing.size());
        for (const std::uint32_t p : passing)
            function.abi.parameterPassing.push_back(static_cast<ProjectOopMirAbiPassing>(p));
    }
    const auto readSizeVector = [&](std::vector<std::size_t>& values) -> bool
    {
        if (!reader.u32(u32)) return false;
        values.clear();
        values.reserve(u32);
        for (std::uint32_t index = 0; index < u32; ++index)
        {
            if (!reader.u64(u64)) return false;
            values.push_back(static_cast<std::size_t>(u64));
        }
        return true;
    };
    if (!readSizeVector(function.abi.parameterPhysicalOrdinals)) return false;
    if (!reader.u32(u32)) return false;
    function.abi.physicalParameterCount = u32;
    const auto readAsyncTuples = [&](auto& tuples) -> bool
    {
        if (!reader.u32(u32)) return false;
        tuples.clear();
        tuples.reserve(u32);
        for (std::uint32_t index = 0; index < u32; ++index)
        {
            std::size_t ordinal = 0;
            std::uint64_t offset = 0;
            std::string typeKey;
            if (!reader.u64(u64)) return false;
            ordinal = static_cast<std::size_t>(u64);
            if (!reader.u64(offset) || !reader.string(typeKey)) return false;
            tuples.emplace_back(ordinal, offset, std::move(typeKey));
        }
        return true;
    };
    if (!readAsyncTuples(function.asyncFrameLocals) ||
        !readAsyncTuples(function.asyncFrameParameters))
        return false;
    {
        if (!reader.u32(u32)) return false;
        function.captureSourceKinds.clear();
        function.captureSourceKinds.reserve(u32);
        for (std::uint32_t index = 0; index < u32; ++index)
        {
            std::uint32_t kind = 0;
            if (!reader.u32(kind)) return false;
            function.captureSourceKinds.push_back(static_cast<MethodLambdaCaptureSourceKind>(kind));
        }
    }
    {
        if (!reader.u32(u32)) return false;
        function.captureModes.clear();
        function.captureModes.reserve(u32);
        for (std::uint32_t index = 0; index < u32; ++index)
        {
            std::uint32_t mode = 0;
            if (!reader.u32(mode)) return false;
            function.captureModes.push_back(static_cast<language::CaptureMode>(mode));
        }
    }
    {
        if (!reader.u32(u32)) return false;
        function.captureOwnsValues.clear();
        function.captureOwnsValues.reserve(u32);
        for (std::uint32_t index = 0; index < u32; ++index)
        {
            std::uint8_t owns = 0;
            if (!reader.byte(owns)) return false;
            function.captureOwnsValues.push_back(owns != 0U);
        }
    }
    if (!readSizeVector(function.capturedLocalOrdinals) ||
        !readSizeVector(function.capturedParameterOrdinals) ||
        !readSizeVector(function.localOrdinals))
        return false;
    // Cleanup regions + exception flow (node pointers as pre-order indices, re-linked after body).
    std::vector<std::size_t> regionActionIndices;
    std::vector<std::size_t> exitNodeIndices;
    if (!reader.u32(u32)) return false;
    function.cleanupRegions.clear();
    function.cleanupRegions.reserve(u32);
    regionActionIndices.reserve(u32);
    for (std::uint32_t index = 0; index < u32; ++index)
    {
        ProjectOopMirCleanupRegion region;
        if (!reader.u64(u64)) return false;
        region.ordinal = static_cast<std::size_t>(u64);
        if (!reader.u32(u32)) return false;
        region.kind = static_cast<ProjectOopMirCleanupKind>(u32);
        if (!reader.u64(u64)) return false;
        regionActionIndices.push_back(static_cast<std::size_t>(u64));
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
        function.cleanupRegions.push_back(std::move(region));
    }
    std::uint8_t requiresPersonality = 0;
    if (!reader.byte(requiresPersonality)) return false;
    function.exceptionFlow.requiresPersonality = requiresPersonality != 0U;
    if (!readSizeVector(function.exceptionFlow.protectedRegionOrdinals)) return false;
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
        if (!reader.u32(u32)) return false;
        exit.kind = static_cast<ProjectOopMirNonlocalExitKind>(u32);
        if (!reader.u64(u64)) return false;
        exitNodeIndices.push_back(static_cast<std::size_t>(u64));
        if (!readSizeVector(exit.cleanupRegionOrdinals)) return false;
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
        function.exceptionFlow.nonlocalExits.push_back(std::move(exit));
    }
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
std::string sanitizeDispatchName(std::string_view value)
{
    return compilation::symbolFragmentForIdentityKey(value);
}

std::uint64_t stableTypeId(std::string_view value) noexcept
{
    // FNV-1a 64-bit. This is an ABI-visible deterministic identity seed, not reflection.
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : value)
    {
        const auto byte = static_cast<unsigned char>(character);
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string genericSpecializationKeyForType(
    const ProjectNameLookup& nameLookup,
    std::size_t typeOrdinal)
{
    if (typeOrdinal >= nameLookup.namedTypes().size()) return {};
    constexpr std::string_view kSpecializationPrefix = "stage44-specialization:";
    const NamedTypeDeclaration& type = nameLookup.namedTypes()[typeOrdinal];
    const auto keyForSyntax = [&](const ast::AstNode* syntax) -> std::string {
        if (syntax == nullptr || !syntax->auxiliaryText().starts_with(kSpecializationPrefix))
            return {};
        std::string key = syntax->auxiliaryText().substr(kSpecializationPrefix.size());
        if (const std::size_t metadata = key.find(";nsc-meta:"); metadata != std::string::npos)
            key.resize(metadata);
        return key;
    };
    if (std::string key = keyForSyntax(type.syntax); !key.empty()) return key;
    for (const NamedTypeDeclarationPart& part : type.parts)
    {
        if (std::string key = keyForSyntax(part.syntax); !key.empty()) return key;
    }
    // Name aggregation can select the open template as the declaration's primary syntax even
    // though this ordinal names the generated closed type.  The generated name is itself a
    // stable compiler contract: __g<arity>__<arguments>__h<16 hex>.  Treat it as reusable when
    // the AST marker is no longer reachable from the aggregate declaration.
    const std::string& name = type.identity.fullyQualifiedName();
    const std::size_t generic = name.rfind("__g");
    const std::size_t hash = name.rfind("__h");
    if (generic != std::string::npos && hash != std::string::npos && hash > generic + 3U &&
        hash + 3U + 16U == name.size())
    {
        bool validHash = true;
        for (const char character : std::string_view(name).substr(hash + 3U))
            validHash = validHash && std::isxdigit(static_cast<unsigned char>(character));
        if (validHash) return "type:" + type.identity.key();
    }
    return {};
}

}
const char* projectOopMirAbiPassingName(ProjectOopMirAbiPassing passing) noexcept
{
    return passing == ProjectOopMirAbiPassing::Indirect ? "win64-indirect" : "win64-direct";
}

const char* projectOopMirCleanupKindName(ProjectOopMirCleanupKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirCleanupKind::LocalOwner: return "local-owner";
        case ProjectOopMirCleanupKind::TemporaryOwner: return "temporary-owner";
        case ProjectOopMirCleanupKind::OwnedParameter: return "owned-parameter";
        case ProjectOopMirCleanupKind::Using: return "using";
        case ProjectOopMirCleanupKind::Finally: return "finally";
    }
    return "unknown";
}

const char* projectOopMirNonlocalExitKindName(ProjectOopMirNonlocalExitKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNonlocalExitKind::Return: return "return";
        case ProjectOopMirNonlocalExitKind::Break: return "break";
        case ProjectOopMirNonlocalExitKind::Continue: return "continue";
        case ProjectOopMirNonlocalExitKind::Throw: return "throw";
        case ProjectOopMirNonlocalExitKind::Rethrow: return "rethrow";
        case ProjectOopMirNonlocalExitKind::Panic: return "panic";
        case ProjectOopMirNonlocalExitKind::CleanupContinuation: return "cleanup-continuation";
    }
    return "unknown";
}

const char* projectOopMirNodeKindName(ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::Block: return "block";
        case ProjectOopMirNodeKind::BlockExpression: return "block-expression";
        case ProjectOopMirNodeKind::LocalDeclaration: return "local-declaration";
        case ProjectOopMirNodeKind::AggregateInitialize: return "aggregate-initialize";
        case ProjectOopMirNodeKind::AggregateSpreadStore: return "aggregate-spread-store";
        case ProjectOopMirNodeKind::AggregateElementStore: return "aggregate-element-store";
        case ProjectOopMirNodeKind::TupleDeconstruct: return "tuple-deconstruct";
        case ProjectOopMirNodeKind::TupleDeconstructElementStore:
            return "tuple-deconstruct-element-store";
        case ProjectOopMirNodeKind::ParameterPack: return "parameter-pack";
        case ProjectOopMirNodeKind::ExpressionStatement: return "expression-statement";
        case ProjectOopMirNodeKind::InlineAssembly: return "inline-assembly";
        case ProjectOopMirNodeKind::InlineAssemblyLine: return "inline-assembly-line";
        case ProjectOopMirNodeKind::InlineAssemblyInput: return "inline-assembly-input";
        case ProjectOopMirNodeKind::InlineAssemblyOutput: return "inline-assembly-output";
        case ProjectOopMirNodeKind::InlineAssemblyClobber: return "inline-assembly-clobber";
        case ProjectOopMirNodeKind::If: return "if";
        case ProjectOopMirNodeKind::While: return "while";
        case ProjectOopMirNodeKind::DoWhile: return "do-while";
        case ProjectOopMirNodeKind::For: return "for";
        case ProjectOopMirNodeKind::Switch: return "switch";
        case ProjectOopMirNodeKind::SwitchSection: return "switch-section";
        case ProjectOopMirNodeKind::Break: return "break";
        case ProjectOopMirNodeKind::Continue: return "continue";
        case ProjectOopMirNodeKind::Return: return "return";
        case ProjectOopMirNodeKind::TryRegion: return "try-region";
        case ProjectOopMirNodeKind::CatchRegion: return "catch-region";
        case ProjectOopMirNodeKind::FinallyRegion: return "finally-region";
        case ProjectOopMirNodeKind::UsingRegion: return "using-region";
        case ProjectOopMirNodeKind::Throw: return "throw";
        case ProjectOopMirNodeKind::Rethrow: return "rethrow";
        case ProjectOopMirNodeKind::CleanupContinuation: return "cleanup-continuation";
        case ProjectOopMirNodeKind::OwnedParameterCleanup: return "owned-parameter-cleanup";
        case ProjectOopMirNodeKind::MoveValue: return "move-value";
        case ProjectOopMirNodeKind::MoveArrayElement: return "move-array-element";
        case ProjectOopMirNodeKind::MoveOwnedField: return "move-owned-field";
        case ProjectOopMirNodeKind::MoveIteratorCurrent: return "move-iterator-current";
        case ProjectOopMirNodeKind::OwnedTemporaryBorrow: return "owned-temporary-borrow";
        case ProjectOopMirNodeKind::FullExpressionCleanup: return "full-expression-cleanup";
        case ProjectOopMirNodeKind::PointerLoad: return "pointer-load";
        case ProjectOopMirNodeKind::RawElementAddress: return "raw-element-address";
        case ProjectOopMirNodeKind::StackAllocation: return "stack-allocation";
        case ProjectOopMirNodeKind::RawAllocation: return "raw-allocation";
        case ProjectOopMirNodeKind::PointerArithmetic: return "pointer-arithmetic";
        case ProjectOopMirNodeKind::CompoundStore: return "compound-store";
        case ProjectOopMirNodeKind::ValueConstruct: return "value-construct";
        case ProjectOopMirNodeKind::PlacementConstruct: return "placement-construct";
        case ProjectOopMirNodeKind::TemporaryAddress: return "temporary-address";
        case ProjectOopMirNodeKind::AddressValue: return "address-value";
        case ProjectOopMirNodeKind::BorrowValue: return "borrow-value";
        case ProjectOopMirNodeKind::ThisValue: return "this";
        case ProjectOopMirNodeKind::ParameterValue: return "parameter-value";
        case ProjectOopMirNodeKind::LocalValue: return "local-value";
        case ProjectOopMirNodeKind::FieldLoad: return "field-load";
        case ProjectOopMirNodeKind::NullConditionalFieldLoad: return "null-conditional-field-load";
        case ProjectOopMirNodeKind::NullConditionalFieldStore: return "null-conditional-field-store";
        case ProjectOopMirNodeKind::NullConditionalArrayLoad: return "null-conditional-array-load";
        case ProjectOopMirNodeKind::NullConditionalArrayStore: return "null-conditional-array-store";
        case ProjectOopMirNodeKind::BitFieldLoad: return "bit-field-load";
        case ProjectOopMirNodeKind::StaticFieldLoad: return "static-field-load";
        case ProjectOopMirNodeKind::NamespaceStateLoad: return "namespace-state-load";
        case ProjectOopMirNodeKind::ParameterAddress: return "parameter-address";
        case ProjectOopMirNodeKind::LocalAddress: return "local-address";
        case ProjectOopMirNodeKind::FieldAddress: return "field-address";
        case ProjectOopMirNodeKind::BitFieldAddress: return "bit-field-address";
        case ProjectOopMirNodeKind::FutureFieldAddress: return "future-field-address";
        case ProjectOopMirNodeKind::AwaitFuture: return "await-future";
        case ProjectOopMirNodeKind::SuspendPoint: return "suspend-point";
        case ProjectOopMirNodeKind::PollAddress: return "poll-address";
        case ProjectOopMirNodeKind::YieldValue: return "yield-value";
        case ProjectOopMirNodeKind::YieldBreak: return "yield-break";
        case ProjectOopMirNodeKind::GeneratorFrameInit: return "generator-frame-init";
        case ProjectOopMirNodeKind::IteratorCurrent: return "iterator-current";
        case ProjectOopMirNodeKind::IteratorMoveNext: return "iterator-move-next";
        case ProjectOopMirNodeKind::IteratorGetEnumerator: return "iterator-get-enumerator";
        case ProjectOopMirNodeKind::IteratorDrop: return "iterator-drop";
        case ProjectOopMirNodeKind::IteratorMoveNextAsync: return "iterator-move-next-async";
        case ProjectOopMirNodeKind::IteratorCancel: return "iterator-cancel";
        case ProjectOopMirNodeKind::AsyncYieldValue: return "async-yield-value";
        case ProjectOopMirNodeKind::DropFrame: return "drop-frame";
        case ProjectOopMirNodeKind::StaticFieldAddress: return "static-field-address";
        case ProjectOopMirNodeKind::NamespaceStateAddress: return "namespace-state-address";
        case ProjectOopMirNodeKind::IntegerConstant: return "integer-constant";
        case ProjectOopMirNodeKind::FloatingConstant: return "floating-constant";
        case ProjectOopMirNodeKind::BooleanConstant: return "boolean-constant";
        case ProjectOopMirNodeKind::NullConstant: return "null-constant";
        case ProjectOopMirNodeKind::DefaultValue: return "default-value";
        case ProjectOopMirNodeKind::StringLiteral: return "string-literal";
        case ProjectOopMirNodeKind::Utf8StringLiteral: return "utf8-string-literal";
        case ProjectOopMirNodeKind::ArrayAllocate: return "array-allocate";
        case ProjectOopMirNodeKind::RangeArrayAllocate: return "range-array-allocate";
        case ProjectOopMirNodeKind::ArrayLength: return "array-length";
        case ProjectOopMirNodeKind::ArrayElementLoad: return "array-element-load";
        case ProjectOopMirNodeKind::ArrayElementAddress: return "array-element-address";
        case ProjectOopMirNodeKind::RangeArrayElementLoad: return "range-array-element-load";
        case ProjectOopMirNodeKind::RangeArrayElementAddress: return "range-array-element-address";
        case ProjectOopMirNodeKind::RangeSlice: return "range-slice";
        case ProjectOopMirNodeKind::StringLength: return "string-length";
        case ProjectOopMirNodeKind::StringElementLoad: return "string-element-load";
        case ProjectOopMirNodeKind::EnumName: return "enum-name";
        case ProjectOopMirNodeKind::StringConcat: return "string-concat";
        case ProjectOopMirNodeKind::StringEquals: return "string-equals";
        case ProjectOopMirNodeKind::StringSubstring: return "string-substring";
        case ProjectOopMirNodeKind::StringCharacterPointer: return "string-character-pointer";
        case ProjectOopMirNodeKind::StringFromCharArray: return "string-from-char-array";
        case ProjectOopMirNodeKind::StringViewFromString: return "string-view-from-string";
        case ProjectOopMirNodeKind::SpanFromArray: return "span-from-array";
        case ProjectOopMirNodeKind::ReadOnlySpanFromArray: return "readonly-span-from-array";
        case ProjectOopMirNodeKind::SpanFromFixedArray: return "span-from-fixed-array";
        case ProjectOopMirNodeKind::ReadOnlySpanFromFixedArray: return "readonly-span-from-fixed-array";
        case ProjectOopMirNodeKind::CoreConsoleWriteLine: return "core-console-writeline";
        case ProjectOopMirNodeKind::CoreConsoleReadLine: return "core-console-readline";
        case ProjectOopMirNodeKind::AggregateEquals: return "aggregate-equals";
        case ProjectOopMirNodeKind::AggregateHash: return "aggregate-hash";
        case ProjectOopMirNodeKind::CoreObjectTypeName: return "core-object-type-name";
        case ProjectOopMirNodeKind::CoreObjectReferenceHash: return "core-object-reference-hash";
        case ProjectOopMirNodeKind::CoreClockQueryCounter: return "core-clock-query-counter";
        case ProjectOopMirNodeKind::CoreClockQueryFrequency: return "core-clock-query-frequency";
        case ProjectOopMirNodeKind::CoreClockQuerySystemFileTime: return "core-clock-query-system-filetime";
        case ProjectOopMirNodeKind::CoreEnvironmentTickCount64: return "core-environment-tick-count64";
        case ProjectOopMirNodeKind::CoreEnvironmentProcessorCount: return "core-environment-processor-count";
        case ProjectOopMirNodeKind::CoreEnvironmentExit: return "core-environment-exit";
        case ProjectOopMirNodeKind::CoreEnvironmentGetVariable: return "core-environment-get-variable";
        case ProjectOopMirNodeKind::CoreEnvironmentSetVariable: return "core-environment-set-variable";
        case ProjectOopMirNodeKind::CoreFileExists: return "core-file-exists";
        case ProjectOopMirNodeKind::CoreFileDelete: return "core-file-delete";
        case ProjectOopMirNodeKind::CoreDirectoryExists: return "core-directory-exists";
        case ProjectOopMirNodeKind::CoreDirectoryCreate: return "core-directory-create";
        case ProjectOopMirNodeKind::CoreFileReadAllText: return "core-file-read-all-text";
        case ProjectOopMirNodeKind::CoreFileGetSize: return "core-file-get-size";
        case ProjectOopMirNodeKind::CoreFileReadAllBytes: return "core-file-read-all-bytes";
        case ProjectOopMirNodeKind::CoreFileReadBytes: return "core-file-read-bytes";
        case ProjectOopMirNodeKind::CoreFileWriteAllBytes: return "core-file-write-all-bytes";
        case ProjectOopMirNodeKind::CoreEnvironmentCurrentDirectory: return "core-environment-current-directory";
        case ProjectOopMirNodeKind::CoreProcessRun: return "core-process-run";
        case ProjectOopMirNodeKind::CoreFileWriteAllText: return "core-file-write-all-text";
        case ProjectOopMirNodeKind::CoreFileAppendAllText: return "core-file-append-all-text";
        case ProjectOopMirNodeKind::CoreDirectoryDelete: return "core-directory-delete";
        case ProjectOopMirNodeKind::CorePathGetFullPath: return "core-path-get-full-path";
        case ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgCount: return "core-environment-get-command-line-arg-count";
        case ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgs: return "core-environment-get-command-line-args";
        case ProjectOopMirNodeKind::CoreDirectoryGetFileCount: return "core-directory-get-file-count";
        case ProjectOopMirNodeKind::CoreDirectoryGetFiles: return "core-directory-get-files";
        case ProjectOopMirNodeKind::CoreDirectoryGetDirectoryCount: return "core-directory-get-directory-count";
        case ProjectOopMirNodeKind::CoreDirectoryGetDirectories: return "core-directory-get-directories";
        case ProjectOopMirNodeKind::CoreThreadCreate: return "core-thread-create";
        case ProjectOopMirNodeKind::CoreThreadJoin: return "core-thread-join";
        case ProjectOopMirNodeKind::CoreThreadDetach: return "core-thread-detach";
        case ProjectOopMirNodeKind::CoreAtomicLoadInt32: return "core-atomic-load-int32";
        case ProjectOopMirNodeKind::CoreAtomicStoreInt32: return "core-atomic-store-int32";
        case ProjectOopMirNodeKind::CoreInterlockedIncrementInt32: return "core-interlocked-increment-int32";
        case ProjectOopMirNodeKind::CoreInterlockedDecrementInt32: return "core-interlocked-decrement-int32";
        case ProjectOopMirNodeKind::CoreInterlockedAddInt32: return "core-interlocked-add-int32";
        case ProjectOopMirNodeKind::CoreInterlockedExchangeInt32: return "core-interlocked-exchange-int32";
        case ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt32: return "core-interlocked-compare-exchange-int32";
        case ProjectOopMirNodeKind::CoreAtomicLoadInt64: return "core-atomic-load-int64";
        case ProjectOopMirNodeKind::CoreAtomicStoreInt64: return "core-atomic-store-int64";
        case ProjectOopMirNodeKind::CoreInterlockedIncrementInt64: return "core-interlocked-increment-int64";
        case ProjectOopMirNodeKind::CoreInterlockedDecrementInt64: return "core-interlocked-decrement-int64";
        case ProjectOopMirNodeKind::CoreInterlockedAddInt64: return "core-interlocked-add-int64";
        case ProjectOopMirNodeKind::CoreInterlockedExchangeInt64: return "core-interlocked-exchange-int64";
        case ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt64: return "core-interlocked-compare-exchange-int64";
        case ProjectOopMirNodeKind::CoreAtomicLoadPointer: return "core-atomic-load-pointer";
        case ProjectOopMirNodeKind::CoreAtomicStorePointer: return "core-atomic-store-pointer";
        case ProjectOopMirNodeKind::CoreInterlockedExchangePointer: return "core-interlocked-exchange-pointer";
        case ProjectOopMirNodeKind::CoreInterlockedCompareExchangePointer: return "core-interlocked-compare-exchange-pointer";
        case ProjectOopMirNodeKind::CoreAtomicThreadFence: return "core-atomic-thread-fence";
        case ProjectOopMirNodeKind::CoreAtomicCompilerFence: return "core-atomic-compiler-fence";
        case ProjectOopMirNodeKind::CoreSimdFloat32x4Add: return "core-simd-float32x4-add";
        case ProjectOopMirNodeKind::CoreSimdFloat32x4Subtract: return "core-simd-float32x4-subtract";
        case ProjectOopMirNodeKind::CoreSimdFloat32x4Multiply: return "core-simd-float32x4-multiply";
        case ProjectOopMirNodeKind::CoreSimdInt128Add: return "core-simd-int128-add";
        case ProjectOopMirNodeKind::CoreSimdInt128Subtract: return "core-simd-int128-subtract";
        case ProjectOopMirNodeKind::CoreSimdInt128Multiply: return "core-simd-int128-multiply";
        case ProjectOopMirNodeKind::CoreSimdFloat256Add: return "core-simd-float256-add";
        case ProjectOopMirNodeKind::CoreSimdFloat256Subtract: return "core-simd-float256-subtract";
        case ProjectOopMirNodeKind::CoreSimdFloat256Multiply: return "core-simd-float256-multiply";
        case ProjectOopMirNodeKind::CoreSimdInt256Add: return "core-simd-int256-add";
        case ProjectOopMirNodeKind::CoreSimdInt256Subtract: return "core-simd-int256-subtract";
        case ProjectOopMirNodeKind::CoreSimdInt256Multiply: return "core-simd-int256-multiply";
        case ProjectOopMirNodeKind::CoreSimdLoadAligned128: return "core-simd-load-aligned-128";
        case ProjectOopMirNodeKind::CoreSimdStoreAligned128: return "core-simd-store-aligned-128";
        case ProjectOopMirNodeKind::CoreSimdLoadUnaligned128: return "core-simd-load-unaligned-128";
        case ProjectOopMirNodeKind::CoreSimdStoreUnaligned128: return "core-simd-store-unaligned-128";
        case ProjectOopMirNodeKind::CoreHalfFromFloat: return "core-half-from-float";
        case ProjectOopMirNodeKind::CoreHalfToFloat: return "core-half-to-float";
        case ProjectOopMirNodeKind::CoreSimdSwizzleWzyx: return "core-simd-swizzle-wzyx";
        case ProjectOopMirNodeKind::CoreSimdShuffleLowHigh: return "core-simd-shuffle-low-high";
        case ProjectOopMirNodeKind::CoreSimdSqrt: return "core-simd-sqrt";
        case ProjectOopMirNodeKind::CoreSimdReciprocalSqrt: return "core-simd-reciprocal-sqrt";
        case ProjectOopMirNodeKind::CoreSimdFusedMultiplyAdd: return "core-simd-fused-multiply-add";
        case ProjectOopMirNodeKind::CoreBitPopulationCount: return "core-bit-popcount";
        case ProjectOopMirNodeKind::CoreBitLeadingZeroCount: return "core-bit-clz";
        case ProjectOopMirNodeKind::CoreBitTrailingZeroCount: return "core-bit-ctz";
        case ProjectOopMirNodeKind::CoreBitRotateLeft: return "core-bit-rotate-left";
        case ProjectOopMirNodeKind::CoreBitRotateRight: return "core-bit-rotate-right";
        case ProjectOopMirNodeKind::CoreBitByteSwap: return "core-bit-byte-swap";
        case ProjectOopMirNodeKind::CoreCpuPrefetchRead: return "core-cpu-prefetch-read";
        case ProjectOopMirNodeKind::CoreCpuPause: return "core-cpu-pause";
        case ProjectOopMirNodeKind::CoreCpuReadTimestamp: return "core-cpu-read-timestamp";
        case ProjectOopMirNodeKind::CoreMutexCreate: return "core-mutex-create";
        case ProjectOopMirNodeKind::CoreMutexCreateNamed: return "core-mutex-create-named";
        case ProjectOopMirNodeKind::CoreMutexLock: return "core-mutex-lock";
        case ProjectOopMirNodeKind::CoreMutexUnlock: return "core-mutex-unlock";
        case ProjectOopMirNodeKind::CoreMutexClose: return "core-mutex-close";
        case ProjectOopMirNodeKind::CoreMonitorEnter: return "core-monitor-enter";
        case ProjectOopMirNodeKind::CoreMonitorExit: return "core-monitor-exit";
        case ProjectOopMirNodeKind::CoreMonitorWait: return "core-monitor-wait";
        case ProjectOopMirNodeKind::CoreMonitorPulse: return "core-monitor-pulse";
        case ProjectOopMirNodeKind::CoreMonitorPulseAll: return "core-monitor-pulse-all";
        case ProjectOopMirNodeKind::CoreSemaphoreCreate: return "core-semaphore-create";
        case ProjectOopMirNodeKind::CoreSemaphoreCreateNamed: return "core-semaphore-create-named";
        case ProjectOopMirNodeKind::CoreSemaphoreWait: return "core-semaphore-wait";
        case ProjectOopMirNodeKind::CoreSemaphoreRelease: return "core-semaphore-release";
        case ProjectOopMirNodeKind::CoreSemaphoreClose: return "core-semaphore-close";
        case ProjectOopMirNodeKind::CoreEventCreate: return "core-event-create";
        case ProjectOopMirNodeKind::CoreEventCreateNamed: return "core-event-create-named";
        case ProjectOopMirNodeKind::CoreEventSet: return "core-event-set";
        case ProjectOopMirNodeKind::CoreEventReset: return "core-event-reset";
        case ProjectOopMirNodeKind::CoreEventWait: return "core-event-wait";
        case ProjectOopMirNodeKind::CoreEventClose: return "core-event-close";
        case ProjectOopMirNodeKind::CoreProcessGetCurrentProcessId: return "core-process-get-current-process-id";
        case ProjectOopMirNodeKind::CoreProcessGetWorkingSetSize: return "core-process-get-working-set-size";
        case ProjectOopMirNodeKind::CoreProcessGetKernelTime: return "core-process-get-kernel-time";
        case ProjectOopMirNodeKind::CoreProcessGetUserTime: return "core-process-get-user-time";
        case ProjectOopMirNodeKind::CoreProcessGetPriorityClass: return "core-process-get-priority-class";
        case ProjectOopMirNodeKind::CoreProcessSetPriorityClass: return "core-process-set-priority-class";
        case ProjectOopMirNodeKind::CoreSystemInfoGetPageSize: return "core-system-info-get-page-size";
        case ProjectOopMirNodeKind::CoreSystemInfoGetProcessorArchitecture: return "core-system-info-get-processor-architecture";
        case ProjectOopMirNodeKind::CoreSystemInfoGetAllocationGranularity: return "core-system-info-get-allocation-granularity";
        case ProjectOopMirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes: return "core-system-info-get-total-physical-memory-bytes";
        case ProjectOopMirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes: return "core-system-info-get-available-physical-memory-bytes";
        case ProjectOopMirNodeKind::CoreSystemInfoGetMemoryLoadPercent: return "core-system-info-get-memory-load-percent";
        case ProjectOopMirNodeKind::CoreDebugIsDebuggerPresent: return "core-debug-is-debugger-present";
        case ProjectOopMirNodeKind::CoreDebugCaptureStackFrameCount: return "core-debug-capture-stack-frame-count";
        case ProjectOopMirNodeKind::CoreDebugOutputString: return "core-debug-output-string";
        case ProjectOopMirNodeKind::CoreThreadSleep: return "core-thread-sleep";
        case ProjectOopMirNodeKind::CoreFileCopy: return "core-file-copy";
        case ProjectOopMirNodeKind::CoreFileMove: return "core-file-move";
        case ProjectOopMirNodeKind::CoreDirectoryMove: return "core-directory-move";
        case ProjectOopMirNodeKind::DelegateCreate: return "delegate-create";
        case ProjectOopMirNodeKind::LambdaCreate: return "lambda-create";
        case ProjectOopMirNodeKind::ClosureCaptureLoad: return "closure-capture-load";
        case ProjectOopMirNodeKind::ClosureCaptureAddress: return "closure-capture-address";
        case ProjectOopMirNodeKind::CaptureCellReference: return "capture-cell-reference";
        case ProjectOopMirNodeKind::ClosureCaptureCellReference: return "closure-capture-cell-reference";
        case ProjectOopMirNodeKind::DelegateInvoke: return "delegate-invoke";
        case ProjectOopMirNodeKind::EventAdd: return "event-add";
        case ProjectOopMirNodeKind::EventRemove: return "event-remove";
        case ProjectOopMirNodeKind::EventInvoke: return "event-invoke";
        case ProjectOopMirNodeKind::Unary: return "unary";
        case ProjectOopMirNodeKind::Binary: return "binary";
        case ProjectOopMirNodeKind::Conditional: return "conditional";
        case ProjectOopMirNodeKind::NullCoalescing: return "null-coalescing";
        case ProjectOopMirNodeKind::OptionalCoalescing: return "optional-coalescing";
        case ProjectOopMirNodeKind::NullCoalescingStore: return "null-coalescing-store";
        case ProjectOopMirNodeKind::Store: return "store";
        case ProjectOopMirNodeKind::Convert: return "convert";
        case ProjectOopMirNodeKind::RuntimeTypeTest: return "runtime-type-test";
        case ProjectOopMirNodeKind::SafeReferenceCast: return "safe-reference-cast";
        case ProjectOopMirNodeKind::CheckedReferenceCast: return "checked-reference-cast";
        case ProjectOopMirNodeKind::DirectInstanceCall: return "direct-instance-call";
        case ProjectOopMirNodeKind::DirectStaticCall: return "direct-static-call";
        case ProjectOopMirNodeKind::VirtualInstanceCall: return "virtual-instance-call";
        case ProjectOopMirNodeKind::InterfaceInstanceCall: return "interface-instance-call";
        case ProjectOopMirNodeKind::AllocateAndConstruct: return "allocate-and-construct";
        case ProjectOopMirNodeKind::Deallocate: return "deallocate";
        case ProjectOopMirNodeKind::ObjectDestroy: return "object-destroy";
        case ProjectOopMirNodeKind::Assert: return "assert";
        case ProjectOopMirNodeKind::Unreachable: return "unreachable";
        case ProjectOopMirNodeKind::Assume: return "assume";
        case ProjectOopMirNodeKind::Panic: return "panic";
        case ProjectOopMirNodeKind::ResultPropagation: return "result-propagation";
        case ProjectOopMirNodeKind::LoopExpression: return "loop-expression";
        case ProjectOopMirNodeKind::MatchExpression: return "match-expression";
        case ProjectOopMirNodeKind::MatchArm: return "match-arm";
        case ProjectOopMirNodeKind::MatchValue: return "match-value";
        case ProjectOopMirNodeKind::OverflowPolicy: return "overflow-policy";
    }
    return "unknown";
}

bool ProjectOopMirModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectVirtualDispatchModel& virtualDispatchModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    std::string& error,
    std::size_t workerCount)
{
    error.clear();
    if (built_)
    {
        error = "ProjectOopMirModel instances are single-use";
        return false;
    }
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !objectLayoutModel.isBuilt() ||
        !virtualDispatchModel.isBuilt() || !interfaceModel.isBuilt() ||
        !destructionModel.isBuilt() || !hirModel.isBuilt())
    {
        error = "Stage 34 OOP MIR requires built OOP HIR and Stage 29 object layouts";
        return false;
    }

    nameLookup_ = &nameLookup;
    memberModel_ = &memberModel;
    objectLayoutModel_ = &objectLayoutModel;
    virtualDispatchModel_ = &virtualDispatchModel;
    interfaceModel_ = &interfaceModel;
    destructionModel_ = &destructionModel;
    hirModel_ = &hirModel;
    if (!importLayouts(nameLookup, memberModel, objectLayoutModel, hirModel, error))
    {
        return false;
    }
    functionByMember_.resize(memberModel.members().size());
    for (const ProjectOopHirFunction& function : hirModel.functions())
    {
        if (function.namespaceFunctionOrdinal.has_value() &&
            *function.namespaceFunctionOrdinal >= functionByNamespaceFunction_.size())
        {
            functionByNamespaceFunction_.resize(*function.namespaceFunctionOrdinal + 1U);
        }
    }
    dispatchTableByType_.resize(nameLookup.namedTypes().size());

    for (const ClassVirtualTable& sourceTable : virtualDispatchModel.classTables())
    {
        if (sourceTable.typeOrdinal >= nameLookup.namedTypes().size())
        {
            error = "Macro A virtual table refers to an invalid class ordinal";
            return false;
        }
        ProjectOopMirDispatchTable table;
        table.isImported = nameLookup.namedTypes()[sourceTable.typeOrdinal].isPrecompiled;
        table.genericSpecializationKey = genericSpecializationKeyForType(nameLookup, sourceTable.typeOrdinal);
        table.typeOrdinal = sourceTable.typeOrdinal;
        table.baseTypeOrdinal = sourceTable.baseTypeOrdinal;
        table.typeKey = "type:" + nameLookup.namedTypes()[sourceTable.typeOrdinal].identity.key();
        const std::string suffix = sanitizeDispatchName(nameLookup.namedTypes()[sourceTable.typeOrdinal].identity.key());
        table.descriptorName = "__nsc_classdesc_" + suffix;
        table.descriptorAccessorName = "__nsc_get_classdesc_" + suffix;
        table.vtableName = "__nsc_vtable_" + suffix;
        table.interfaceMapName = "__nsc_ifmap_" + suffix;
        table.stableTypeId = stableTypeId(table.typeKey);
        // C8: the SHORT name, not the fully-qualified key -- `Dictionary`, not
        // `Proj::NativeSharp.Core.Collections.Generic.Dictionary`. That is what a ToString()
        // default is useful for, and it matches what real C#'s GetType().Name gives.
        {
            const std::string& identity = nameLookup.namedTypes()[sourceTable.typeOrdinal].identity.key();
            const std::size_t lastDot = identity.find_last_of(".:");
            table.typeDisplayName = lastDot == std::string::npos
                ? identity : identity.substr(lastDot + 1);
        }
        table.typeNameGlobalName = "__nsc_typename_" + suffix;
        table.slotMemberOrdinals = sourceTable.slotMemberOrdinals;
        for (std::size_t slot = 0; slot < sourceTable.slotMemberOrdinals.size(); ++slot)
        {
            const std::size_t slotMember = sourceTable.slotMemberOrdinals[slot];
            const bool abstractSlot = slot < sourceTable.slotAbstract.size() && sourceTable.slotAbstract[slot];
            if (abstractSlot)
            {
                table.slotCalleeNames.emplace_back();
                continue;
            }
            const ProjectOopHirFunction* target = hirModel.functionForMember(slotMember);
            if (target == nullptr)
            {
                error = "Stage 35 concrete vtable slot refers to a method without OOP HIR body";
                return false;
            }
            table.slotCalleeNames.push_back(target->mangledName);
        }
        const ClassDestructionPlan* destruction = destructionModel.planForType(table.typeOrdinal);
        if (destruction == nullptr)
        {
            error = "Stage 35 dispatch table has no class destruction plan";
            return false;
        }
        table.destructionEntryName = destruction->destructionEntryName;
        table.objectDestructionEntryName = destruction->objectDestructionEntryName;
        table.destructorMemberOrdinals = destruction->destructorMemberOrdinals;
        if (!table.isImported)
        {
            for (const std::size_t destructorMember : destruction->destructorMemberOrdinals)
            {
                if (destructorMember < memberModel.members().size() &&
                    memberModel.members()[destructorMember].isPrecompiled)
                {
                    continue;
                }
                const ProjectOopHirFunction* destructorBody = hirModel.functionForMember(destructorMember);
                if (destructorBody == nullptr || !destructorBody->destructor)
                {
                    error = "Stage 35 destruction plan refers to a destructor without OOP HIR body";
                    return false;
                }
                table.destructorCalleeNames.push_back(destructorBody->mangledName);
            }
        }

        for (const std::size_t interfaceOrdinal : interfaceModel.interfacesOfClass(table.typeOrdinal))
        {
            const ClassInterfaceTablePlan* plan = interfaceModel.tablePlan(table.typeOrdinal, interfaceOrdinal);
            if (plan == nullptr)
            {
                error = "Stage 36 class interface map has no semantic table plan";
                return false;
            }
            ProjectOopMirDispatchTable::InterfaceTable interfaceTable;
            interfaceTable.interfaceTypeOrdinal = interfaceOrdinal;
            interfaceTable.interfaceTypeKey = interfaceModel.typeKeyForOrdinal(interfaceOrdinal);
            interfaceTable.stableInterfaceTypeId = plan->stableInterfaceTypeId;
            interfaceTable.methodTableName = "__nsc_iftable_" + suffix + "__" +
                sanitizeDispatchName(nameLookup.namedTypes()[interfaceOrdinal].identity.key());
            interfaceTable.interfaceMemberOrdinals = plan->interfaceMethodMemberOrdinals;
            for (const std::optional<std::size_t> implementation : plan->implementationMemberOrdinals)
            {
                if (!implementation.has_value())
                {
                    interfaceTable.implementationMemberOrdinals.push_back(memberModel.members().size());
                    interfaceTable.calleeNames.emplace_back();
                    continue;
                }
                interfaceTable.implementationMemberOrdinals.push_back(*implementation);
                const ProjectOopHirFunction* implementationFunction = hirModel.functionForMember(*implementation);
                if (implementationFunction == nullptr)
                {
                    error = "Stage 36 concrete interface implementation has no OOP HIR function";
                    return false;
                }
                interfaceTable.calleeNames.push_back(implementationFunction->mangledName);
            }
            table.interfaceTables.push_back(std::move(interfaceTable));
        }
        dispatchTableByType_[table.typeOrdinal] = dispatchTables_.size();
        dispatchTables_.push_back(std::move(table));
    }

    // Bloque 1 item 1 (owning field storage) -- collect, per class, the instance fields it OWNS, so
    // the destruction entry can release them without the author writing a `delete` by hand. Done as
    // a pass of its own AFTER every dispatch table exists, because deciding whether a field's type
    // is destroyable means asking whether that type has a class descriptor at all -- and that is
    // exactly the set of types that got a table above.
    //
    // Only fields whose type genuinely owns a separate allocation are listed. A value-typed `owned`
    // field owns no heap block for the traversal to release, which is the same rule already applied
    // to `owned` PARAMETERS (requiresOwnedDestruction) and to `owned` LOCALS
    // (ProjectMethodBodyModel) -- the three spellings agree on purpose.
    {
        std::unordered_set<std::string> destroyableClassTypeKeys;
        for (const ProjectOopMirDispatchTable& built : dispatchTables_)
        {
            destroyableClassTypeKeys.insert(built.typeKey);
        }
        const auto fieldOwnsAllocation = [&](const ClassMemberSymbol& member) {
            if (member.declaredTypeKey == "builtin:string") return true;
            return destroyableClassTypeKeys.contains(member.declaredTypeKey);
        };
        for (ProjectOopMirDispatchTable& built : dispatchTables_)
        {
            // The producer already emitted this complete plan behind destructionEntryName.
            // Rebuilding it would require importing private destructor bodies and field layout.
            if (built.isImported) continue;
            // The chain is walked MOST-DERIVED FIRST, and within each class the owned fields are
            // taken in REVERSE declaration order -- destruction is the exact reverse of
            // construction. This is the frozen order contract; the full statement and its
            // rationale live on ProjectOopMirDispatchTable::destructionSteps.
            std::vector<std::size_t> chain;
            for (std::optional<std::size_t> current = built.typeOrdinal; current.has_value();)
            {
                chain.push_back(*current);
                const std::optional<std::size_t> tableIndex =
                    (*current < dispatchTableByType_.size())
                        ? dispatchTableByType_[*current]
                        : std::nullopt;
                current = tableIndex.has_value()
                    ? dispatchTables_[*tableIndex].baseTypeOrdinal
                    : std::nullopt;
            }
            for (const std::size_t chainTypeOrdinal : chain)
            {
                // The precompiled producer owns the complete remainder of this base chain,
                // including private destructor bodies and private owned-field layout. Calling its
                // descriptor's destruction entry is the only complete operation available to a
                // consumer, and it is terminal because that entry also releases the allocation.
                if (chainTypeOrdinal < nameLookup.namedTypes().size() &&
                    nameLookup.namedTypes()[chainTypeOrdinal].isPrecompiled)
                {
                    built.delegatedBaseDestructionTypeOrdinal = chainTypeOrdinal;
                    break;
                }
                // This class's destructor body first: it runs while every one of its own owned
                // fields is still alive, and the null-guarded releases right after it skip
                // anything it already discharged by hand.
                const ClassDestructionPlan* chainPlan = destructionModel.planForType(chainTypeOrdinal);
                if (chainPlan != nullptr && chainPlan->explicitDestructorMemberOrdinal.has_value())
                {
                    const ProjectOopHirFunction* body =
                        hirModel.functionForMember(*chainPlan->explicitDestructorMemberOrdinal);
                    if (body == nullptr || !body->destructor)
                    {
                        error = "Bloque 1 destruction order refers to a destructor without OOP HIR body";
                        return false;
                    }
                    ProjectOopMirDispatchTable::DestructionStep bodyStep;
                    bodyStep.destructorCalleeName = body->mangledName;
                    built.destructionSteps.push_back(std::move(bodyStep));
                }
                const std::span<const std::size_t> classMembers =
                    memberModel.membersOfType(chainTypeOrdinal);
                for (auto memberIt = classMembers.rbegin(); memberIt != classMembers.rend(); ++memberIt)
                {
                    const std::size_t memberOrdinal = *memberIt;
                    const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
                    if (member.kind != ClassMemberKind::Field || member.isStatic) continue;
                    if (member.declaredOwnership != sema::OwnershipKind::Owned) continue;
                    if (!fieldOwnsAllocation(member)) continue;
                    const ObjectFieldLayout* layout = objectLayoutModel.fieldLayoutForMember(memberOrdinal);
                    if (layout == nullptr)
                    {
                        error = "Bloque 1 owned field has no object layout";
                        return false;
                    }
                    const ProjectOopMirDispatchTable::OwnedField ownedField{
                        memberOrdinal, layout->offset, member.declaredTypeKey == "builtin:string"};
                    built.ownedFields.push_back(ownedField);
                    ProjectOopMirDispatchTable::DestructionStep fieldStep;
                    fieldStep.ownedField = ownedField;
                    fieldStep.isFieldRelease = true;
                    built.destructionSteps.push_back(std::move(fieldStep));
                }
            }
        }
    }

    if (!lowerFunctions(
            nameLookup,
            memberModel,
            objectLayoutModel,
            virtualDispatchModel,
            interfaceModel,
            destructionModel,
            hirModel,
            error,
            workerCount))
    {
        return false;
    }

    for (const ProjectOopHirFieldInitializer& source : hirModel.fieldInitializers())
    {
        auto value = lowerNode(source.value.get(), error);
        if (value == nullptr)
        {
            // Same reason as the function body above: a field initializer that fails to lower
            // otherwise reports nothing at all about which field it was.
            error += "\n  while lowering the field initializer of member #" +
                std::to_string(source.memberOrdinal);
            return false;
        }
        fieldInitializers_.push_back(ProjectOopMirFieldInitializer{
            source.memberOrdinal,
            source.declaringTypeOrdinal,
            source.isStatic,
            source.isConst,
            std::move(value)});
    }

    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& field = memberModel.members()[memberOrdinal];
        if (field.kind == ClassMemberKind::Field && field.isStatic)
        {
            ProjectOopMirStaticField staticField;
            staticField.memberOrdinal = memberOrdinal;
            staticField.declaringTypeOrdinal = field.declaringTypeOrdinal;
            staticField.name = field.name;
            staticField.typeKey = field.declaredTypeKey;
            staticField.isReadonly = field.isReadonly;
            staticField.isVolatile = field.isVolatile;
            staticField.isConst = field.isConst;
            staticField.isImported = field.isPrecompiled;
            staticField.genericSpecializationKey =
                genericSpecializationKeyForType(nameLookup, field.declaringTypeOrdinal);
            if (field.declaringTypeOrdinal >= nameLookup.namedTypes().size())
            {
                error = "Stage 37 static field has an invalid declaring type ordinal";
                return false;
            }
            const std::string typeFragment = compilation::symbolFragmentForIdentityKey(
                nameLookup.namedTypes()[field.declaringTypeOrdinal].identity.key());
            const std::string fieldFragment = compilation::symbolFragmentForIdentityKey(field.name);
            staticField.storageSymbol = "__nsc_static_" + typeFragment + "__" + fieldFragment;
            staticField.accessorName = "__nsc_get_static_" + typeFragment + "__" + fieldFragment;
            staticField.sourceRange = field.range;
            staticField.debugDisplayName =
                nameLookup.namedTypes()[field.declaringTypeOrdinal].identity.fullyQualifiedName() +
                "." + field.name;
            staticField.isEnumConstant = field.isEnumConstant;
            if (field.isConst)
            {
                for (const ProjectOopHirFieldInitializer& initializer : hirModel.fieldInitializers())
                {
                    if (initializer.memberOrdinal != memberOrdinal || initializer.value == nullptr)
                    {
                        continue;
                    }
                    // Stage 51.x: an enum member's declaredTypeKey is its own enum type (see the
                    // comment in ProjectMemberModel::collectEnumMembers), so isIntegral() alone
                    // would miss it here -- it is still an int-sized compile-time constant.
                    if (ProjectTypeConversion::isIntegral(field.declaredTypeKey) || field.isEnumConstant)
                    {
                        const std::optional<std::uint64_t> value =
                            MirConstantEvaluator::integerValue(initializer.value.get(), staticFields_);
                        if (value.has_value())
                        {
                            staticField.hasConstantInitializer = true;
                            staticField.constantIntegerValue = *value;
                        }
                    }
                    else if (field.declaredTypeKey == "builtin:bool")
                    {
                        const std::optional<bool> value =
                            MirConstantEvaluator::booleanValue(initializer.value.get(), staticFields_);
                        if (value.has_value())
                        {
                            staticField.hasConstantInitializer = true;
                            staticField.constantBooleanValue = *value;
                        }
                    }
                    else if (field.declaredTypeKey == "builtin:float" || field.declaredTypeKey == "builtin:double" ||
                             field.declaredTypeKey == "builtin:decimal")
                    {
                        const std::optional<double> value =
                            MirConstantEvaluator::floatingValue(initializer.value.get(), staticFields_);
                        if (value.has_value())
                        {
                            staticField.hasConstantInitializer = true;
                            staticField.constantFloatingValue = *value;
                        }
                    }
                    else if (field.declaredTypeKey == "builtin:string")
                    {
                        const std::optional<std::string> value =
                            MirConstantEvaluator::stringValue(initializer.value.get(), staticFields_);
                        if (value.has_value())
                        {
                            staticField.hasConstantInitializer = true;
                            staticField.constantStringValue = *value;
                        }
                    }
                    break;
                }
                if (!staticField.hasConstantInitializer)
                {
                    error = "Stage 37 const initializer validated semantically but could not be materialized as a native constant";
                    return false;
                }
            }
            staticFields_.push_back(std::move(staticField));
        }
    }

    const std::size_t namespaceStateMemberBase = memberModel.members().size();
    for (std::size_t stateOrdinal = 0;
         stateOrdinal < namespaceFunctionModel.states().size(); ++stateOrdinal)
    {
        const NamespaceStateSymbol& state = namespaceFunctionModel.states()[stateOrdinal];
        ProjectOopMirStaticField staticField;
        staticField.memberOrdinal = namespaceStateMemberBase + stateOrdinal;
        staticField.declaringTypeOrdinal = nameLookup.namedTypes().size();
        staticField.name = state.name;
        staticField.typeKey = state.declaredTypeKey;
        staticField.isReadonly = state.isConst;
        staticField.isConst = state.isConst;
        staticField.isImported = state.isPrecompiled;
        staticField.storageSymbol = state.storageName;
        staticField.accessorName = "__nsc_get_" + state.storageName;
        staticField.namespaceStateOrdinal = stateOrdinal;
        staticField.sourceRange = state.range;
        staticField.debugDisplayName = state.namespaceName.empty()
            ? state.name
            : state.namespaceName + "." + state.name;
        switch (state.initializerKind)
        {
            case NamespaceStateInitializerKind::Integer:
                staticField.hasConstantInitializer = true;
                staticField.constantIntegerValue = state.integerValue;
                break;
            case NamespaceStateInitializerKind::Floating:
                staticField.hasConstantInitializer = true;
                staticField.constantFloatingValue = state.floatingValue;
                break;
            case NamespaceStateInitializerKind::Boolean:
                staticField.hasConstantInitializer = true;
                staticField.constantBooleanValue = state.booleanValue;
                break;
            case NamespaceStateInitializerKind::String:
                staticField.hasConstantInitializer = true;
                staticField.constantStringValue = state.stringValue;
                break;
            case NamespaceStateInitializerKind::Null:
                staticField.hasConstantInitializer = true;
                break;
            case NamespaceStateInitializerKind::Zero:
                if (state.isConst) staticField.hasConstantInitializer = true;
                break;
        }
        staticFields_.push_back(std::move(staticField));
    }
    const auto recordGenericSpecialization = [&](std::string_view key) -> std::size_t {
        const auto found = std::find_if(genericSpecializations_.begin(), genericSpecializations_.end(),
            [key](const ProjectOopMirGenericSpecialization& specialization) {
                return specialization.key == key;
            });
        if (found != genericSpecializations_.end())
            return static_cast<std::size_t>(found - genericSpecializations_.begin());
        genericSpecializations_.push_back(
            ProjectOopMirGenericSpecialization{std::string(key), {}, {}, {}});
        return genericSpecializations_.size() - 1;
    };
    for (std::size_t index = 0; index < functions_.size(); ++index)
    {
        if (functions_[index].genericSpecializationKey.empty()) continue;
        genericSpecializations_[recordGenericSpecialization(functions_[index].genericSpecializationKey)]
            .functionIndices.push_back(index);
    }
    for (std::size_t index = 0; index < staticFields_.size(); ++index)
    {
        if (staticFields_[index].genericSpecializationKey.empty()) continue;
        genericSpecializations_[recordGenericSpecialization(staticFields_[index].genericSpecializationKey)]
            .staticFieldIndices.push_back(index);
    }
    for (std::size_t index = 0; index < dispatchTables_.size(); ++index)
    {
        const ProjectOopMirDispatchTable& table = dispatchTables_[index];
        if (!table.genericSpecializationKey.empty())
        {
            genericSpecializations_[recordGenericSpecialization(table.genericSpecializationKey)]
                .dispatchTableIndices.push_back(index);
        }
        for (const ProjectOopMirDispatchTable::InterfaceTable& source : table.interfaceTables)
        {
            if (source.interfaceMemberOrdinals.size() != source.implementationMemberOrdinals.size() ||
                source.interfaceMemberOrdinals.size() != source.calleeNames.size())
            {
                error = "Item 398 MIR interface binding has inconsistent slot vectors";
                return false;
            }
            if (source.interfaceTypeKey.empty() || source.stableInterfaceTypeId == 0 ||
                source.methodTableName.empty())
            {
                error = "Item 398 MIR interface binding is missing stable ABI identity";
                return false;
            }
            if (interfaceBindingForTypes(table.typeOrdinal, source.interfaceTypeOrdinal) != nullptr)
            {
                error = "Item 398 MIR contains a duplicate class/interface binding";
                return false;
            }
            interfaceBindings_.push_back(ProjectOopMirInterfaceBinding{
                table.typeOrdinal,
                source.interfaceTypeOrdinal,
                source.interfaceTypeKey,
                source.stableInterfaceTypeId,
                source.methodTableName,
                source.interfaceMemberOrdinals,
                source.implementationMemberOrdinals,
                source.calleeNames});
        }
    }

    panicHandlerMangledName_.clear();
    if (!panicHandler_.empty())
    {
        const auto found = std::find_if(functions_.begin(), functions_.end(), [&](const auto& function) {
            return function.debugDisplayName == panicHandler_;
        });
        if (found == functions_.end())
        {
            error = "PanicHandler '" + panicHandler_ +
                "' does not name a project namespace function";
            return false;
        }
        static const std::vector<std::string> expectedParameters = {
            "builtin:string", "builtin:string", "builtin:int", "builtin:int"};
        if (!found->isStatic || found->referenceReturn || found->isVariadic ||
            found->returnTypeKey != "builtin:void" || !found->errorTypeKey.empty() ||
            found->parameterTypeKeys != expectedParameters)
        {
            error = "PanicHandler '" + panicHandler_ +
                "' must be a non-throwing free function with signature "
                "void(string message, string file, int line, int column)";
            return false;
        }
        panicHandlerMangledName_ = found->mangledName;
    }

    built_ = true;
    // NSC305-C04. Persist the built MIR functions (no-op when the cache is disabled).
    storeMirFunctions();
    return true;
}

void ProjectOopMirModel::setPanicProfile(language::PanicProfile profile) noexcept
{
    panicProfile_ = profile;
}

language::PanicProfile ProjectOopMirModel::panicProfile() const noexcept
{
    return panicProfile_;
}

void ProjectOopMirModel::setPanicHandler(std::string handler)
{
    panicHandler_ = std::move(handler);
}

std::string_view ProjectOopMirModel::panicHandler() const noexcept
{
    return panicHandler_;
}

std::string_view ProjectOopMirModel::panicHandlerMangledName() const noexcept
{
    return panicHandlerMangledName_;
}

std::span<const ProjectOopMirFunction> ProjectOopMirModel::functions() const noexcept
{
    return functions_;
}

std::span<const ProjectOopMirFieldInitializer> ProjectOopMirModel::fieldInitializers() const noexcept
{
    return fieldInitializers_;
}

std::span<const ProjectOopMirStaticField> ProjectOopMirModel::staticFields() const noexcept
{
    return staticFields_;
}

std::span<const ProjectOopMirStructLayout> ProjectOopMirModel::structLayouts() const noexcept
{
    return structLayouts_;
}

std::span<const ProjectOopMirClassLayout> ProjectOopMirModel::classLayouts() const noexcept
{
    return classLayouts_;
}

std::span<const ProjectOopMirDispatchTable> ProjectOopMirModel::dispatchTables() const noexcept
{
    return dispatchTables_;
}

std::span<const ProjectOopMirGenericSpecialization>
ProjectOopMirModel::genericSpecializations() const noexcept
{
    return genericSpecializations_;
}

std::span<const ProjectOopMirInterfaceBinding> ProjectOopMirModel::interfaceBindings() const noexcept
{
    return interfaceBindings_;
}

const ProjectOopMirDispatchTable* ProjectOopMirModel::dispatchTableForType(std::size_t typeOrdinal) const noexcept
{
    if (typeOrdinal >= dispatchTableByType_.size() || !dispatchTableByType_[typeOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *dispatchTableByType_[typeOrdinal];
    return index < dispatchTables_.size() ? &dispatchTables_[index] : nullptr;
}

const ProjectOopMirInterfaceBinding* ProjectOopMirModel::interfaceBindingForTypes(
    std::size_t implementingTypeOrdinal,
    std::size_t interfaceTypeOrdinal) const noexcept
{
    const auto found = std::find_if(interfaceBindings_.begin(), interfaceBindings_.end(),
        [implementingTypeOrdinal, interfaceTypeOrdinal](const ProjectOopMirInterfaceBinding& binding) {
            return binding.implementingTypeOrdinal == implementingTypeOrdinal &&
                binding.interfaceTypeOrdinal == interfaceTypeOrdinal;
        });
    return found == interfaceBindings_.end() ? nullptr : &*found;
}

bool ProjectOopMirModel::interfaceSlotForMember(
    std::size_t interfaceMemberOrdinal,
    std::uint64_t& stableInterfaceTypeId,
    std::size_t& slot) const noexcept
{
    for (const ProjectOopMirInterfaceBinding& binding : interfaceBindings_)
    {
        const auto found = std::find(binding.interfaceMemberOrdinals.begin(),
            binding.interfaceMemberOrdinals.end(), interfaceMemberOrdinal);
        if (found == binding.interfaceMemberOrdinals.end()) continue;
        stableInterfaceTypeId = binding.stableInterfaceTypeId;
        slot = static_cast<std::size_t>(found - binding.interfaceMemberOrdinals.begin());
        return true;
    }
    return false;
}

const ProjectOopMirFunction* ProjectOopMirModel::functionForMember(std::size_t memberOrdinal) const noexcept
{
    if (memberOrdinal >= functionByMember_.size() || !functionByMember_[memberOrdinal].has_value())
    {
        return nullptr;
    }
    return &functions_[*functionByMember_[memberOrdinal]];
}

const ProjectOopMirFunction* ProjectOopMirModel::functionForNamespaceFunction(
    std::size_t functionOrdinal) const noexcept
{
    if (functionOrdinal >= functionByNamespaceFunction_.size() ||
        !functionByNamespaceFunction_[functionOrdinal].has_value())
    {
        return nullptr;
    }
    return &functions_[*functionByNamespaceFunction_[functionOrdinal]];
}
const ProjectOopMirFunction* ProjectOopMirModel::executableEntryPoint(
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    bool& ambiguous) const noexcept
{
    ambiguous = false;
    const ProjectOopMirFunction* result = nullptr;
    for (const ProjectOopMirFunction& function : functions_)
    {
        bool candidate = false;
        if (function.namespaceFunctionOrdinal.has_value())
        {
            if (*function.namespaceFunctionOrdinal >= namespaceFunctionModel.functions().size())
                continue;
            const NamespaceFunctionSymbol& symbol =
                namespaceFunctionModel.functions()[*function.namespaceFunctionOrdinal];
            candidate = symbol.name == "Main" && symbol.parameters.empty() &&
                symbol.declaredTypeKey == "builtin:int" && !symbol.isExternC;
        }
        else if (!function.isLambda && !function.constructor && !function.destructor &&
                 memberModel_ != nullptr && function.memberOrdinal < memberModel_->members().size())
        {
            const ClassMemberSymbol& member = memberModel_->members()[function.memberOrdinal];
            candidate = member.kind == ClassMemberKind::Method && member.name == "Main" &&
                member.isStatic && member.parameters.empty() &&
                member.declaredTypeKey == "builtin:int";
        }
        if (!candidate) continue;
        if (result != nullptr)
        {
            ambiguous = true;
            return nullptr;
        }
        result = &function;
    }
    return result;
}
bool ProjectOopMirModel::isBuilt() const noexcept
{
    return built_;
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


const ObjectFieldLayout* ProjectOopMirModel::fieldLayout(std::size_t memberOrdinal) const noexcept
{
    return objectLayoutModel_ != nullptr
        ? objectLayoutModel_->fieldLayoutForMember(memberOrdinal)
        : nullptr;
}

const ObjectAllocationPlan* ProjectOopMirModel::allocationPlan(std::size_t constructionOrdinal) const noexcept
{
    if (objectLayoutModel_ == nullptr)
    {
        return nullptr;
    }
    // The plan vector is NOT indexed by construction ordinal, and treating it as if it were was
    // the bug. buildAllocationPlans() walks every construction but APPENDS a plan only for the
    // ones that actually allocate: it skips an unresolved construction, and -- since C6 -- every
    // STRUCT construction, which has no class layout and needs no plan at all because there is
    // nothing to allocate.
    //
    // So `allocationPlans()[ordinal]` was only correct while every construction produced a plan.
    // One skipped construction shifted every later plan down by one, and from there the lookup
    // either ran off the end -- the reported "Stage 34 cannot find Stage 29 allocation plan", an
    // internal error with no file and no line, which cost nine bisection steps to isolate when it
    // showed up in a 300-line file -- or, in the cases that stayed in range, silently returned
    // SOMEBODY ELSE'S plan and would have allocated a class with the wrong size and alignment.
    // The loud half is what got reported; the silent half is the reason this is worth fixing at
    // the root instead of widening the bounds check.
    //
    // Each plan already records the ordinal it belongs to, so match on it.
    for (const ObjectAllocationPlan& plan : objectLayoutModel_->allocationPlans())
    {
        if (plan.constructionOrdinal == constructionOrdinal)
        {
            return &plan;
        }
    }
    return nullptr;
}

// C2.1 "fixed arrays": the size and alignment of one element, for every element type this
// compiler can put in an array -- managed (heap, length-prefixed) or raw (inline storage, a raw
// pointer). Extracted from an inline lambda in the managed-array case so the raw case asks the
// SAME question the same way; two answers that could disagree about a struct's size would produce
// a GEP stride that does not match the layout the field was given.
std::optional<std::pair<std::uint64_t, std::uint64_t>> ProjectOopMirModel::elementLayout(
    std::string_view typeKey) const
{
    if (nameLookup_ == nullptr || objectLayoutModel_ == nullptr) return std::nullopt;
    // One rule for "how big is one of these", shared with the OOP HIR model's aggregate-element
    // offsets (C7). See ProjectObjectLayoutModel::valueLayoutForTypeKey.
    return objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, typeKey);
}

}
