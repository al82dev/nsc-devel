#include "InteropTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace interop_support;

void surfaceImpactContract(TestContext& context, std::size_t variant)
{
    auto oldMetadata = minimalMetadata();
    oldMetadata.types.push_back(publicStruct("type:Value", "Test.Value"));
    auto newMetadata = oldMetadata;
    if (variant == 1 || variant == 7)
        newMetadata.types.push_back(publicStruct("type:Added", "Test.Added"));
    else
        newMetadata.types.front().size += 4;
    const auto report = interop::LibraryApiDiffer::compare(oldMetadata, newMetadata);
    if (variant == 1 || variant == 7)
        context.expect(report.sourceCompatible && report.abiCompatible,
            "adding an independent public type must remain compatible");
    else
        context.expect(!report.abiCompatible, "public physical layout change must be ABI-breaking");
    context.expect(!report.changes.empty(), "changed surfaces must report at least one classified change");
}

void classificationContract(TestContext& context, std::size_t)
{
    interop::LibraryApiDiffReport report;
    context.expect(interop::LibraryApiDiffer::classification(report) == "compatible",
        "fully compatible report classification must be exact");
    report.sourceCompatible = false;
    context.expect(interop::LibraryApiDiffer::classification(report) == "source-breaking",
        "source-only break classification must be exact");
    report.abiCompatible = false;
    context.expect(interop::LibraryApiDiffer::classification(report) == "abi-breaking",
        "ABI break classification must dominate source classification");
    context.expect(std::string(interop::LibraryApiDiffer::impactName(interop::LibraryApiChangeImpact::Additive)) ==
        "additive", "additive impact name must be exact");
}

void schemaContract(TestContext& context, std::size_t variant)
{
    context.expect(interop::kMetadataFormatMajor == 1 && interop::kMetadataFormatMinor == 38,
        "current public NSMETA format must be 1.38");
    context.expect(interop::kNativeAbiMajor == 1 && interop::kNativeAbiMinor == 0,
        "native ABI version must be independent from metadata format");
    context.expect(interop::kManagedDelegateCellSize == 32, "managed delegate cell size must remain ABI-visible");
    interop::LibraryMetadata metadata = minimalMetadata();
    context.expect(metadata.formatMajor == interop::kMetadataFormatMajor &&
        metadata.formatMinor == interop::kMetadataFormatMinor, "default model must use current schema constants");
    if (variant == 8)
        context.expect((interop::kMetadataTypeRecordDeriveMask & interop::kMetadataTypeRecord) == 0,
            "record marker and derive mask must occupy disjoint bits");
}

void stableIdentityContract(TestContext& context, std::size_t)
{
    auto left = minimalMetadata();
    interop::LibraryMetadataGenericSpecialization first;
    first.templateKey = "generic:A";
    first.specializationKey = "generic:A<builtin:int>";
    first.specializedName = "A_Int";
    first.typeArguments = {"builtin:int"};
    interop::LibraryMetadataGenericSpecialization second = first;
    second.specializationKey = "generic:A<builtin:long>";
    second.specializedName = "A_Long";
    second.typeArguments = {"builtin:long"};
    left.genericSpecializations = {first, second};
    auto right = left;
    std::reverse(right.genericSpecializations.begin(), right.genericSpecializations.end());
    context.expect(interop::LibraryApiDiffer::abiSurfaceManifest(left) ==
        interop::LibraryApiDiffer::abiSurfaceManifest(right),
        "specialized type identities must canonicalize independently of insertion order");
}
}

void registerInteropDiffSchemaModule(TestRegistry& registry)
{
    addInteropContract(registry, 17, "unit", surfaceImpactContract);
    addInteropContract(registry, 18, "unit", classificationContract);
    addInfrastructureContract(registry, 19, "unit", "canonical-capability-nominal-type-corpus");
    addInfrastructureContract(registry, 20, "unit", "complete-library-capability-surface-corpus");
    addInfrastructureContract(registry, 21, "integration", "isolated-metadata-directory-and-filesystem-errors");
    addInteropContract(registry, 22, "unit", schemaContract);
    addInfrastructureContract(registry, 23, "unit", "byte-level-codec-boundary-and-16mib-allocation-fixture");
    addInteropContract(registry, 24, "unit", stableIdentityContract);
}
}
