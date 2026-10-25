#include "ThinLtoLibraryLinking.h"

#include "Internal/DriverInternal.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/LLVM/ThinLtoLibrarySidecar.h"
#include "nativesharp/LLVM/ThinLtoLibrarySidecarCodec.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::driver
{
namespace
{
// SHA-256 of a whole file in hexadecimal, or empty when it cannot be read.
// SHA-256 de un archivo completo en hexadecimal, o vacio si no se puede leer.
std::string fileDigest(const std::filesystem::path& path)
{
    basic::Sha256Digest digest{};
    std::uintmax_t size = 0;
    std::string ignored;
    return basic::sha256File(path, digest, size, ignored) ? basic::sha256Hex(digest) : std::string();
}

// Lower-case file name of a metadata path, the name a dependency is matched by.
// Nombre de archivo en minusculas de una ruta de metadata, el nombre con que se empata una dependencia.
std::string dependencyMetadataName(const std::filesystem::path& metadata)
{
    std::string name = metadata.filename().string();
    for (char& character : name)
    {
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    }
    return name;
}

// The compatibility line of the configured NativeSharp runtime library (item 53): the units call
// it, so a different runtime makes the sidecar incompatible.
// La linea de compatibilidad de la biblioteca de runtime de NativeSharp configurada (punto 53): las
// unidades la llaman, asi que otro runtime vuelve incompatible el sidecar.
std::string thinLtoRuntimeIdentity()
{
    const std::filesystem::path runtime = linker::configuredWindowsRuntimeLibrary();
    const std::string digest = runtime.empty() ? std::string("none") : fileDigest(runtime);
    return "runtime " + (digest.empty() ? std::string("unreadable") : digest) + "\n";
}

// "name digest" lines, sorted, of the metadata of every library reference of `compilation`.
// Lineas "nombre digest", ordenadas, de la metadata de cada referencia a biblioteca de `compilation`.
std::string thinLtoDependencyText(const frontend::ProjectCompilation& compilation)
{
    std::vector<std::string> lines;
    if (compilation.projectGraph() != nullptr)
    {
        std::unordered_set<std::string> seen;
        for (const compilation::Project& project : compilation.projectGraph()->projects())
        {
            for (const compilation::LibraryReference& reference : project.libraryReferences())
            {
                if (!seen.insert(foldedPathKey(reference.metadataPath)).second) continue;
                const std::string digest = fileDigest(reference.metadataPath);
                lines.push_back(dependencyMetadataName(reference.metadataPath) + " " + digest);
            }
        }
    }
    std::ranges::sort(lines);
    std::string text;
    for (const std::string& line : lines) text += line + "\n";
    return text;
}

// Every dependency of a sidecar must be referenced by this build with the same metadata bytes.
// Toda dependencia de un sidecar debe estar referenciada por este build con los mismos bytes.
bool dependenciesMatch(
    const std::string& dependencies,
    const std::unordered_map<std::string, std::filesystem::path>& metadataByName,
    std::string& reason)
{
    std::size_t begin = 0;
    while (begin < dependencies.size())
    {
        std::size_t end = dependencies.find('\n', begin);
        if (end == std::string::npos) end = dependencies.size();
        const std::string line = dependencies.substr(begin, end - begin);
        begin = end + 1;
        const std::size_t separator = line.rfind(' ');
        if (separator == std::string::npos)
        {
            reason = "the sidecar lists an invalid dependency";
            return false;
        }
        const std::string name = line.substr(0, separator);
        const auto found = metadataByName.find(name);
        if (found == metadataByName.end())
        {
            reason = "dependency " + name + " is not referenced by this build";
            return false;
        }
        if (fileDigest(found->second) != line.substr(separator + 1))
        {
            reason = "dependency " + name + " differs from the one the library was built with";
            return false;
        }
    }
    return true;
}

// The checks of NSC-DISENO-SIDECAR-THINLTO-BIBLIOTECAS.md, section 6; `reason` says why one failed.
// Las revisiones de la seccion 6 del diseno; `reason` dice por que fallo una.
bool acceptSidecar(
    const compilation::LibraryReference& reference,
    const std::filesystem::path& sidecarPath,
    const std::string& compatibility,
    const std::unordered_map<std::string, std::filesystem::path>& metadataByName,
    llvm_backend::ThinLtoLibrarySidecar& sidecar,
    std::string& reason)
{
    if (!llvm_backend::ThinLtoLibrarySidecarCodec::read(sidecarPath, sidecar, reason)) return false;
    if (sidecar.compatibility != compatibility)
    {
        reason = "built with another LLVM version, target or options";
        return false;
    }
    if (fileDigest(reference.libraryPath) != sidecar.libraryDigest)
    {
        reason = "the sidecar does not match its library";
        return false;
    }
    if (fileDigest(reference.metadataPath) != sidecar.metadataDigest)
    {
        reason = "the sidecar does not match its metadata";
        return false;
    }
    return dependenciesMatch(sidecar.dependencies, metadataByName, reason);
}

// Removes the archive directories nobody used for a week; housekeeping that never fails a build.
// Borra los directorios de archivos que nadie uso en una semana; mantenimiento que nunca falla un build.
void pruneArchiveDirectories(const std::filesystem::path& root, const std::filesystem::path& keep)
{
    std::error_code error;
    const std::filesystem::file_time_type limit =
        std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * 7);
    for (std::filesystem::directory_iterator entry(root, error), end; !error && entry != end; entry.increment(error))
    {
        std::error_code entryError;
        if (entry->path() == keep || !entry->is_directory(entryError)) continue;
        const std::filesystem::file_time_type written = entry->last_write_time(entryError);
        if (!entryError && written < limit) std::filesystem::remove_all(entry->path(), entryError);
    }
}

// True when `archive` is a COFF archive that NSLink can read.
// Verdadero cuando `archive` es un archivo COFF que NSLink puede leer.
bool usableArchive(const std::filesystem::path& archive)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(archive, error)) return false;
    linker::CoffArchiveVerifier verifier;
    return verifier.verify(archive);
}

// Packs the objects of one consumed library into
// %TEMP%/nativesharp-thinlto-libraries/<digest>/<library file name>. NSLink spells every archive
// path into the PDB module table and its GUID, so a path that depends only on the objects keeps -g
// builds reproducible; the archive is written as a partial file and renamed into place.
// Empaqueta los objetos de una biblioteca consumida en
// %TEMP%/nativesharp-thinlto-libraries/<digest>/<nombre de la biblioteca>. NSLink escribe la ruta de
// cada archivo en la tabla de modulos del PDB y en su GUID, asi que una ruta que solo depende de los
// objetos mantiene reproducibles los builds -g; el archivo se escribe parcial y se renombra.
bool publishLibraryArchive(
    const llvm_backend::ThinLtoLibraryObjects& library,
    std::filesystem::path& archive,
    std::string& error)
{
    if (library.objectFiles.empty())
    {
        error = "ThinLTO produced no objects for '" + library.libraryPath.string() + "'";
        return false;
    }
    std::string material = library.libraryPath.filename().generic_string() + "\n";
    for (const std::filesystem::path& object : library.objectFiles)
    {
        basic::Sha256Digest digest{};
        std::uintmax_t size = 0;
        if (!basic::sha256File(object, digest, size, error)) return false;
        material += std::to_string(size) + ":" + basic::sha256Hex(digest) + "\n";
    }
    const std::string digest = basic::sha256Hex(basic::sha256(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()))).substr(0, 16);
    std::error_code filesystemError;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path(filesystemError) / "nativesharp-thinlto-libraries";
    if (filesystemError)
    {
        error = "cannot locate the temporary directory: " + filesystemError.message();
        return false;
    }
    const std::filesystem::path directory = root / digest;
    archive = directory / library.libraryPath.filename();
    if (usableArchive(archive))
    {
        std::filesystem::last_write_time(
            directory, std::filesystem::file_time_type::clock::now(), filesystemError);
        return true;
    }
    std::filesystem::create_directories(directory, filesystemError);
    if (filesystemError)
    {
        error = "cannot create '" + directory.string() + "': " + filesystemError.message();
        return false;
    }
    pruneArchiveDirectories(root, directory);
    // The objects live in the private temporary directory of this generation, whose name is unique.
    // Los objetos viven en el directorio temporal privado de esta generacion, cuyo nombre es unico.
    const std::filesystem::path objectDirectory = library.objectFiles.front().parent_path();
    const std::filesystem::path staged = objectDirectory / ("thinlto-library-" + digest + ".lib");
    std::unique_ptr<linker::LldLinker> librarian = linker::LldLinker::discover(error);
    if (librarian == nullptr || !librarian->createStaticLibrary(library.objectFiles, staged, error)) return false;
    if (!usableArchive(staged))
    {
        error = "the ThinLTO archive of '" + library.libraryPath.string() + "' failed COFF verification";
        return false;
    }
    std::filesystem::path partial = archive;
    partial += ".partial-" + objectDirectory.filename().string();
    std::filesystem::copy_file(staged, partial, std::filesystem::copy_options::overwrite_existing, filesystemError);
    if (!filesystemError) std::filesystem::rename(partial, archive, filesystemError);
    if (filesystemError)
    {
        std::error_code cleanupError;
        std::filesystem::remove(partial, cleanupError);
        // Another build may have published the same content first.
        // Otro build pudo publicar primero el mismo contenido.
        if (usableArchive(archive)) return true;
        error = "cannot publish the ThinLTO archive '" + archive.string() + "': " + filesystemError.message();
        return false;
    }
    return true;
}
}

bool ThinLtoLibraryLinking::collect(
    const frontend::ProjectCompilation& compilation,
    llvm_backend::LinkTimeOptimizationMode mode,
    llvm_backend::LlvmOptimizationLevel level,
    llvm_backend::DebugInformationKind debugInformation,
    std::string& error)
{
    libraries_.clear();
    fallbacks_.clear();
    consumed_ = 0;
    active_ = llvm_backend::ProjectOopObjectGenerator::thinLtoRuns(mode, level);
    if (!active_ || compilation.projectGraph() == nullptr) return true;
    std::string compatibility;
    if (!llvm_backend::ProjectOopObjectGenerator::thinLtoCompatibility(level, debugInformation, compatibility, error))
        return false;
    compatibility += thinLtoRuntimeIdentity();
    std::unordered_map<std::string, std::filesystem::path> metadataByName;
    for (const compilation::Project& project : compilation.projectGraph()->projects())
    {
        for (const compilation::LibraryReference& reference : project.libraryReferences())
            metadataByName.emplace(dependencyMetadataName(reference.metadataPath), reference.metadataPath);
    }
    std::unordered_set<std::string> seen;
    for (const compilation::Project& project : compilation.projectGraph()->projects())
    {
        for (const compilation::LibraryReference& reference : project.libraryReferences())
        {
            // A DLL boundary is a binary contract: only static libraries are consumed.
            // El limite de una DLL es un contrato binario: solo se consumen bibliotecas estaticas.
            if (!reference.runtimePath.empty() || !seen.insert(foldedPathKey(reference.libraryPath)).second)
                continue;
            const std::filesystem::path sidecarPath =
                llvm_backend::ThinLtoLibrarySidecarCodec::pathFor(reference.libraryPath);
            std::error_code statusError;
            if (!std::filesystem::is_regular_file(sidecarPath, statusError)) continue;
            llvm_backend::ThinLtoLibrarySidecar sidecar;
            std::string reason;
            if (acceptSidecar(reference, sidecarPath, compatibility, metadataByName, sidecar, reason))
                libraries_.push_back({reference.libraryPath, std::move(sidecar.units)});
            else
                fallbacks_.push_back(reference.libraryPath.filename().string() + ": " + reason);
        }
    }
    return true;
}

std::optional<std::vector<std::string>> ThinLtoLibraryLinking::preservedSymbols(
    std::span<const std::filesystem::path> linkLibraries,
    std::span<const std::string> roots) const
{
    if (!active_) return std::nullopt;
    const std::filesystem::path runtime = linker::configuredWindowsRuntimeLibrary();
    // Without a separate runtime library the runtime module is built with the program after
    // ThinLTO, and nothing can scan what it references.
    // Sin biblioteca de runtime aparte, el modulo de runtime se construye con el programa despues de
    // ThinLTO y nada puede revisar lo que referencia.
    if (runtime.empty()) return std::nullopt;
    std::unordered_set<std::string> consumed;
    for (const llvm_backend::ThinLtoLibraryUnits& library : libraries_)
        consumed.insert(foldedPathKey(library.libraryPath));
    std::vector<std::filesystem::path> inputs = {runtime};
    for (const std::filesystem::path& library : linkLibraries)
        if (!consumed.contains(foldedPathKey(library))) inputs.push_back(library);
    std::vector<std::string> symbols(roots.begin(), roots.end());
    for (const std::filesystem::path& input : inputs)
    {
        std::string ignored;
        if (!llvm_backend::ProjectOopObjectGenerator::collectUndefinedSymbols(input, symbols, ignored))
            return std::nullopt;
    }
    return symbols;
}

std::vector<llvm_backend::ThinLtoLibraryUnits> ThinLtoLibraryLinking::takeLibraries()
{
    return std::move(libraries_);
}

bool ThinLtoLibraryLinking::replaceLinkLibraries(
    const llvm_backend::ProjectOopObjectBundle& bundle,
    std::vector<std::filesystem::path>& linkLibraries,
    std::string& error)
{
    consumed_ = 0;
    for (const llvm_backend::ThinLtoLibraryObjects& library : bundle.thinLtoLibraryObjects())
    {
        std::filesystem::path archive;
        if (!publishLibraryArchive(library, archive, error)) return false;
        const std::string key = foldedPathKey(library.libraryPath);
        for (std::filesystem::path& linked : linkLibraries)
            if (foldedPathKey(linked) == key) linked = archive;
        ++consumed_;
    }
    return true;
}

void ThinLtoLibraryLinking::printResult(std::ostream& output) const
{
    if (!active_) return;
    output << "thinlto-libraries: " << consumed_ << '\n';
    for (const std::string& fallback : fallbacks_) output << "thinlto-library-fallback: " << fallback << '\n';
}

bool ThinLtoLibraryLinking::writeSidecar(
    const llvm_backend::ProjectOopObjectBundle& bundle,
    const frontend::ProjectCompilation& compilation,
    const std::filesystem::path& libraryStage,
    const std::filesystem::path& metadataStage,
    const std::filesystem::path& sidecarStage,
    std::string& error)
{
    llvm_backend::ThinLtoLibrarySidecar sidecar;
    sidecar.compatibility = bundle.thinLtoCompatibility().empty()
        ? std::string()
        : bundle.thinLtoCompatibility() + thinLtoRuntimeIdentity();
    sidecar.libraryDigest = fileDigest(libraryStage);
    sidecar.metadataDigest = fileDigest(metadataStage);
    sidecar.dependencies = thinLtoDependencyText(compilation);
    if (sidecar.compatibility.empty() || sidecar.libraryDigest.empty() || sidecar.metadataDigest.empty())
    {
        error = "the ThinLTO sidecar needs the compatibility identity, the library and its metadata";
        return false;
    }
    sidecar.units.assign(bundle.thinLtoUnitBitcode().begin(), bundle.thinLtoUnitBitcode().end());
    return llvm_backend::ThinLtoLibrarySidecarCodec::write(sidecarStage, sidecar, error);
}
} // namespace nativesharp::driver
