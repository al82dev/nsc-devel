#include "nativesharp/Interop/CppBinaryDependencies.h"

#include <filesystem>
#include <exception>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace nativesharp::interop
{
namespace
{
bool sameName(const std::filesystem::path& left, const std::filesystem::path& right)
{
#ifdef _WIN32
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
    return left == right;
#endif
}

bool readArtifact(const std::filesystem::path& path, CppBinaryArtifact& artifact, std::string& error)
{
    std::error_code ec;
    if (!path.is_absolute() || path.filename().empty() || !std::filesystem::is_regular_file(path, ec) || ec)
    {
        error = "C/C++ binary dependency is not an absolute regular file: " + path.generic_string();
        return false;
    }
    const auto name = path.filename().generic_u8string();
    artifact.fileName.assign(reinterpret_cast<const char*>(name.data()), name.size());
    return basic::sha256File(path, artifact.digest, artifact.size, error);
}

bool resolveArtifact(const CppBinaryArtifact& expected, const std::filesystem::path& directory,
    std::filesystem::path& resolved, std::string& error)
{
    std::filesystem::path leaf;
    try { leaf = std::filesystem::path(std::u8string(expected.fileName.begin(), expected.fileName.end())); }
    catch (const std::exception&)
    {
        error = "invalid UTF-8 in C/C++ binary dependency file name";
        return false;
    }
    if (expected.fileName.empty() || expected.fileName.find('\0') != std::string::npos ||
        leaf != leaf.filename() || leaf == "." || leaf == ".." || leaf.has_root_name())
    {
        error = "invalid C/C++ binary dependency file name: " + expected.fileName;
        return false;
    }
    const auto path = directory / leaf;
    CppBinaryArtifact actual;
    if (!readArtifact(path, actual, error)) return false;
    if (actual.size != expected.size || actual.digest != expected.digest)
    {
        error = "C/C++ binary dependency identity mismatch: " + expected.fileName;
        return false;
    }
    resolved = path;
    return true;
}
}

bool collectCppBinaryDependencies(std::span<const compilation::CppLibraryReference> references,
    std::vector<CppBinaryDependency>& output, std::string& error)
{
    std::vector<CppBinaryDependency> result;
    std::vector<std::pair<std::filesystem::path, CppBinaryArtifact>> names;
    const auto registerArtifact = [&](const std::filesystem::path& path, CppBinaryArtifact& artifact) {
        if (!readArtifact(path, artifact, error)) return false;
        for (const auto& [name, previous] : names)
            if (sameName(name, path.filename()) &&
                (previous.size != artifact.size || previous.digest != artifact.digest))
            {
                error = "C/C++ binary dependencies have conflicting file name: " + artifact.fileName;
                return false;
            }
        names.emplace_back(path.filename(), artifact);
        return true;
    };
    for (const auto& reference : references)
    {
        if (reference.libraryPath.empty())
        {
            if (reference.runtimePath.empty()) continue;
            error = "C/C++ runtime dependency has no link library";
            return false;
        }
        CppBinaryDependency dependency;
        if (!registerArtifact(reference.libraryPath, dependency.library)) return false;
        if (!reference.runtimePath.empty() && !registerArtifact(reference.runtimePath, dependency.runtime)) return false;
        result.push_back(std::move(dependency));
    }
    output = std::move(result);
    error.clear();
    return true;
}

bool resolveCppBinaryDependencies(std::span<const CppBinaryDependency> dependencies,
    const std::filesystem::path& directory, std::vector<ResolvedCppBinaryDependency>& output,
    std::string& error)
{
    std::error_code ec;
    if (!directory.is_absolute() || !std::filesystem::is_directory(directory, ec) || ec)
    {
        error = "C/C++ binary dependency directory is not an absolute directory";
        return false;
    }
    std::vector<ResolvedCppBinaryDependency> result;
    result.reserve(dependencies.size());
    for (const auto& dependency : dependencies)
    {
        ResolvedCppBinaryDependency resolved;
        if (!resolveArtifact(dependency.library, directory, resolved.libraryPath, error)) return false;
        if (!dependency.runtime.fileName.empty())
        {
            if (!resolveArtifact(dependency.runtime, directory, resolved.runtimePath, error)) return false;
        }
        else if (dependency.runtime.size != 0 || dependency.runtime.digest != basic::Sha256Digest{})
        {
            error = "C/C++ binary dependency has an incomplete runtime identity";
            return false;
        }
        result.push_back(std::move(resolved));
    }
    output = std::move(result);
    error.clear();
    return true;
}

bool collectCppBinaryMetadataDependencies(std::span<const compilation::CppLibraryReference> references,
    std::vector<LibraryMetadataCppBinaryDependency>& output, std::string& error)
{
    std::vector<CppBinaryDependency> dependencies;
    if (!collectCppBinaryDependencies(references, dependencies, error)) return false;
    std::vector<LibraryMetadataCppBinaryDependency> result;
    result.reserve(dependencies.size());
    for (const auto& dependency : dependencies)
    {
        if (dependency.library.size > std::numeric_limits<std::uint64_t>::max() ||
            dependency.runtime.size > std::numeric_limits<std::uint64_t>::max())
        {
            error = "C/C++ binary dependency exceeds NSMETA size limit";
            return false;
        }
        result.push_back({dependency.library.fileName, static_cast<std::uint64_t>(dependency.library.size),
                          dependency.library.digest, dependency.runtime.fileName,
                          static_cast<std::uint64_t>(dependency.runtime.size), dependency.runtime.digest});
    }
    output = std::move(result);
    error.clear();
    return true;
}

bool resolveCppBinaryMetadataDependencies(std::span<const LibraryMetadataCppBinaryDependency> dependencies,
    const std::filesystem::path& directory, std::vector<ResolvedCppBinaryDependency>& output,
    std::string& error)
{
    std::vector<CppBinaryDependency> records;
    records.reserve(dependencies.size());
    for (const auto& dependency : dependencies)
    {
        if (dependency.librarySize > std::numeric_limits<std::uintmax_t>::max() ||
            dependency.runtimeSize > std::numeric_limits<std::uintmax_t>::max())
        {
            error = "C/C++ binary dependency exceeds host size limit";
            return false;
        }
        records.push_back({{dependency.libraryFileName, static_cast<std::uintmax_t>(dependency.librarySize),
                            dependency.libraryDigest},
                           {dependency.runtimeFileName, static_cast<std::uintmax_t>(dependency.runtimeSize),
                            dependency.runtimeDigest}});
    }
    return resolveCppBinaryDependencies(records, directory, output, error);
}

bool collectCppGraphBinaryMetadataDependencies(const compilation::ProjectGraph& graph,
    std::vector<LibraryMetadataCppBinaryDependency>& output, std::string& error)
{
    std::vector<compilation::CppLibraryReference> references;
    for (const auto& project : graph.projects())
        for (const auto& reference : project.cppLibraryReferences())
            references.push_back(reference);
    return collectCppBinaryMetadataDependencies(references, output, error);
}
}
