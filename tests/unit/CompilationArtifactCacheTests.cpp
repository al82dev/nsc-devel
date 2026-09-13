#include "nativesharp/LLVM/CompilationArtifactCache.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
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

    CompilationArtifactCache cache;
    cache.setRoot(root / "cache");
    cache.setToolchainIdentity("toolchain-A");
    const std::string key = cache.objectKey(
        "define i32 @main() { ret i32 0 }", "x86_64-pc-windows-msvc",
        "e-m:w-p270:32:32", LlvmOptimizationLevel::O2,
        DebugInformationKind::None);
    tests.expect(key.size() == 64, "object key is a canonical SHA-256 identity");
    tests.expect(key == cache.objectKey(
            "define i32 @main() { ret i32 0 }", "x86_64-pc-windows-msvc",
            "e-m:w-p270:32:32", LlvmOptimizationLevel::O2,
            DebugInformationKind::None),
        "identical optimized content and policy produce one key");
    tests.expect(key != cache.objectKey(
            "define i32 @main() { ret i32 1 }", "x86_64-pc-windows-msvc",
            "e-m:w-p270:32:32", LlvmOptimizationLevel::O2,
            DebugInformationKind::None),
        "optimized LLVM content participates in invalidation");
    tests.expect(key != cache.objectKey(
            "define i32 @main() { ret i32 0 }", "x86_64-pc-windows-msvc",
            "e-m:w-p270:32:32", LlvmOptimizationLevel::O0,
            DebugInformationKind::None),
        "optimization level participates in invalidation");
    tests.expect(key != cache.objectKey(
            "define i32 @main() { ret i32 0 }", "x86_64-pc-windows-msvc",
            "e-m:w-p270:32:32", LlvmOptimizationLevel::O2,
            DebugInformationKind::Full),
        "debug policy participates in invalidation");

    CompilationArtifactCache otherToolchain = cache;
    otherToolchain.setToolchainIdentity("toolchain-B");
    tests.expect(key != otherToolchain.objectKey(
            "define i32 @main() { ret i32 0 }", "x86_64-pc-windows-msvc",
            "e-m:w-p270:32:32", LlvmOptimizationLevel::O2,
            DebugInformationKind::None),
        "compiler build identity participates in invalidation");

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

    const auto cachedObject = cache.root() / "objects" / key.substr(0, 2) / (key + ".obj");
    writeBytes(cachedObject, "corrupt");
    hit = true;
    error.clear();
    tests.expect(cache.restoreObject(key, restored, hit, error) && !hit && error.empty(),
        "digest mismatch is a safe cache miss rather than stale object reuse");

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
            CompilationArtifactCacheScope nested(otherToolchain);
            tests.expect(nativesharp::llvm_backend::activeCompilationArtifactCache() == &otherToolchain,
                "nested compiler invocations receive isolated cache policy");
        }
        tests.expect(nativesharp::llvm_backend::activeCompilationArtifactCache() == &cache,
            "nested scope restores the outer cache policy");
    }
    tests.expect(nativesharp::llvm_backend::activeCompilationArtifactCache() == nullptr,
        "cache scope restores the process-local default");

    return tests.resultKeeping(root);
}
