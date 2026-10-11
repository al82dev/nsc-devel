#include "InteropTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace interop_support;

void abiVocabularyContract(TestContext& context, std::size_t variant)
{
    auto metadata = minimalMetadata();
    metadata.types.push_back(publicStruct("type:Point", "Test.Point"));
    const std::string report = abiReport(metadata);
    context.expect(report.starts_with("NativeSharp ABI report v1\n"), "ABI report header must be exact");
    context.expect(report.find("module: Test.Module") != std::string::npos,
        "ABI report must publish module identity");
    // The report is an ABI artifact: its public identity is the canonical identityKey, not the
    // source-facing fullyQualifiedName. Requiring the latter would couple this oracle to a field
    // that the report deliberately does not serialize.
    context.expect(report.find("  type type:Point kind=struct") != std::string::npos,
        "ABI report must publish the canonical public type identity");
    if (variant == 8) context.expect(report == abiReport(metadata), "ABI report primitives must be byte-deterministic");
}

void deterministicReportContract(TestContext& context, std::size_t variant)
{
    auto left = minimalMetadata();
    left.types.push_back(publicStruct("type:Z", "Test.Z"));
    left.types.push_back(publicStruct("type:A", "Test.A"));
    auto right = left;
    std::reverse(right.types.begin(), right.types.end());
    context.expect(abiReport(left) == abiReport(right), "ABI report must ignore incidental catalog order");
    if (variant == 8)
        for (std::size_t index = 0; index < 1024; ++index)
            context.expect(abiReport(left) == abiReport(right), "ABI report ordering must remain deterministic at scale");
}

interop::LibraryMetadata metadataWithCallable(std::size_t variant)
{
    auto metadata = minimalMetadata();
    interop::LibraryMetadataNamespaceFunction function;
    function.accessibility = 3;
    function.namespaceName = "Test";
    function.name = "Use";
    function.signatureKey = "Use(builtin:int)";
    function.declaredTypeKey = "builtin:int";
    function.returnLifetimeName = "result";
    function.lifetimeParameters = {"a", "result"};
    function.lifetimeOutlivesConstraints = {{"a", "result"}};
    interop::LibraryMetadataParameter parameter;
    parameter.name = "value";
    parameter.typeKey = "builtin:int";
    parameter.lifetimeName = "a";
    parameter.ffiCallbackLifetime = static_cast<std::uint8_t>(variant % 3);
    function.parameters.push_back(parameter);
    function.parameterEscapeSummary.push_back(interop::LibraryMetadataEscapeClass::NoEscape);
    metadata.namespaceFunctions.push_back(std::move(function));
    return metadata;
}

void lifetimeEscapeContract(TestContext& context, std::size_t variant)
{
    const auto metadata = metadataWithCallable(variant);
    const std::string manifest = interop::LibraryApiDiffer::sourceSurfaceManifest(metadata);
    context.expect(manifest.find("namespace-function:Test:Use(builtin:int)") != std::string::npos,
        "source surface must identify callable canonically");
    const auto changed = metadataWithCallable((variant + 1) % 8);
    context.expect(interop::LibraryApiDiffer::sourceFingerprint(metadata) !=
        interop::LibraryApiDiffer::sourceFingerprint(changed),
        "callback lifetime change must alter source fingerprint");
}

void sourceSurfaceContract(TestContext& context, std::size_t)
{
    auto metadata = minimalMetadata();
    metadata.types.push_back(publicStruct("type:Visible", "Test.Visible"));
    auto hidden = publicStruct("type:Hidden", "Test.Hidden");
    hidden.accessibility = 0;
    metadata.types.push_back(hidden);
    const std::string manifest = interop::LibraryApiDiffer::sourceSurfaceManifest(metadata);
    context.expect(manifest.find("type:type:Visible") != std::string::npos,
        "source surface must include public types");
    context.expect(manifest.find("type:type:Hidden") == std::string::npos,
        "source surface must omit private types");
}

void abiSurfaceContract(TestContext& context, std::size_t variant)
{
    auto metadata = minimalMetadata();
    metadata.types.push_back(publicStruct("type:Value", "Test.Value"));
    const std::string before = interop::LibraryApiDiffer::abiSurfaceFingerprint(metadata);
    metadata.types.front().size += variant;
    const std::string after = interop::LibraryApiDiffer::abiSurfaceFingerprint(metadata);
    context.expect(before != after, "physical layout change must alter ABI surface fingerprint");
}

void manifestFingerprintContract(TestContext& context, std::size_t variant)
{
    auto metadata = metadataWithCallable(variant);
    const std::string sourceManifest = interop::LibraryApiDiffer::sourceSurfaceManifest(metadata);
    const std::string abiManifest = interop::LibraryApiDiffer::abiSurfaceManifest(metadata);
    const std::string sourceFingerprint = interop::LibraryApiDiffer::sourceFingerprint(metadata);
    const std::string abiFingerprint = interop::LibraryApiDiffer::abiSurfaceFingerprint(metadata);
    context.expect(!sourceManifest.empty() && !abiManifest.empty(), "both canonical manifests must be materialized");
    context.expect(sourceFingerprint.size() == 64 && abiFingerprint.size() == 64,
        "surface fingerprints must be lowercase SHA-256 hex");
    context.expect(sourceFingerprint == interop::LibraryApiDiffer::sourceFingerprint(metadata),
        "source fingerprint must be deterministic");
}
}

void registerInteropSurfacesModule(TestRegistry& registry)
{
    addInfrastructureContract(registry, 9, "unit", "c-header-dependency-cycle-project-fixture");
    addInfrastructureContract(registry, 10, "integration", "c-header-atomic-publication-filesystem-fixture");
    addInteropContract(registry, 11, "unit", abiVocabularyContract);
    addInteropContract(registry, 12, "unit", deterministicReportContract);
    addInteropContract(registry, 13, "unit", lifetimeEscapeContract);
    addInteropContract(registry, 14, "unit", sourceSurfaceContract);
    addInteropContract(registry, 15, "unit", abiSurfaceContract);
    addInteropContract(registry, 16, "unit", manifestFingerprintContract);
}
}
