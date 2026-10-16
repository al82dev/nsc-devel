#include "LlvmTestSupport.h"

#include "nativesharp/LLVM/CompilationArtifactCache.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace nsc::suite
{
namespace
{
using namespace llvm_support;
using namespace nativesharp::llvm_backend;

void coffContract(TestContext& context, std::size_t variant)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("nsc-host-llvm-coff-" + std::to_string(variant) + ".obj");
    std::array<unsigned char, 20> bytes{};
    bytes[0] = 0x64;
    bytes[1] = 0x86;
    bytes[2] = 1;
    if (variant == 4) bytes[0] = 0x4c;
    if (variant == 5) bytes[2] = 0;
    if (variant == 6) bytes[16] = 4;
    if (variant != 3)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        const std::size_t size = variant == 7 ? 12 : bytes.size();
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(size));
    }
    CoffObjectVerifier verifier;
    const bool valid = verifier.verify(path);
    const bool expected = variant == 1 || variant == 2 || variant == 8;
    context.expect(valid == expected, "COFF verifier must enforce header, machine, section and optional-header invariants");
    if (expected)
        context.expect(verifier.info().machine == 0x8664 && verifier.info().sectionCount == 1 &&
            verifier.info().optionalHeaderSize == 0, "accepted COFF header fields must be published exactly");
    else
        context.expect(!verifier.error().empty(), "rejected COFF object must retain a diagnostic");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

void debugKindContract(TestContext& context, std::size_t variant)
{
    context.expect(debugInformationKindName(DebugInformationKind::None) == "none", "debug none name must be exact");
    context.expect(debugInformationKindName(DebugInformationKind::LineTablesOnly) == "line-tables-only",
        "line-table debug name must be exact");
    context.expect(debugInformationKindName(DebugInformationKind::Full) == "full", "full debug name must be exact");
    if (variant >= 3)
        context.expect(debugInformationKindName(static_cast<DebugInformationKind>(255)).empty(),
            "unknown debug kind must have no invented name");
}

void prefixMapContract(TestContext& context, std::size_t variant)
{
    DebugPrefixMap map{std::filesystem::path("A:/sources"), std::filesystem::path("/src")};
    context.expect(map.sourcePrefix.generic_string() == "A:/sources", "source prefix must preserve portable spelling");
    context.expect(map.replacementPrefix.generic_string() == "/src", "replacement prefix must preserve identity");
    if (variant == 8)
    {
        std::vector<DebugPrefixMap> maps(2048, map);
        context.expect(maps.front().sourcePrefix == maps.back().sourcePrefix,
            "prefix-map value model must remain deterministic at scale");
    }
}

void cacheReasonContract(TestContext& context, std::size_t variant)
{
    context.expect(std::string(cacheMissReasonName(CacheMissReason::Hit)) == "hit",
        "cache miss reason name for Hit must be exact");
    context.expect(std::string(cacheMissReasonName(CacheMissReason::Disabled)) == "disabled",
        "cache miss reason name for Disabled must be exact");
    context.expect(std::string(cacheMissReasonName(CacheMissReason::KeyMismatch)) == "key-mismatch",
        "cache miss reason name for KeyMismatch must be exact");
    context.expect(std::string(cacheMissReasonName(CacheMissReason::Cold)) == "cold",
        "cache miss reason name for Cold must be exact");
    context.expect(std::string(cacheMissReasonName(CacheMissReason::IncompletePublication)) ==
            "incomplete-publication",
        "cache miss reason name for IncompletePublication must be exact");
    context.expect(std::string(cacheMissReasonName(CacheMissReason::Corruption)) == "corruption",
        "cache miss reason name for Corruption must be exact");

    const std::filesystem::path root = std::filesystem::current_path() /
        ("nsc-host-llvm-cache-reason-" + std::to_string(variant));
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    filesystemError.clear();
    std::filesystem::create_directories(root, filesystemError);
    context.expect(!filesystemError && std::filesystem::is_directory(root, filesystemError),
        "cache reason fixture directory must be creatable");
    const std::string key(64, 'a');
    const std::filesystem::path destination = root / "restored.obj";
    bool hit = false;
    std::string error;
    CacheMissReason reason = CacheMissReason::Hit;

    CompilationArtifactCache disabled;
    context.expect(disabled.restoreObject(key, destination, hit, error, &reason) && !hit &&
            reason == CacheMissReason::Disabled,
        "a disabled artifact cache must report a Disabled miss");

    CompilationArtifactCache cache;
    cache.setRoot(root);
    reason = CacheMissReason::Hit;
    context.expect(cache.restoreObject(key, destination, hit, error, &reason) && !hit &&
            reason == CacheMissReason::Cold,
        "an empty artifact cache must report a Cold miss");

    reason = CacheMissReason::Hit;
    context.expect(!cache.restoreObject("not-a-key", destination, hit, error, &reason) && !hit &&
            reason == CacheMissReason::KeyMismatch,
        "a non-canonical key must report a KeyMismatch miss");

    const std::filesystem::path source = root / "source.obj";
    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output << "canonical object bytes";
    }
    const bool stored = cache.storeObject(key, source, error);
    context.expect(stored && error.empty(),
        "artifact cache must store a canonical object entry; error=" + error);

    const std::filesystem::path cachedObject = root / "objects" / key.substr(0, 2) / (key + ".obj");
    {
        std::ofstream output(cachedObject, std::ios::binary | std::ios::trunc);
        output << "corrupted bytes";
    }
    reason = CacheMissReason::Hit;
    context.expect(cache.restoreObject(key, destination, hit, error, &reason) && !hit &&
            reason == CacheMissReason::Corruption,
        "a digest-mismatched entry must be reported as Corruption and rebuilt");
    context.expect(!std::filesystem::exists(cachedObject, filesystemError) || filesystemError,
        "a corrupted entry must be quarantined after detection");

    std::filesystem::remove_all(root, filesystemError);
}
}

void registerLlvmFoundationModule(TestRegistry& registry)
{
    addLlvmContract(registry, 1, "integration", coffContract);
    addLlvmContract(registry, 7, "unit", debugKindContract);
    addLlvmContract(registry, 8, "unit", prefixMapContract);
    addLlvmContract(registry, 22, "unit", cacheReasonContract);
}
}
