#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/CodegenIdentity.h"
#include "nativesharp/LLVM/ProjectOopMirCacheIdentity.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <latch>
#include <string>
#include <thread>
#include <vector>

namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::SourceRange;
using nativesharp::llvm_backend::CompilationArtifactCache;
using nativesharp::llvm_backend::CompilationArtifactCacheScope;
using nativesharp::llvm_backend::DebugInformationKind;
using nativesharp::llvm_backend::LlvmOptimizationLevel;
using nativesharp::tests::TestContext;

void writeBytes(const std::filesystem::path& path, std::string_view bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::string readBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_compilation_artifact_cache_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    tests.expect(nativesharp::llvm_backend::defaultProjectArtifactCacheRoot(
            std::filesystem::path{"A:/work/Game/Game.nsproj"}) ==
            std::filesystem::path{"A:/work/Game/obj/nativesharp/artifact-cache"},
        "project cache root follows the deterministic obj/nativesharp convention");

    nativesharp::source::SourceManager debugSources;
    const FileId debugFile = debugSources.addBuffer(
        "A:/source/game.ns", "first line\nsecond line\n");
    const std::vector<SourceRange> debugRanges = {
        SourceRange(SourceLocation(debugFile, 0), SourceLocation(debugFile, 10))};
    const std::string lineTableIdentity =
        nativesharp::llvm_backend::projectOopLineTableSourceIdentity(
            debugSources, {}, debugRanges);
    tests.expect(lineTableIdentity ==
            nativesharp::llvm_backend::projectOopLineTableSourceIdentity(
                debugSources, {}, debugRanges),
        "identical line-table sources and ranges produce one identity");

    nativesharp::source::SourceManager changedContents;
    const FileId changedContentsFile = changedContents.addBuffer(
        "A:/source/game.ns", "first line\nchanged line\n");
    const std::vector<SourceRange> changedContentsRanges = {
        SourceRange(SourceLocation(changedContentsFile, 0),
            SourceLocation(changedContentsFile, 10))};
    tests.expect(lineTableIdentity !=
            nativesharp::llvm_backend::projectOopLineTableSourceIdentity(
                changedContents, {}, changedContentsRanges),
        "source content invalidates a line-table object identity");

    nativesharp::source::SourceManager changedPath;
    const FileId changedPathFile = changedPath.addBuffer(
        "A:/relocated/game.ns", "first line\nsecond line\n");
    const std::vector<SourceRange> changedPathRanges = {
        SourceRange(SourceLocation(changedPathFile, 0), SourceLocation(changedPathFile, 10))};
    tests.expect(lineTableIdentity !=
            nativesharp::llvm_backend::projectOopLineTableSourceIdentity(
                changedPath, {}, changedPathRanges),
        "source path invalidates a line-table object identity");

    const std::vector<SourceRange> changedRanges = {
        SourceRange(SourceLocation(debugFile, 1), SourceLocation(debugFile, 10))};
    tests.expect(lineTableIdentity !=
            nativesharp::llvm_backend::projectOopLineTableSourceIdentity(
                debugSources, {}, changedRanges),
        "source offsets invalidate a line-table object identity");

    const std::vector<nativesharp::llvm_backend::DebugPrefixMap> prefixMaps = {{
        std::filesystem::path{"A:/source"}, std::filesystem::path{"S:/mapped"}}};
    tests.expect(lineTableIdentity !=
            nativesharp::llvm_backend::projectOopLineTableSourceIdentity(
                debugSources, prefixMaps, debugRanges),
        "debug prefix maps invalidate a line-table object identity");

    CompilationArtifactCache cache;
    cache.setRoot(root / "cache");
    tests.expect(nativesharp::llvm_backend::kCodegenCacheIdentity.size() == 64 &&
            std::ranges::all_of(nativesharp::llvm_backend::kCodegenCacheIdentity, [](char value)
            {
                return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
            }),
        "generated codegen identity is one canonical SHA-256");
    cache.setCodegenIdentity("codegen-A");
    const std::string key = cache.objectKey(
        "define i32 @main() { ret i32 0 }", "x86_64-pc-windows-msvc",
        "e-m:w-p270:32:32", "fp=precise;lto=none", LlvmOptimizationLevel::O2,
        DebugInformationKind::None);
    tests.expect(key.size() == 64, "object key is a canonical SHA-256 identity");
    tests.expect(key == cache.objectKey(
            "define i32 @main() { ret i32 0 }", "x86_64-pc-windows-msvc",
            "e-m:w-p270:32:32", "fp=precise;lto=none", LlvmOptimizationLevel::O2,
            DebugInformationKind::None),
        "identical optimized content and policy produce one key");
    tests.expect(key != cache.objectKey(
            "define i32 @main() { ret i32 1 }", "x86_64-pc-windows-msvc",
            "e-m:w-p270:32:32", "fp=precise;lto=none", LlvmOptimizationLevel::O2,
            DebugInformationKind::None),
        "optimized LLVM content participates in invalidation");
    tests.expect(key != cache.objectKey(
            "define i32 @main() { ret i32 0 }", "x86_64-pc-windows-msvc",
            "e-m:w-p270:32:32", "fp=precise;lto=none", LlvmOptimizationLevel::O0,
            DebugInformationKind::None),
        "optimization level participates in invalidation");
    tests.expect(key != cache.objectKey(
            "define i32 @main() { ret i32 0 }", "x86_64-pc-windows-msvc",
            "e-m:w-p270:32:32", "fp=precise;lto=none", LlvmOptimizationLevel::O2,
            DebugInformationKind::Full),
        "debug policy participates in invalidation");

    CompilationArtifactCache otherCodegen = cache;
    otherCodegen.setCodegenIdentity("codegen-B");
    tests.expect(key != otherCodegen.objectKey(
            "define i32 @main() { ret i32 0 }", "x86_64-pc-windows-msvc",
            "e-m:w-p270:32:32", "fp=precise;lto=none", LlvmOptimizationLevel::O2,
            DebugInformationKind::None),
        "semantic codegen identity participates in invalidation");

    tests.expect(key != cache.objectKey(
            "define i32 @main() { ret i32 0 }", "x86_64-pc-windows-msvc",
            "e-m:w-p270:32:32", "fp=fast;lto=none", LlvmOptimizationLevel::O2,
            DebugInformationKind::None),
        "the complete backend policy participates in invalidation");
    const std::string inputKey = cache.inputObjectKey(
        "canonical-mir", "windows-x64-msvc", "fp=precise;lto=none",
        LlvmOptimizationLevel::O0, DebugInformationKind::None);
    tests.expect(inputKey != cache.inputObjectKey(
            "canonical-mir", "windows-x64-msvc", "fp=precise;lto=full",
            LlvmOptimizationLevel::O0, DebugInformationKind::None),
        "the pre-LLVM key cannot cross an LTO policy boundary");

    const auto source = root / "source.obj";
    const auto restored = root / "restored.obj";
    writeBytes(source, "verified-object-bytes");
    std::string error;
    tests.expect(cache.storeObject(key, source, error) && error.empty(),
        "verified object bytes store with an authenticated sidecar");
    bool hit = false;
    tests.expect(cache.restoreObject(key, restored, hit, error) && hit &&
            readBytes(restored) == "verified-object-bytes",
        "matching cache entry restores exact object bytes");

#if defined(_WIN32)
    std::filesystem::path deepCacheRoot = root;
    while ((deepCacheRoot / "objects" / key.substr(0, 2) /
            (key + ".obj.lock")).native().size() <= 280U)
        deepCacheRoot /= "deep-builder-case-01";
    CompilationArtifactCache deepCache = cache;
    deepCache.setRoot(deepCacheRoot);
    const auto deepRestored = root / "deep-restored.obj";
    error.clear();
    const bool deepStored = deepCache.storeObject(key, source, error);
    tests.expect(deepStored && error.empty(),
        "artifact cache stores through a Win32 lock path longer than MAX_PATH (" + error + ")");
    hit = false;
    error.clear();
    const bool deepRestoredSuccessfully =
        deepCache.restoreObject(key, deepRestored, hit, error);
    tests.expect(deepRestoredSuccessfully && hit &&
            readBytes(deepRestored) == "verified-object-bytes",
        "artifact cache restores through a Win32 lock path longer than MAX_PATH (" + error + ")");
#endif

    const auto cachedObject = cache.root() / "objects" / key.substr(0, 2) / (key + ".obj");
    writeBytes(cachedObject, "corrupt");
    hit = true;
    error.clear();
    tests.expect(cache.restoreObject(key, restored, hit, error) && !hit && error.empty(),
        "digest mismatch is a safe cache miss rather than stale object reuse");
    tests.expect(!std::filesystem::exists(cachedObject) &&
            std::distance(std::filesystem::directory_iterator(cache.root() / "quarantine"),
                std::filesystem::directory_iterator()) == 2,
        "a corrupt object and its metadata are quarantined away from future lookups");

    error.clear();
    tests.expect(cache.storeObject(key, source, error),
        "a later producer repairs an incomplete or corrupt cache entry");

    const auto concurrentSourceA = root / "concurrent-a.obj";
    const auto concurrentSourceB = root / "concurrent-b.obj";
    const auto concurrentRestored = root / "concurrent-restored.obj";
    writeBytes(concurrentSourceA, "same-deterministic-object");
    writeBytes(concurrentSourceB, "same-deterministic-object");
    const std::string concurrentKey = cache.inputObjectKey(
        "concurrent-mir", "windows-x64-msvc", "fp=precise;lto=none",
        LlvmOptimizationLevel::O0, DebugInformationKind::None);
    std::latch concurrentStart(1);
    bool storedA = false;
    bool storedB = false;
    std::string errorA;
    std::string errorB;
    std::thread writerA([&] {
        concurrentStart.wait();
        storedA = cache.storeObject(concurrentKey, concurrentSourceA, errorA);
    });
    std::thread writerB([&] {
        concurrentStart.wait();
        storedB = cache.storeObject(concurrentKey, concurrentSourceB, errorB);
    });
    concurrentStart.count_down();
    writerA.join();
    writerB.join();
    hit = false;
    error.clear();
    tests.expect(storedA && storedB && errorA.empty() && errorB.empty() &&
            cache.restoreObject(concurrentKey, concurrentRestored, hit, error) && hit &&
            readBytes(concurrentRestored) == "same-deterministic-object",
        "concurrent producers may compete for one key without publishing a partial object");

    std::size_t evictedEntries = 0;
    std::uintmax_t evictedBytes = 0;
    error.clear();
    tests.expect(cache.prune(0, evictedEntries, evictedBytes, error) && error.empty() &&
            evictedEntries == 2 && evictedBytes > 0,
        "a bounded cache evicts complete oldest entries down to its byte budget");
    hit = true;
    tests.expect(cache.restoreObject(concurrentKey, concurrentRestored, hit, error) && !hit,
        "an evicted key is a clean miss and never leaves a half-entry");

    hit = true;
    error.clear();
    tests.expect(!cache.restoreObject("not-a-key", restored, hit, error) && !hit &&
            error.find("canonical SHA-256") != std::string::npos,
        "non-canonical cache identities are rejected");

    tests.expect(nativesharp::llvm_backend::activeCompilationArtifactCache() == nullptr,
        "no cache leaks outside an invocation scope");
    {
        CompilationArtifactCacheScope scope(cache);
        tests.expect(nativesharp::llvm_backend::activeCompilationArtifactCache() == &cache,
            "invocation scope publishes the selected cache");
        {
            CompilationArtifactCacheScope nested(otherCodegen);
            tests.expect(nativesharp::llvm_backend::activeCompilationArtifactCache() == &otherCodegen,
                "nested compiler invocations receive isolated cache policy");
        }
        tests.expect(nativesharp::llvm_backend::activeCompilationArtifactCache() == &cache,
            "nested scope restores the outer cache policy");
    }
    tests.expect(nativesharp::llvm_backend::activeCompilationArtifactCache() == nullptr,
        "cache scope restores the process-local default");

    return tests.resultKeeping(root);
}
