#include "Internal/DriverInternal.h"
#include "nativesharp/Interop/CppBinaryDependencies.h"
#include "nativesharp/Basic/Sha256.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

std::filesystem::path pathFromUtf8(std::string_view value)
{
#if defined(_WIN32)
    if (value.empty())
        return {};
    const int required =
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0)
        return std::filesystem::path(std::string(value));
    std::wstring converted(static_cast<std::size_t>(required), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                              converted.data(), required) != required)
        return std::filesystem::path(std::string(value));
    return std::filesystem::path(converted);
#else
    const std::u8string encoded(reinterpret_cast<const char8_t *>(value.data()), value.size());
    return std::filesystem::path(encoded);
#endif
}

bool filesHaveEqualContents(const std::filesystem::path &left, const std::filesystem::path &right)
{
    std::error_code sizeError;
    const std::uintmax_t leftSize = std::filesystem::file_size(left, sizeError);
    if (sizeError)
        return false;
    const std::uintmax_t rightSize = std::filesystem::file_size(right, sizeError);
    if (sizeError || leftSize != rightSize)
        return false;

    std::ifstream leftStream(left, std::ios::binary);
    std::ifstream rightStream(right, std::ios::binary);
    std::array<char, 64 * 1024> leftBuffer{};
    std::array<char, 64 * 1024> rightBuffer{};
    while (leftStream && rightStream)
    {
        leftStream.read(leftBuffer.data(), static_cast<std::streamsize>(leftBuffer.size()));
        rightStream.read(rightBuffer.data(), static_cast<std::streamsize>(rightBuffer.size()));
        const std::streamsize count = leftStream.gcount();
        if (count != rightStream.gcount() ||
            !std::equal(leftBuffer.begin(), leftBuffer.begin() + count, rightBuffer.begin()))
        {
            return false;
        }
    }
    return leftStream.eof() && rightStream.eof();
}

bool collectCppRuntimeCandidates(const frontend::ProjectCompilation &compilation,
                                 std::vector<std::filesystem::path> &candidates,
                                 std::string &error)
{
    if (!compilation.projectGraph()) { error = "C/C++ runtime selection requires a project graph"; return false; }
    interop::LibraryMetadataCodec codec;
    std::vector<std::filesystem::path> result;
    for (const auto &project : compilation.projectGraph()->projects())
    {
        for (const auto &reference : project.cppLibraryReferences())
            if (!reference.runtimePath.empty()) result.push_back(reference.runtimePath);
        for (const auto &reference : project.libraryReferences())
        {
            const auto metadata = compilation.libraryMetadataRepository().decode(
                reference.metadataPath,
                [&codec](std::span<const std::uint8_t> bytes, interop::LibraryMetadata &model,
                         std::string &why) { return codec.read(bytes, model, why); }, error);
            if (!metadata) return false;
            if (metadata->cppBinaryDependencies.empty()) continue;
            std::vector<interop::ResolvedCppBinaryDependency> dependencies;
            if (!interop::resolveCppBinaryMetadataDependencies(metadata->cppBinaryDependencies,
                    reference.metadataPath.parent_path(), dependencies, error)) return false;
            for (const auto &dependency : dependencies)
                if (!dependency.runtimePath.empty()) result.push_back(dependency.runtimePath);
        }
    }
    candidates = std::move(result);
    return true;
}

bool appendReferencedCppRuntimes(const frontend::ProjectCompilation &compilation,
                                 const linker::PeExecutableInfo &image,
                                 std::vector<std::filesystem::path> &runtimeLibraries,
                                 std::vector<std::filesystem::path> &dynamicallyLoadedRuntimes,
                                 std::string &error)
{
    std::vector<std::filesystem::path> candidates;
    if (!collectCppRuntimeCandidates(compilation, candidates, error)) return false;
    auto selected = runtimeLibraries;
    std::unordered_set<std::string> selectedPaths;
    for (const auto &path : selected) selectedPaths.insert(foldedPathKey(path));
    std::unordered_set<std::string> importedNames;
    std::unordered_set<std::string> dynamicIdentities;
    if (const auto *mir = compilation.oopMirModel())
    {
        const auto fields = mir->staticFields();
        for (const auto &field : fields)
        {
            if (!field.isCppExternal || !field.cppThreadLocal || !field.cppDllImport ||
                field.cppTlsRuntimeIdentity.empty()) continue;
            const bool collision = std::any_of(fields.begin(), fields.end(), [&](const auto &other) {
                return other.isCppExternal && other.cppThreadLocal && other.cppDllImport &&
                    other.storageSymbol == field.storageSymbol && !other.cppTlsRuntimeIdentity.empty() &&
                    other.cppTlsRuntimeIdentity != field.cppTlsRuntimeIdentity;
            });
            if (collision) dynamicIdentities.insert(field.cppTlsRuntimeIdentity);
        }
    }
    std::unordered_set<std::string> matchedIdentities;
    std::vector<std::filesystem::path> dynamicPaths;
    for (const auto &candidate : candidates)
    {
        if (dynamicIdentities.empty()) break;
        basic::Sha256Digest digest{};
        std::uintmax_t size = 0;
        if (!basic::sha256File(candidate, digest, size, error)) return false;
        const auto identity = candidate.filename().string() + ":" + basic::sha256Hex(digest);
        if (!dynamicIdentities.contains(identity)) continue;
        matchedIdentities.insert(identity);
        if (selectedPaths.insert(foldedPathKey(candidate)).second) selected.push_back(candidate);
        dynamicPaths.push_back(candidate);
    }
    if (matchedIdentities.size() != dynamicIdentities.size())
    {
        error = "a dynamically resolved C TLS runtime is missing from the declared dependencies";
        return false;
    }
    for (const auto &name : image.importedLibraries) importedNames.insert(foldedName(name));
    std::size_t scanned = 0;
    bool changed = true;
    while (changed)
    {
        changed = false;
        while (scanned < selected.size())
        {
            linker::PeExecutableVerifier verifier;
            if (!verifier.verifyDynamicLibrary(selected[scanned]))
            {
                error = "runtime dependency '" + selected[scanned].string() + "' is not a valid AMD64 DLL: " + verifier.error();
                return false;
            }
            for (const auto &name : verifier.info().importedLibraries)
                importedNames.insert(foldedName(name));
            ++scanned;
        }
        for (const auto &candidate : candidates)
            if (importedNames.contains(foldedName(candidate.filename().string())) &&
                selectedPaths.insert(foldedPathKey(candidate)).second)
            {
                selected.push_back(candidate);
                changed = true;
            }
    }
    runtimeLibraries = std::move(selected);
    dynamicallyLoadedRuntimes = std::move(dynamicPaths);
    return true;
}

bool validateCppRuntimeDestinations(const frontend::ProjectCompilation &compilation,
                                    std::span<const std::filesystem::path> runtimeLibraries,
                                    const std::filesystem::path &destinationDirectory,
                                    std::string &error)
{
    std::vector<std::filesystem::path> candidates;
    if (!collectCppRuntimeCandidates(compilation, candidates, error)) return false;
    std::unordered_set<std::string> selected;
    for (const auto &path : runtimeLibraries) selected.insert(foldedPathKey(path));
    for (const auto &source : candidates)
        {
            if (source.empty() || !selected.contains(foldedPathKey(source))) continue;
            const auto destination = destinationDirectory / source.filename();
            std::error_code ec;
            const bool exists = std::filesystem::exists(destination, ec);
            if (ec)
            {
                error = "cannot inspect C/C++ runtime destination '" + destination.string() + "': " + ec.message();
                return false;
            }
            if (exists && !filesHaveEqualContents(source, destination))
            {
                error = "runtime package destination already contains a different '" + destination.filename().string() + "': '" + destination.string() + "'";
                return false;
            }
        }
    return true;
}

bool validatePackagedCppRuntimeDestinations(const frontend::ProjectCompilation &compilation,
                                            std::span<const std::filesystem::path> runtimeLibraries,
                                            std::span<const std::filesystem::path> stagedRuntimes,
                                            const std::filesystem::path &destinationDirectory,
                                            bool &protectExisting,
                                            std::string &error)
{
    std::vector<std::filesystem::path> candidates;
    if (!collectCppRuntimeCandidates(compilation, candidates, error)) return false;
    std::unordered_set<std::string> selected;
    for (const auto &path : runtimeLibraries) selected.insert(foldedPathKey(path));
    bool hasCppRuntime = false;
    for (const auto &candidate : candidates)
        hasCppRuntime |= selected.contains(foldedPathKey(candidate));
    protectExisting = hasCppRuntime;
    if (!hasCppRuntime) return true;
    for (const auto &source : stagedRuntimes)
    {
        const auto destination = destinationDirectory / source.filename();
        std::error_code ec;
        const bool exists = std::filesystem::exists(destination, ec);
        if (ec)
        {
            error = "cannot inspect runtime destination '" + destination.string() + "': " + ec.message();
            return false;
        }
        if (exists && !filesHaveEqualContents(source, destination))
        {
            error = "runtime package destination already contains a different '" + destination.filename().string() + "': '" + destination.string() + "'";
            return false;
        }
    }
    return true;
}

bool packageRuntimeClosure(std::span<const std::filesystem::path> runtimeLibraries,
                           const std::filesystem::path &destination, std::vector<std::filesystem::path> &copiedFiles,
                           std::string &error)
{
    // Packaging is deliberately rooted in explicit LibraryReference runtimes.  A
    // dependency is followed only when the PE names it and a sibling DLL with that
    // exact name exists; no PATH lookup or directory scan is performed.
    std::vector<std::filesystem::path> closure;
    std::unordered_map<std::string, std::filesystem::path> owners;
    auto addRuntime = [&](const std::filesystem::path &runtime)
    {
        const std::string name = foldedName(runtime.filename().string());
        const auto owner = owners.find(name);
        if (owner != owners.end())
        {
            if (foldedPathKey(owner->second) != foldedPathKey(runtime) &&
                !filesHaveEqualContents(owner->second, runtime))
            {
                error = "runtime package contains different DLLs named '" + runtime.filename().string() + "': '" +
                        owner->second.string() + "' and '" + runtime.string() + "'";
                return false;
            }
            return true;
        }
        owners.emplace(name, runtime);
        closure.push_back(runtime);
        return true;
    };

    for (const std::filesystem::path &runtime : runtimeLibraries)
    {
        if (!addRuntime(runtime))
            return false;
    }

    for (std::size_t index = 0; index < closure.size(); ++index)
    {
        linker::PeExecutableVerifier verifier;
        if (!verifier.verifyDynamicLibrary(closure[index]))
        {
            error = "runtime package dependency '" + closure[index].string() +
                    "' is not a valid AMD64 DLL: " + verifier.error();
            return false;
        }
        for (const std::string &importedName : verifier.info().importedLibraries)
        {
            const std::filesystem::path sibling = closure[index].parent_path() / importedName;
            std::error_code existsError;
            if (!std::filesystem::is_regular_file(sibling, existsError))
                continue;
            if (!addRuntime(sibling))
                return false;
        }
    }

    const std::filesystem::path packageDirectory = destination.parent_path();
    for (const std::filesystem::path &source : closure)
    {
        const std::filesystem::path target = packageDirectory / source.filename();
        std::error_code equivalentError;
        if (std::filesystem::equivalent(source, target, equivalentError) && !equivalentError)
            continue;
        std::error_code existsError;
        if (std::filesystem::exists(target, existsError))
        {
            if (!existsError && filesHaveEqualContents(source, target))
                continue;
            error = "runtime package destination already contains a different '" + target.filename().string() + "': '" +
                    target.string() + "'";
            return false;
        }
        std::error_code copyError;
        std::filesystem::copy_file(source, target, std::filesystem::copy_options::none, copyError);
        if (copyError)
        {
            error =
                "cannot package runtime '" + source.string() + "' as '" + target.string() + "': " + copyError.message();
            return false;
        }
        copiedFiles.push_back(target);
    }
    return true;
}

void removePackagedFiles(std::span<const std::filesystem::path> copiedFiles)
{
    std::error_code removeError;
    for (auto iterator = copiedFiles.rbegin(); iterator != copiedFiles.rend(); ++iterator)
    {
        std::filesystem::remove(*iterator, removeError);
        removeError.clear();
    }
}
} // namespace nativesharp::driver
