#include "InteropTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace interop_support;

void compilerGateContract(TestContext& context, std::size_t variant)
{
    interop::LibraryMetadataCodec codec;
    auto metadata = minimalMetadata();
    std::string error;
    if (variant == 3) metadata.moduleName.clear();
    if (variant == 4) metadata.targetTriple = "aarch64-pc-windows-msvc";
    if (variant == 5) metadata.formatMinor = interop::kMetadataFormatMinor + 1;
    if (variant == 6) metadata.targetId = 99;
    const bool expected = variant == 1 || variant == 2 || variant == 7 || variant == 8;
    context.expect(codec.verifyCurrentCompiler(metadata, error) == expected,
        "current compiler gates must accept only supported format, target and module identity");
    context.expect(expected == error.empty(), "gate diagnostic state must match acceptance exactly");
    if (variant == 8)
        for (std::size_t index = 0; index < 2048; ++index)
            context.expect(codec.verifyCurrentCompiler(metadata, error), "gate result must remain deterministic at scale");
}

void typeValidationContract(TestContext& context, std::size_t variant)
{
    interop::LibraryMetadataCodec codec;
    auto metadata = minimalMetadata();
    auto type = publicStruct("type:Record", "Test.Record");
    if (variant == 1 || variant == 2 || variant == 7 || variant == 8)
        type.flags = interop::kMetadataTypeRecord;
    else if (variant == 3)
        type.flags = interop::kMetadataTypeDeriveEquality;
    else if (variant == 4)
    {
        type.kind = interop::LibraryMetadataTypeKind::Class;
        type.flags = interop::kMetadataTypeCopy;
    }
    else if (variant == 5)
    {
        type.flags = interop::kMetadataTypeVariant | interop::kMetadataTypeTrait;
    }
    else
    {
        type.flags = interop::kMetadataTypeOpaqueResult;
    }
    metadata.types.push_back(std::move(type));
    std::string error;
    const bool expected = variant == 1 || variant == 2 || variant == 7 || variant == 8;
    context.expect(codec.verifyCurrentCompiler(metadata, error) == expected,
        "type flags must obey record, Copy, variant, trait and opaque invariants");
    if (!expected) context.expect(!error.empty(), "invalid type contract must retain its first diagnostic");
}

void mergeContract(TestContext& context, std::size_t variant)
{
    interop::LibraryMetadataMerger merger;
    interop::LibraryMetadata merged;
    std::string error;
    context.expect(!merger.merge({}, merged, error), "empty fragment set must be rejected");
    context.expect(error.find("at least one fragment") != std::string::npos,
        "empty merge must identify the missing fragment precondition");

    auto first = minimalMetadata("Merge.Module");
    first.types.push_back(publicStruct("type:B", "Merge.B"));
    auto second = minimalMetadata("Merge.Module");
    second.types.push_back(publicStruct("type:A", "Merge.A"));
    const std::array<interop::LibraryMetadata, 2> fragments = {first, second};
    context.expect(merger.merge(fragments, merged, error), "compatible fragments must merge");
    context.expect(error.empty() && merged.types.size() == 2 && merged.types[0].identityKey == "type:A",
        "merge must canonicalize output order and clear diagnostic state");
    context.expect(std::all_of(merged.abiFingerprint.begin(), merged.abiFingerprint.end(),
        [](std::uint8_t byte) { return byte == 0; }), "merge must clear incoming ABI fingerprint");

    if (variant >= 3)
    {
        auto conflict = first;
        conflict.types.front().size = 8;
        const std::array<interop::LibraryMetadata, 2> conflicting = {first, conflict};
        context.expect(!merger.merge(conflicting, merged, error), "divergent duplicate identity must be rejected");
        context.expect(error.find("merge conflict") != std::string::npos,
            "duplicate conflict must retain exact causal category");
    }
}
}

void registerInteropValidationMergeModule(TestRegistry& registry)
{
    addInteropContract(registry, 43, "unit", compilerGateContract);
    addInteropContract(registry, 44, "unit", typeValidationContract);
    addInteropContract(registry, 48, "unit", mergeContract);
}
}
