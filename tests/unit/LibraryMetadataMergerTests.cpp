#include "nativesharp/Basic/Version.h"
#include "nativesharp/Interop/LibraryMetadataMerger.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace
{
using nativesharp::interop::LibraryMetadata;
using nativesharp::interop::LibraryMetadataBinaryDependency;
using nativesharp::interop::LibraryMetadataDispatch;
using nativesharp::interop::LibraryMetadataDispatchInterface;
using nativesharp::interop::LibraryMetadataExport;
using nativesharp::interop::LibraryMetadataGenericSpecialization;
using nativesharp::interop::LibraryMetadataGenericTemplate;
using nativesharp::interop::LibraryMetadataMember;
using nativesharp::interop::LibraryMetadataMerger;
using nativesharp::interop::LibraryMetadataPlatformImport;
using nativesharp::interop::LibraryMetadataType;
using nativesharp::interop::LibraryMetadataTypeKind;
using nativesharp::tests::TestContext;

LibraryMetadata header()
{
    LibraryMetadata metadata;
    metadata.producerVersion = std::string(nativesharp::kCompilerVersion);
    metadata.moduleName = "Merged";
    metadata.targetTriple = "x86_64-pc-windows-msvc";
    return metadata;
}

LibraryMetadataType type(std::string identity, std::uint64_t size)
{
    LibraryMetadataType record;
    record.kind = LibraryMetadataTypeKind::Struct;
    record.accessibility = 3;
    record.identityKey = std::move(identity);
    record.fullyQualifiedName = record.identityKey;
    record.size = size;
    record.alignment = size;
    record.interfaceTypeKeys = {"type:Zed", "type:Alpha", "type:Zed"};
    return record;
}

LibraryMetadataMember member(std::string declaringType, std::string name)
{
    LibraryMetadataMember record;
    record.kind = 1;
    record.accessibility = 3;
    record.declaringTypeKey = std::move(declaringType);
    record.name = std::move(name);
    record.signatureKey = record.name + "()";
    record.declaredTypeKey = "builtin:void";
    return record;
}

std::vector<unsigned char> readBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void testOrderIndependentMerge(TestContext& tests, const std::filesystem::path& root)
{
    LibraryMetadata left = header();
    left.types = {type("Merged::Zulu", 8), type("Merged::Alpha", 4)};
    left.members = {member("type:Merged::Zulu", "Run")};
    LibraryMetadataGenericTemplate generic;
    generic.canonicalKey = "Merged::Cache`1";
    generic.dependencies = {"Merged::Zulu`1", "Merged::Alpha`1", "Merged::Zulu`1"};
    left.genericTemplates.push_back(generic);
    LibraryMetadataDispatch dispatch;
    dispatch.typeKey = "type:Merged::Zulu";
    LibraryMetadataDispatchInterface zed;
    zed.interfaceTypeKey = "type:Merged::IZed";
    LibraryMetadataDispatchInterface alpha;
    alpha.interfaceTypeKey = "type:Merged::IAlpha";
    dispatch.interfaces = {zed, alpha};
    left.dispatchTables.push_back(dispatch);
    left.externalTypeKeys = {"type:Foreign::Zed", "type:Foreign::Alpha"};
    LibraryMetadataBinaryDependency dependency;
    dependency.moduleName = "Foundation";
    dependency.libraryFileName = "Foundation.lib";
    dependency.metadataFileName = "Foundation.nsmeta";
    dependency.abiFingerprint[0] = 1U;
    left.binaryDependencies.push_back(dependency);
    left.platformImports.push_back({"kernel32.dll", "GetCurrentProcessId"});

    LibraryMetadata right = header();
    right.types = {type("Merged::Alpha", 4), type("Merged::Middle", 2)};
    right.members = {member("type:Merged::Alpha", "Read")};
    LibraryMetadataGenericSpecialization specialization;
    specialization.templateKey = "Merged::Cache`1";
    specialization.specializationKey = "generic:Merged::Cache<int>";
    specialization.specializedName = "CacheInt";
    specialization.typeArguments = {"builtin:int"};
    right.genericSpecializations.push_back(specialization);
    LibraryMetadataExport exportRecord;
    exportRecord.cAbiName = "merged_read";
    exportRecord.declaringTypeKey = "type:Merged::Alpha";
    exportRecord.returnTypeKey = "builtin:int";
    right.exports.push_back(exportRecord);
    right.externalTypeKeys = {"type:Foreign::Alpha", "type:Foreign::Middle"};
    right.binaryDependencies.push_back(dependency);
    right.platformImports.push_back({"kernel32.dll", "GetCurrentProcessId"});
    right.platformImports.push_back({"user32.dll", "MessageBeep"});

    LibraryMetadataMerger merger;
    LibraryMetadata forward;
    LibraryMetadata reverse;
    std::string error;
    const std::vector<LibraryMetadata> forwardFragments = {left, right};
    const std::vector<LibraryMetadata> reverseFragments = {right, left};
    tests.expect(merger.merge(forwardFragments, forward, error),
        "worker fragments merge in forward completion order");
    error.clear();
    tests.expect(merger.merge(reverseFragments, reverse, error),
        "worker fragments merge in reverse completion order");
    tests.expect(forward.types.size() == 3 && reverse.types.size() == 3,
        "exact duplicate records collapse across fragments");
    tests.expect(forward.types[0].identityKey == "Merged::Alpha" &&
            forward.types[1].identityKey == "Merged::Middle" &&
            forward.types[2].identityKey == "Merged::Zulu",
        "types publish by canonical identity rather than arrival order");
    tests.expect(forward.members.size() == 2 &&
            forward.members[0].declaringTypeKey == "type:Merged::Alpha",
        "members publish by declaring type and signature");
    tests.expect(forward.types[0].interfaceTypeKeys ==
            std::vector<std::string>({"type:Alpha", "type:Zed"}),
        "unordered nested type sets are sorted and deduplicated");
    tests.expect(forward.genericTemplates[0].dependencies ==
            std::vector<std::string>({"Merged::Alpha`1", "Merged::Zulu`1"}),
        "generic dependency sets have canonical order");
    tests.expect(forward.dispatchTables[0].interfaces[0].interfaceTypeKey ==
            "type:Merged::IAlpha",
        "dispatch interfaces have canonical order");
    tests.expect(forward.externalTypeKeys ==
            std::vector<std::string>({"type:Foreign::Alpha", "type:Foreign::Middle",
                "type:Foreign::Zed"}),
        "external dependency union is sorted and duplicate-free");
    tests.expect(forward.genericSpecializations.size() == 1 &&
            forward.exports.size() == 1,
        "specializations and C exports survive fragmented collection");
    tests.expect(forward.binaryDependencies.size() == 1 &&
            forward.binaryDependencies[0].moduleName == "Foundation" &&
            forward.platformImports.size() == 2,
        "binary closure and private platform imports merge canonically without duplicates");

    nativesharp::interop::LibraryMetadataCodec codec;
    const auto forwardPath = root / "forward.nsmeta";
    const auto reversePath = root / "reverse.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(forward, forwardPath, error),
        "forward merged model serializes");
    error.clear();
    tests.expect(codec.writeModel(reverse, reversePath, error),
        "reverse merged model serializes");
    tests.expect(readBytes(forwardPath) == readBytes(reversePath),
        "fragment completion order produces byte-identical NSMETA");
}

void testDeterministicRejection(TestContext& tests)
{
    LibraryMetadata first = header();
    first.types.push_back(type("Merged::Value", 4));
    LibraryMetadata conflict = header();
    conflict.types.push_back(type("Merged::Value", 8));

    LibraryMetadataMerger merger;
    LibraryMetadata merged;
    std::string error;
    const std::vector<LibraryMetadata> fragments = {first, conflict};
    tests.expect(!merger.merge(fragments, merged, error) &&
            error == "NSMETA merge conflict for type 'Merged::Value'",
        "conflicting duplicate identities fail with a stable diagnostic");

    conflict = header();
    conflict.moduleName = "Other";
    error.clear();
    const std::vector<LibraryMetadata> incompatible = {first, conflict};
    tests.expect(!merger.merge(incompatible, merged, error) &&
            error.find("fragment 1") != std::string::npos &&
            error.find("module 'Merged'") != std::string::npos,
        "incompatible fragment headers are rejected before publication");

    error.clear();
    tests.expect(!merger.merge({}, merged, error) &&
            error == "NSMETA merge requires at least one fragment",
        "an empty merge cannot publish a metadata file");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_library_metadata_merger_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    testOrderIndependentMerge(tests, root);
    testDeterministicRejection(tests);
    return tests.resultKeeping(root);
}
