#include "nativesharp/Interop/LibraryApiDiff.h"

#include "../support/TestContext.h"

#include <algorithm>

namespace
{
nativesharp::interop::LibraryMetadata metadata()
{
    using namespace nativesharp::interop;
    LibraryMetadata value;
    value.moduleName = "Demo.Api";
    value.targetTriple = "x86_64-pc-windows-msvc";
    value.languageEdition = 3;

    LibraryMetadataType type;
    type.kind = LibraryMetadataTypeKind::Struct;
    type.accessibility = 3;
    type.identityKey = "Demo.Api::Model.Value";
    type.fullyQualifiedName = "Model.Value";
    type.interfaceTypeKeys = {"type:Demo.Contracts::Readable"};
    type.size = 8;
    type.alignment = 8;
    value.types.push_back(type);

    LibraryMetadataMember member;
    member.kind = 2;
    member.accessibility = 3;
    member.declaringTypeKey = "type:Demo.Api::Model.Value";
    member.name = "Read";
    member.signatureKey = "Read()";
    member.declaredTypeKey = "builtin:int";
    member.mangledName = "__nsc_demo_read";
    member.flags = kMetadataMemberNoHeap;
    value.members.push_back(member);

    LibraryMetadataGenericTemplate generic;
    generic.kind = 0;
    generic.accessibility = 3;
    generic.canonicalKey = "Demo.Api::Model.Box`1";
    generic.moduleName = "Demo.Api";
    generic.namespaceName = "Model";
    generic.name = "Box";
    generic.parameters = {"T"};
    generic.sourceText = "public struct Box<T> { /* docs */ public T Get() { return value; } }";
    value.genericTemplates.push_back(generic);

    LibraryMetadataDispatch dispatch;
    dispatch.typeKey = "type:Demo.Api::Model.Value";
    dispatch.stableTypeId = 7;
    dispatch.descriptorName = "__nsc_desc_value";
    LibraryMetadataDispatchInterface implementation;
    implementation.interfaceTypeKey = "type:Demo.Contracts::Readable";
    implementation.interfaceSignatures = {"Read()"};
    implementation.implementationSignatures = {"Read()"};
    implementation.calleeNames = {"__nsc_demo_read"};
    dispatch.interfaces.push_back(implementation);
    value.dispatchTables.push_back(dispatch);
    return value;
}
}

int main()
{
    using nativesharp::interop::LibraryApiDiffer;
    using nativesharp::interop::LibraryMetadataMember;
    using nativesharp::interop::LibraryMetadataType;
    nativesharp::tests::TestContext tests;

    const auto base = metadata();
    auto reordered = base;
    std::ranges::reverse(reordered.types);
    std::ranges::reverse(reordered.members);
    tests.expect(LibraryApiDiffer::sourceSurfaceManifest(base) ==
            LibraryApiDiffer::sourceSurfaceManifest(reordered) &&
            LibraryApiDiffer::abiSurfaceManifest(base) ==
            LibraryApiDiffer::abiSurfaceManifest(reordered),
        "canonical API manifests ignore metadata record order");

    auto spelling = base;
    spelling.types.front().fullyQualifiedName = "MODEL.value";
    tests.expect(LibraryApiDiffer::sourceFingerprint(base) ==
            LibraryApiDiffer::sourceFingerprint(spelling),
        "canonical identity, not display spelling, owns a public type");

    auto privateChange = base;
    LibraryMetadataType privateType = privateChange.types.front();
    privateType.accessibility = 0;
    privateType.identityKey = "Demo.Api::Private.Hidden";
    privateType.size = 4096;
    privateChange.types.push_back(privateType);
    LibraryMetadataMember privateMember = privateChange.members.front();
    privateMember.accessibility = 0;
    privateMember.signatureKey = "Hidden()";
    privateMember.mangledName = "__private_changed";
    privateChange.members.push_back(privateMember);
    tests.expect(LibraryApiDiffer::sourceFingerprint(base) ==
            LibraryApiDiffer::sourceFingerprint(privateChange) &&
            LibraryApiDiffer::abiSurfaceFingerprint(base) ==
            LibraryApiDiffer::abiSurfaceFingerprint(privateChange),
        "private declarations do not perturb the public source or ABI surface");

    auto docsOnly = base;
    docsOnly.genericTemplates.front().sourceText =
        "// changed docs\n public  struct Box < T > { public T Get ( ) { return value ; } }";
    tests.expect(LibraryApiDiffer::sourceFingerprint(base) ==
            LibraryApiDiffer::sourceFingerprint(docsOnly),
        "generic source trivia and documentation do not perturb the semantic API");

    auto genericBehavior = docsOnly;
    genericBehavior.genericTemplates.front().sourceText =
        "public struct Box<T> { public T Get() { return other; } }";
    tests.expect(LibraryApiDiffer::sourceFingerprint(base) !=
            LibraryApiDiffer::sourceFingerprint(genericBehavior),
        "a reusable generic body change perturbs its public compilation contract");

    auto effect = base;
    effect.members.front().flags &= ~nativesharp::interop::kMetadataMemberNoHeap;
    tests.expect(LibraryApiDiffer::sourceFingerprint(base) !=
            LibraryApiDiffer::sourceFingerprint(effect) &&
            LibraryApiDiffer::abiSurfaceFingerprint(base) ==
            LibraryApiDiffer::abiSurfaceFingerprint(effect),
        "callable effects are source contracts without inventing physical ABI changes");

    auto layout = base;
    layout.types.front().size = 16;
    tests.expect(LibraryApiDiffer::sourceFingerprint(base) ==
            LibraryApiDiffer::sourceFingerprint(layout) &&
            LibraryApiDiffer::abiSurfaceFingerprint(base) !=
            LibraryApiDiffer::abiSurfaceFingerprint(layout),
        "contractual layout participates only in the ABI surface");

    auto implementation = base;
    implementation.dispatchTables.front().interfaces.front().calleeNames.front() =
        "__nsc_demo_read_v2";
    tests.expect(LibraryApiDiffer::abiSurfaceFingerprint(base) !=
            LibraryApiDiffer::abiSurfaceFingerprint(implementation),
        "published interface implementation dispatch participates in the ABI surface");

    tests.expect(LibraryApiDiffer::sourceSurfaceManifest(base).starts_with(
            "25:NativeSharp.source-api.v2;") &&
            LibraryApiDiffer::abiSurfaceManifest(base).starts_with(
                "26:NativeSharp.abi-surface.v2;"),
        "tool-consumable manifests carry an explicit versioned schema");
    return tests.result();
}
