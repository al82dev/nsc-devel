#include "InteropTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace interop_support;

void deterministicProjectionContract(TestContext& context, std::size_t variant)
{
    auto left = minimalMetadata();
    left.types.push_back(publicStruct("type:Z", "Test.Z"));
    left.types.push_back(publicStruct("type:A", "Test.A"));
    auto right = left;
    std::reverse(right.types.begin(), right.types.end());
    context.expect(interop::LibraryApiDiffer::sourceSurfaceManifest(left) ==
        interop::LibraryApiDiffer::sourceSurfaceManifest(right),
        "source projection must canonicalize catalog order");
    context.expect(interop::LibraryApiDiffer::abiSurfaceManifest(left) ==
        interop::LibraryApiDiffer::abiSurfaceManifest(right),
        "ABI projection must canonicalize catalog order");
    if (variant == 8)
        context.expect(interop::LibraryApiDiffer::abiSurfaceFingerprint(left) ==
            interop::LibraryApiDiffer::abiSurfaceFingerprint(right),
            "large-profile fingerprint must remain ordering-independent");
}

void payloadRoundTripContract(TestContext& context, std::size_t variant)
{
    auto metadata = minimalMetadata("Codec.Module");
    metadata.types.push_back(publicStruct("type:Value", "Codec.Value"));
    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("nsc-host-interop-codec-" + std::to_string(variant) + ".nsmeta");
    interop::LibraryMetadataCodec codec;
    std::string error;
    context.expect(codec.writeModel(metadata, path, error), "representable current model must encode successfully");
    context.expect(error.empty(), "successful encoding must leave no diagnostic");
    interop::LibraryMetadata decoded;
    context.expect(codec.read(path, decoded, error), "encoded model must decode through public file framing");
    context.expect(decoded.moduleName == metadata.moduleName && decoded.types.size() == metadata.types.size(),
        "round trip must preserve model identity and catalog cardinality");
    context.expect(decoded.payloadDecodedByCurrentReader, "authenticated decode must mark current-reader provenance");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

void frozenAbiContract(TestContext& context, std::size_t variant)
{
    interop::LibraryMetadataCodec codec;
    auto metadata = minimalMetadata();
    std::string error;
    context.expect(codec.verifyCurrentCompiler(metadata, error), "current frozen ABI contract must verify");
    metadata.nativeAbiMajor += 1;
    context.expect(!codec.verifyCurrentCompiler(metadata, error), "native ABI major mismatch must be rejected");
    context.expect(error.find("ABI") != std::string::npos, "frozen ABI rejection must retain an ABI diagnostic");
    if (variant == 8)
    {
        metadata = minimalMetadata();
        metadata.pointerAlignment = 4;
        context.expect(!codec.verifyCurrentCompiler(metadata, error), "pointer alignment mismatch must be rejected");
    }
}
}

void registerInteropCodecModule(TestRegistry& registry)
{
    addInteropContract(registry, 34, "unit", deterministicProjectionContract);
    addInteropContract(registry, 35, "unit", payloadRoundTripContract);
    addInteropContract(registry, 39, "unit", frozenAbiContract);
}
}
