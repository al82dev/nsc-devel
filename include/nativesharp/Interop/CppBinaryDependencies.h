#pragma once

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Compilation/CppLibraryReference.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Interop/LibraryMetadataCppBinaryDependency.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::interop
{
struct CppBinaryArtifact final
{
    std::string fileName;
    std::uintmax_t size = 0;
    basic::Sha256Digest digest{};
};

struct CppBinaryDependency final
{
    CppBinaryArtifact library;
    CppBinaryArtifact runtime;
};

struct ResolvedCppBinaryDependency final
{
    std::filesystem::path libraryPath;
    std::filesystem::path runtimePath;
};

// Preserves link order. On failure, output remains unchanged.
bool collectCppBinaryDependencies(std::span<const compilation::CppLibraryReference> references,
    std::vector<CppBinaryDependency>& output, std::string& error);
// Resolves the recorded leaf names beside the producer's metadata, preserving link order.
// On failure, output remains unchanged.
bool resolveCppBinaryDependencies(std::span<const CppBinaryDependency> dependencies,
    const std::filesystem::path& directory, std::vector<ResolvedCppBinaryDependency>& output,
    std::string& error);
// Bridge the serialized catalog to the same verified collection and resolution paths.
bool collectCppBinaryMetadataDependencies(std::span<const compilation::CppLibraryReference> references,
    std::vector<LibraryMetadataCppBinaryDependency>& output, std::string& error);
// Collect in ProjectGraph::projects() and per-project declaration order.
bool collectCppGraphBinaryMetadataDependencies(const compilation::ProjectGraph& graph,
    std::vector<LibraryMetadataCppBinaryDependency>& output, std::string& error);
bool resolveCppBinaryMetadataDependencies(std::span<const LibraryMetadataCppBinaryDependency> dependencies,
    const std::filesystem::path& directory, std::vector<ResolvedCppBinaryDependency>& output,
    std::string& error);
}
