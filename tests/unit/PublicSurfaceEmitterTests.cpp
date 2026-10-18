#include "PublicSurfaceEmitter.h"

#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <string>
#include <utility>

namespace
{
nativesharp::interop::LibraryMetadataType publicType(
    nativesharp::interop::LibraryMetadataTypeKind kind,
    std::string identity,
    std::string name)
{
    nativesharp::interop::LibraryMetadataType type;
    type.kind = kind;
    type.accessibility = 3U;
    type.identityKey = std::move(identity);
    type.fullyQualifiedName = std::move(name);
    return type;
}
}

int main()
{
    using nativesharp::driver::PublicSurfaceEmitter;
    using nativesharp::interop::LibraryMetadata;
    using nativesharp::interop::LibraryMetadataTypeKind;
    using nativesharp::tests::TestContext;

    TestContext tests;
    LibraryMetadata metadata;
    metadata.moduleName = "Golden";
    metadata.types.push_back(publicType(
        LibraryMetadataTypeKind::Class,
        "Golden::Demo.Outer",
        "Demo.Outer"));
    metadata.types.push_back(publicType(
        LibraryMetadataTypeKind::Struct,
        "Golden::Demo.Outer+Inner",
        "Demo.Outer+Inner"));
    metadata.types.back().size = 1U;

    std::string source;
    std::string error;
    const bool built = PublicSurfaceEmitter::build(metadata, source, error);
    const std::string expected =
        "namespace Demo\n"
        "{\n"
        "public class Outer\n"
        "{\n"
        "public pack(1) align(1) size(1) struct Inner\n"
        "{\n"
        "}\n"
        "}\n"
        "}\n";
    tests.expect(built && error.empty(),
        "public surface golden metadata renders without a diagnostic");
    tests.expect(source == expected,
        "public surface rendering preserves nested declaration order byte for byte");

    error.clear();
    const bool verified = PublicSurfaceEmitter::verify(metadata, error);
    tests.expect(verified && error.empty(),
        "the golden public surface remains valid NativeSharp source: " + error);
    return tests.result();
}
