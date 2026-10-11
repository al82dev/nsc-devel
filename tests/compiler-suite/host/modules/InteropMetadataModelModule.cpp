#include "InteropTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace interop_support;

void enumMappingContract(TestContext& context, std::size_t)
{
    context.expect(static_cast<std::uint8_t>(interop::LibraryMetadataTypeKind::Class) == 0 &&
        static_cast<std::uint8_t>(interop::LibraryMetadataTypeKind::Delegate) == 4,
        "metadata type-kind wire values must remain stable");
    context.expect(static_cast<std::uint8_t>(interop::LibraryMetadataOwnershipKind::Owned) == 1 &&
        static_cast<std::uint8_t>(interop::LibraryMetadataOwnershipKind::Borrowed) == 2,
        "metadata ownership wire values must remain stable");
    context.expect(static_cast<std::uint8_t>(interop::LibraryMetadataParameterPassKind::In) == 3,
        "parameter pass-kind wire values must remain stable");
    context.expect(static_cast<std::uint8_t>(interop::LibraryMetadataEscapeClass::Unknown) == 4,
        "escape-summary wire values must remain stable");
}

void defaultValueContract(TestContext& context, std::size_t variant)
{
    auto left = minimalMetadata();
    interop::LibraryMetadataNamespaceFunction function;
    function.namespaceName = "Test";
    function.name = "F";
    function.signatureKey = "F(int)";
    function.declaredTypeKey = "builtin:int";
    interop::LibraryMetadataParameter parameter;
    parameter.name = "value";
    parameter.typeKey = "builtin:int";
    parameter.defaultValueKind = interop::LibraryMetadataDefaultValueKind::Integer;
    parameter.defaultValueBits = variant;
    function.parameters.push_back(parameter);
    left.namespaceFunctions.push_back(function);
    auto right = left;
    right.namespaceFunctions.front().parameters.front().defaultValueBits = variant + 1;
    context.expect(interop::LibraryApiDiffer::sourceFingerprint(left) !=
        interop::LibraryApiDiffer::sourceFingerprint(right),
        "literal default payload must be part of source API identity");
    context.expect(interop::LibraryApiDiffer::abiSurfaceFingerprint(left) ==
        interop::LibraryApiDiffer::abiSurfaceFingerprint(right),
        "literal default payload must not alter binary ABI identity");
}

void flagsSignatureContract(TestContext& context, std::size_t)
{
    context.expect((interop::kMetadataMemberInliningMask >> interop::kMetadataMemberInliningShift) == 3U,
        "inlining policy must occupy exactly two public bits");
    context.expect((interop::kMetadataFieldBitField & interop::kMetadataFieldUnnamedBitField) == 0,
        "named and unnamed bit-field flags must be disjoint");
    context.expect((interop::kMetadataMemberReferenceReturn & interop::kMetadataMemberNoHeap) == 0,
        "reference-return and noheap flags must be independent");
    context.expect(interop::kMetadataGenericConstraintMask == 0xFFU,
        "generic constraint mask must cover the eight declared capabilities");
}

void genericCanonicalizationContract(TestContext& context, std::size_t)
{
    auto left = minimalMetadata();
    interop::LibraryMetadataGenericTemplate generic;
    generic.accessibility = 3;
    generic.canonicalKey = "generic:Identity`1";
    generic.moduleName = "Test.Module";
    generic.name = "Identity";
    generic.parameters = {"T"};
    generic.sourceText = "T Identity<T>(T value) { // comment\n return value; }";
    left.genericTemplates.push_back(generic);
    auto right = left;
    right.genericTemplates.front().sourceText = "T Identity<T>(T value){return value;}";
    context.expect(interop::LibraryApiDiffer::sourceFingerprint(left) ==
        interop::LibraryApiDiffer::sourceFingerprint(right),
        "generic source canonicalization must ignore comments and formatting");
    right.genericTemplates.front().sourceText = "T Identity<T>(T value){return default(T);}";
    context.expect(interop::LibraryApiDiffer::sourceFingerprint(left) !=
        interop::LibraryApiDiffer::sourceFingerprint(right),
        "generic body token change must alter reusable source API identity");
}
}

void registerInteropMetadataModelModule(TestRegistry& registry)
{
    addInteropContract(registry, 25, "unit", enumMappingContract);
    addInteropContract(registry, 26, "unit", defaultValueContract);
    addInteropContract(registry, 27, "unit", flagsSignatureContract);
    addInteropContract(registry, 28, "integration", genericCanonicalizationContract);
}
}
