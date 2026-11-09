#include "Internal/DriverInternal.h"
#include "nativesharp/Interop/CppBinaryDependencies.h"
#include "nativesharp/Basic/Sha256.h"
#include "../LLVM/Internal/CppTlsDllAlias.h"

#include <fstream>
#include <unordered_set>

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

std::string pathToUtf8(const std::filesystem::path &value)
{
    const auto encoded = value.generic_u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

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

bool collectCppRuntimesRequiringIsolation(const frontend::ProjectCompilation &compilation,
                                          std::unordered_set<std::string> &identities, std::string &error)
{
    // EN: T08-D05b. Same closure as the packaging (a dependency is a sibling DLL that the PE names), evaluated
    //     before HIR. A root needs isolation when a DLL of its closure, other than the root itself, has a
    //     homonym with other contents in some closure. An invalid DLL is skipped here: the packaging reports it.
    // ES: T08-D05b. El mismo cierre que el empaquetado (una dependencia es una DLL hermana que nombra el PE),
    //     evaluado antes de HIR. Una raiz necesita aislamiento si una DLL de su cierre, distinta de la propia raiz,
    //     tiene un homonimo con otro contenido en algun cierre. Una DLL invalida se omite aqui: la informa el
    //     empaquetado.
    identities.clear();
    if (!compilation.projectGraph()) return true;
    std::vector<std::filesystem::path> candidates;
    if (!collectCppRuntimeCandidates(compilation, candidates, error)) return false;
    std::vector<std::filesystem::path> files;
    std::vector<std::vector<std::size_t>> imports;
    std::unordered_map<std::string, std::size_t> index;
    const auto addFile = [&](const std::filesystem::path &path) -> std::size_t
    {
        const auto [found, inserted] = index.emplace(foldedPathKey(path), files.size());
        if (inserted)
        {
            files.push_back(path);
            imports.emplace_back();
        }
        return found->second;
    };
    std::vector<std::size_t> roots;
    for (const auto &candidate : candidates) roots.push_back(addFile(candidate));
    if (roots.size() < 2) return true;
    for (std::size_t position = 0; position < files.size(); ++position)
    {
        linker::PeExecutableVerifier verifier;
        if (!verifier.verifyDynamicLibrary(files[position])) continue;
        for (const std::string &importedName : verifier.info().importedLibraries)
        {
            const std::filesystem::path sibling = files[position].parent_path() / importedName;
            std::error_code existsError;
            if (!std::filesystem::is_regular_file(sibling, existsError)) continue;
            const std::size_t imported = addFile(sibling);
            imports[position].push_back(imported);
        }
    }
    std::vector<std::string> digests(files.size());
    std::unordered_map<std::string, std::unordered_set<std::string>> contentsByName;
    for (std::size_t position = 0; position < files.size(); ++position)
    {
        basic::Sha256Digest digest{};
        std::uintmax_t size = 0;
        if (!basic::sha256File(files[position], digest, size, error)) return false;
        digests[position] = basic::sha256Hex(digest);
        contentsByName[foldedPathKey(files[position].filename())].insert(digests[position]);
    }
    for (const std::size_t root : roots)
    {
        std::vector<std::size_t> closure{root};
        std::unordered_set<std::size_t> seen{root};
        bool conflict = false;
        for (std::size_t position = 0; position < closure.size(); ++position)
            for (const std::size_t imported : imports[closure[position]])
                if (seen.insert(imported).second)
                {
                    closure.push_back(imported);
                    const std::string name = foldedPathKey(files[imported].filename());
                    if (name != foldedPathKey(files[root].filename()) && contentsByName[name].size() > 1)
                        conflict = true;
                }
        if (conflict) identities.insert(pathToUtf8(files[root].filename()) + ":" + digests[root]);
    }
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
        const auto encodedName = candidate.filename().generic_u8string();
        const auto identity = std::string(reinterpret_cast<const char*>(encodedName.data()), encodedName.size()) +
            ":" + basic::sha256Hex(digest);
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
            if (importedNames.contains(foldedPathKey(candidate.filename())) &&
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

bool selectCppTlsRuntimeAliases(const frontend::ProjectCompilation &compilation,
                                std::span<const std::filesystem::path> dynamicallyLoadedRuntimes,
                                CppTlsRuntimeAliases &aliases, std::string &error)
{
    aliases.clear();
    const auto *mir = compilation.oopMirModel();
    if (!mir) return true;
    const auto fields = mir->staticFields();
    std::unordered_set<std::string> identities;
    for (const auto &field : fields)
    {
        if (!field.isCppExternal || !field.cppThreadLocal || !field.cppDllImport ||
            field.cppTlsRuntimeIdentity.empty()) continue;
        if (std::any_of(fields.begin(), fields.end(), [&](const auto &other) {
                return other.isCppExternal && other.cppThreadLocal && other.cppDllImport &&
                    other.storageSymbol == field.storageSymbol &&
                    other.cppTlsRuntimeIdentity != field.cppTlsRuntimeIdentity &&
                    interop::cppTlsSameDllName(field.cppTlsRuntimeIdentity, other.cppTlsRuntimeIdentity);
            })) identities.insert(field.cppTlsRuntimeIdentity);
    }
    if (identities.empty()) return true;
    std::unordered_set<std::string> matched;
    for (const auto &path : dynamicallyLoadedRuntimes)
    {
        basic::Sha256Digest digest{};
        std::uintmax_t size = 0;
        if (!basic::sha256File(path, digest, size, error)) return false;
        const std::string identity = pathToUtf8(path.filename()) + ":" + basic::sha256Hex(digest);
        if (!identities.contains(identity)) continue;
        const std::string alias = interop::cppTlsDllAlias(identity);
        if (alias.empty()) { error = "invalid C TLS DLL identity"; return false; }
        aliases.emplace(foldedPathKey(path), alias);
        matched.insert(identity);
    }
    if (matched.size() != identities.size())
    {
        error = "cannot locate all DLLs needed for C TLS aliases";
        return false;
    }
    return true;
}

bool computeCppTlsRuntimeAliases(const frontend::ProjectCompilation &compilation,
                                 std::span<const std::filesystem::path> dynamicallyLoadedRuntimes,
                                 std::span<const std::filesystem::path> runtimeLibraries,
                                 const linker::PeExecutableInfo &image,
                                 CppTlsRuntimeAliases &aliases, std::string &error)
{
    if (!selectCppTlsRuntimeAliases(compilation, dynamicallyLoadedRuntimes, aliases, error)) return false;
    if (aliases.empty()) return true;
    std::unordered_set<std::string> renamedNames;
    for (const auto &path : dynamicallyLoadedRuntimes)
        if (aliases.contains(foldedPathKey(path))) renamedNames.insert(foldedPathKey(path.filename()));
    const auto hasAmbiguousImport = [&](const linker::PeExecutableInfo &info) {
        return std::any_of(info.importedLibraries.begin(), info.importedLibraries.end(),
            [&](const std::string &name) { return renamedNames.contains(foldedName(name)); });
    };
    if (hasAmbiguousImport(image))
    {
        error = "same-named C TLS DLLs are also imported directly and cannot yet be distinguished";
        return false;
    }
    for (const auto &path : runtimeLibraries)
    {
        linker::PeExecutableVerifier verifier;
        if (!verifier.verifyDynamicLibrary(path))
        {
            error = "cannot inspect C TLS runtime '" + pathToUtf8(path) + "': " + verifier.error();
            return false;
        }
        if (hasAmbiguousImport(verifier.info()))
        {
            error = "same-named C TLS DLLs are also imported transitively and cannot yet be distinguished";
            return false;
        }
    }
    return true;
}

bool validateCppRuntimeDestinations(const frontend::ProjectCompilation &compilation,
                                    std::span<const std::filesystem::path> runtimeLibraries,
                                    const CppTlsRuntimeAliases &aliases,
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
            const auto alias = aliases.find(foldedPathKey(source));
            const auto destination = destinationDirectory /
                (alias == aliases.end() ? source.filename() : pathFromUtf8(alias->second));
            std::error_code ec;
            const bool exists = std::filesystem::exists(destination, ec);
            if (ec)
            {
                error = "cannot inspect C/C++ runtime destination '" + pathToUtf8(destination) + "': " + ec.message();
                return false;
            }
            if (exists && !filesHaveEqualContents(source, destination))
            {
                error = "runtime package destination already contains a different '" + pathToUtf8(destination.filename()) + "': '" + pathToUtf8(destination) + "'";
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
        const auto destination = destinationDirectory / runtimePackageRelativePath(source);
        std::error_code ec;
        const bool exists = std::filesystem::exists(destination, ec);
        if (ec)
        {
            error = "cannot inspect runtime destination '" + pathToUtf8(destination) + "': " + ec.message();
            return false;
        }
        if (exists && !filesHaveEqualContents(source, destination))
        {
            error = "runtime package destination already contains a different '" + pathToUtf8(destination.filename()) + "': '" + pathToUtf8(destination) + "'";
            return false;
        }
    }
    return true;
}

namespace
{
// EN: T08-D05. Names of the isolation artifacts: one private assembly folder per alias and the activation
//     context manifest that the alias loader (`__nsc_cpp_dll_tls_export`) activates around LoadLibraryExW.
// ES: T08-D05. Nombres de los artefactos de aislamiento: una carpeta de ensamblado privado por alias y el
//     manifiesto de contexto de activacion que el cargador del alias activa alrededor de LoadLibraryExW.
constexpr std::string_view kIsolationAssemblyPrefix = "NativeSharp.Isolation.";
constexpr std::string_view kIsolationManifestSuffix = ".nsc-isolation.manifest";

// EN: the last 16 alphanumeric characters of the alias stem (its identity hash). CreateActCtxW rejects long
//     assembly names with ERROR_SXS_CANT_GEN_ACTCTX; the full 64-digit hash was observed to fail, 16 digits work.
// ES: los ultimos 16 caracteres alfanumericos del nombre del alias (su hash de identidad). CreateActCtxW rechaza
//     nombres de ensamblado largos con ERROR_SXS_CANT_GEN_ACTCTX; el hash completo de 64 digitos fallo y 16
//     digitos funcionan.
std::string isolationAssemblyName(const std::filesystem::path &aliasName)
{
    std::string digits;
    for (const char character : pathToUtf8(aliasName.stem()))
    {
        const auto value = static_cast<unsigned char>(character);
        if ((value >= '0' && value <= '9') || (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z'))
            digits.push_back(character);
    }
    if (digits.size() > 16) digits.erase(0, digits.size() - 16);
    return std::string(kIsolationAssemblyPrefix) + digits;
}

std::string isolationAssemblyIdentity(std::string_view name)
{
    return "<assemblyIdentity type=\"win32\" name=\"" + std::string(name) +
           "\" version=\"1.0.0.0\" processorArchitecture=\"amd64\"/>";
}

struct PackagedRuntime
{
    std::filesystem::path source;
    std::filesystem::path targetName;
    std::vector<std::size_t> imports;
};
} // namespace

std::filesystem::path runtimePackageRelativePath(const std::filesystem::path &stagedRuntime)
{
    const std::filesystem::path folder = stagedRuntime.parent_path().filename();
    if (pathToUtf8(folder).starts_with(kIsolationAssemblyPrefix)) return folder / stagedRuntime.filename();
    return stagedRuntime.filename();
}

bool packageRuntimeClosure(std::span<const std::filesystem::path> runtimeLibraries,
                           std::span<const std::filesystem::path> dynamicallyLoadedRuntimes,
                           const CppTlsRuntimeAliases &aliases,
                           const std::filesystem::path &destination, std::vector<std::filesystem::path> &copiedFiles,
                           std::string &error)
{
    // Packaging is deliberately rooted in explicit LibraryReference runtimes.  A
    // dependency is followed only when the PE names it and a sibling DLL with that
    // exact name exists; no PATH lookup or directory scan is performed.
    // EN: T08-D05. Each root keeps its own closure. A name packaged with different contents is accepted only
    //     when every root that reaches it is a C TLS alias loaded by the alias loader and the name is not the
    //     root itself: each such alias receives a private assembly with its own versions and an activation
    //     context manifest. Any other homonymous conflict is rejected as before. No DLL is modified.
    //     A root that the product neither imports nor loads at run time (a Runtime declared only so that it is
    //     packaged) does not take part in the conflict check.
    // ES: T08-D05. Cada raiz conserva su propio cierre. Un nombre empaquetado con contenidos distintos solo se
    //     acepta si todas las raices que lo alcanzan son alias TLS C cargados por el cargador de alias y el
    //     nombre no es la propia raiz: cada uno de esos alias recibe un ensamblado privado con sus versiones y
    //     un manifiesto de contexto de activacion. Cualquier otro conflicto homonimo se rechaza como antes. No
    //     se modifica ninguna DLL. Una raiz que el producto no importa ni carga en ejecucion (un Runtime declarado
    //     solo para empaquetarlo) no participa en la comprobacion de conflictos.
    std::vector<PackagedRuntime> files;
    std::unordered_map<std::string, std::size_t> fileIndex;
    const auto addFile = [&](const std::filesystem::path &source) -> std::size_t
    {
        const std::string key = foldedPathKey(source);
        const auto found = fileIndex.find(key);
        if (found != fileIndex.end()) return found->second;
        const auto alias = aliases.find(key);
        files.push_back({source, alias == aliases.end() ? source.filename() : pathFromUtf8(alias->second), {}});
        fileIndex.emplace(key, files.size() - 1);
        return files.size() - 1;
    };
    std::vector<std::size_t> roots;
    for (const std::filesystem::path &runtime : runtimeLibraries) roots.push_back(addFile(runtime));
    for (std::size_t index = 0; index < files.size(); ++index)
    {
        linker::PeExecutableVerifier verifier;
        if (!verifier.verifyDynamicLibrary(files[index].source))
        {
            error = "runtime package dependency '" + pathToUtf8(files[index].source) +
                    "' is not a valid AMD64 DLL: " + verifier.error();
            return false;
        }
        for (const std::string &importedName : verifier.info().importedLibraries)
        {
            const std::filesystem::path sibling = files[index].source.parent_path() / importedName;
            std::error_code existsError;
            if (!std::filesystem::is_regular_file(sibling, existsError))
                continue;
            const std::size_t imported = addFile(sibling);
            files[index].imports.push_back(imported);
        }
    }

    // EN: closure of every root, in breadth-first order. / ES: cierre de cada raiz, en anchura.
    std::vector<std::vector<std::size_t>> closures;
    for (const std::size_t root : roots)
    {
        std::vector<std::size_t> closure{root};
        std::unordered_set<std::size_t> seen{root};
        for (std::size_t position = 0; position < closure.size(); ++position)
            for (const std::size_t imported : files[closure[position]].imports)
                if (seen.insert(imported).second) closure.push_back(imported);
        closures.push_back(std::move(closure));
    }

    // EN: versions of every packaged name; a version is a set of files with equal contents.
    // ES: versiones de cada nombre empaquetado; una version es un grupo de archivos de igual contenido.
    std::unordered_map<std::string, std::vector<std::size_t>> versionRepresentatives;
    std::vector<std::size_t> versionOf(files.size());
    std::vector<std::string> nameOrder;
    for (std::size_t index = 0; index < files.size(); ++index)
    {
        const std::string name = foldedPathKey(files[index].targetName);
        auto [position, inserted] = versionRepresentatives.try_emplace(name);
        if (inserted) nameOrder.push_back(name);
        auto &representatives = position->second;
        std::size_t version = representatives.size();
        for (std::size_t candidate = 0; candidate < representatives.size(); ++candidate)
        {
            const auto &other = files[representatives[candidate]].source;
            if (foldedPathKey(other) == foldedPathKey(files[index].source) ||
                filesHaveEqualContents(other, files[index].source))
            {
                version = candidate;
                break;
            }
        }
        if (version == representatives.size()) representatives.push_back(index);
        versionOf[index] = version;
    }
    std::unordered_set<std::string> conflicting;
    for (const auto &[name, representatives] : versionRepresentatives)
        if (representatives.size() > 1) conflicting.insert(name);

    // EN: names the product image imports, and runtimes the generated code loads by path.
    // ES: nombres que importa la imagen del producto y runtimes que el codigo generado carga por ruta.
    std::unordered_set<std::string> imageImports;
    {
        linker::PeExecutableVerifier image;
        if (image.verify(destination) || image.verifyDynamicLibrary(destination))
            for (const std::string &imported : image.info().importedLibraries)
                imageImports.insert(foldedName(imported));
    }
    std::unordered_set<std::string> dynamicallyLoaded;
    for (const auto &runtime : dynamicallyLoadedRuntimes) dynamicallyLoaded.insert(foldedPathKey(runtime));
    std::vector<std::uint8_t> isolatedAlias(roots.size(), 0);
    for (std::size_t rootPosition = 0; rootPosition < roots.size(); ++rootPosition)
    {
        const std::size_t root = roots[rootPosition];
        const std::string sourceKey = foldedPathKey(files[root].source);
        const bool imported = imageImports.contains(foldedPathKey(files[root].targetName));
        const bool aliased = aliases.contains(sourceKey) && !imported;
        if (!imported && !aliased && !dynamicallyLoaded.contains(sourceKey)) continue;
        isolatedAlias[rootPosition] = aliased ? 1 : 0;
        for (const std::size_t member : closures[rootPosition])
        {
            const std::string name = foldedPathKey(files[member].targetName);
            if (!conflicting.contains(name) || (aliased && member != root)) continue;
            const auto &representatives = versionRepresentatives[name];
            error = "runtime package contains different DLLs named '" + pathToUtf8(files[member].targetName) +
                    "': '" + pathToUtf8(files[representatives[0]].source) + "' and '" +
                    pathToUtf8(files[representatives[1]].source) + "'";
            // EN: an alias in the import table is loaded before any activation context can be activated.
            // ES: un alias de la tabla de importacion se carga antes de poder activar un contexto.
            if (imported && aliases.contains(sourceKey))
                error += "; C TLS alias '" + pathToUtf8(files[root].targetName) +
                         "' is imported directly, so its dependencies cannot be isolated";
            return false;
        }
    }

    const std::filesystem::path packageDirectory = destination.parent_path();
    const auto stage = [&](const std::filesystem::path &source, const std::filesystem::path &target) -> bool
    {
        std::error_code equivalentError;
        if (std::filesystem::equivalent(source, target, equivalentError) && !equivalentError)
            return true;
        std::error_code existsError;
        if (std::filesystem::exists(target, existsError))
        {
            if (!existsError && filesHaveEqualContents(source, target))
                return true;
            error = "runtime package destination already contains a different '" + pathToUtf8(target.filename()) +
                    "': '" + pathToUtf8(target) + "'";
            return false;
        }
        std::error_code copyError;
        std::filesystem::copy_file(source, target, std::filesystem::copy_options::none, copyError);
        if (copyError)
        {
            error = "cannot package runtime '" + pathToUtf8(source) + "' as '" + pathToUtf8(target) + "': " +
                    copyError.message();
            return false;
        }
        copiedFiles.push_back(target);
        return true;
    };
    const auto writeText = [&](const std::filesystem::path &target, const std::string &contents) -> bool
    {
        std::ofstream stream(target, std::ios::binary | std::ios::trunc);
        stream << contents;
        stream.close();
        if (!stream)
        {
            error = "cannot write runtime isolation manifest '" + pathToUtf8(target) + "'";
            return false;
        }
        copiedFiles.push_back(target);
        return true;
    };

    std::unordered_set<std::string> packagedNames;
    for (const std::vector<std::size_t> &closure : closures)
        for (const std::size_t member : closure)
        {
            const std::string name = foldedPathKey(files[member].targetName);
            if (conflicting.contains(name) || !packagedNames.insert(name).second) continue;
            if (!stage(files[member].source, packageDirectory / files[member].targetName)) return false;
        }

    std::unordered_set<std::string> assemblyNames;
    for (std::size_t rootPosition = 0; rootPosition < roots.size(); ++rootPosition)
    {
        if (isolatedAlias[rootPosition] == 0) continue;
        std::vector<std::size_t> isolated;
        for (const std::size_t member : closures[rootPosition])
            if (conflicting.contains(foldedPathKey(files[member].targetName))) isolated.push_back(member);
        if (isolated.empty()) continue;
        const std::filesystem::path &aliasName = files[roots[rootPosition]].targetName;
        const std::string assembly = isolationAssemblyName(aliasName);
        if (!assemblyNames.insert(assembly).second)
        {
            error = "runtime isolation assembly name collision: '" + assembly + "'";
            return false;
        }
        const std::filesystem::path assemblyDirectory = packageDirectory / pathFromUtf8(assembly);
        std::error_code directoryError;
        std::filesystem::create_directories(assemblyDirectory, directoryError);
        if (directoryError)
        {
            error = "cannot create runtime isolation directory '" + pathToUtf8(assemblyDirectory) +
                    "': " + directoryError.message();
            return false;
        }
        std::string assemblyManifest =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
            "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\r\n  " +
            isolationAssemblyIdentity(assembly) + "\r\n";
        for (const std::size_t member : isolated)
        {
            if (!stage(files[member].source, assemblyDirectory / files[member].targetName)) return false;
            assemblyManifest += "  <file name=\"" + pathToUtf8(files[member].targetName) + "\"/>\r\n";
        }
        assemblyManifest += "</assembly>\r\n";
        if (!writeText(assemblyDirectory / pathFromUtf8(assembly + ".manifest"), assemblyManifest)) return false;
        const std::string contextManifest =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
            "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\r\n"
            "  <dependency><dependentAssembly>" + isolationAssemblyIdentity(assembly) +
            "</dependentAssembly></dependency>\r\n</assembly>\r\n";
        std::filesystem::path contextPath = packageDirectory / aliasName;
        contextPath += pathFromUtf8(kIsolationManifestSuffix);
        if (!writeText(contextPath, contextManifest)) return false;
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
        // EN: an isolation folder is removed once empty (T08-D05). / ES: una carpeta de aislamiento vacia se borra.
        if (runtimePackageRelativePath(*iterator).has_parent_path())
            std::filesystem::remove(iterator->parent_path(), removeError);
        removeError.clear();
    }
}
} // namespace nativesharp::driver
