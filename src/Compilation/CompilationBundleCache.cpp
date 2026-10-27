#include "nativesharp/Compilation/CompilationBundleCache.h"

#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"

#include <atomic>
#include <charconv>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

namespace nativesharp::compilation
{
namespace
{
constexpr std::string_view kManifestHeader = "nsc-compilation-bundle-v1";
std::atomic<unsigned long long> stagingOrdinal = 0;
std::atomic<unsigned long long> quarantineOrdinal = 0;

int hexDigit(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool decodeHex(std::string_view encoded, std::string& decoded)
{
    if (encoded.size() % 2 != 0) return false;
    decoded.clear();
    decoded.reserve(encoded.size() / 2);
    for (std::size_t index = 0; index < encoded.size(); index += 2)
    {
        const int high = hexDigit(encoded[index]);
        const int low = hexDigit(encoded[index + 1]);
        if (high < 0 || low < 0) return false;
        decoded.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

std::string encodeHex(std::string_view value)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2);
    for (const unsigned char byte : value)
    {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0f]);
    }
    return result;
}

bool parseSize(std::string_view text, std::uintmax_t& result)
{
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool validArtifactName(const std::filesystem::path& name)
{
    return !name.empty() && name == name.filename() && name != "." && name != "..";
}

// EN: Hashes each file with sha256File, one file per worker when there are several. hashed[index]
//     is the result of sha256File for paths[index] and errors[index] its error.
// ES: Calcula el hash de cada archivo con sha256File, un archivo por worker cuando hay varios.
//     hashed[index] es el resultado de sha256File para paths[index] y errors[index] su error.
void hashFiles(
    const std::vector<std::filesystem::path>& paths,
    std::vector<basic::Sha256Digest>& digests,
    std::vector<std::uintmax_t>& sizes,
    std::vector<std::string>& errors,
    std::vector<std::uint8_t>& hashed)
{
    const std::size_t count = paths.size();
    const auto hashFile = [&](std::size_t index)
    {
        hashed[index] = basic::sha256File(paths[index], digests[index], sizes[index], errors[index])
            ? 1 : 0;
    };
    if (count < 2)
    {
        for (std::size_t index = 0; index < count; ++index) hashFile(index);
        return;
    }
    driver::CompilationWorkerPool workers(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        workers.submit([&hashFile, index](std::size_t) { hashFile(index); });
    }
    workers.wait();
}

// EN: Hashes the outputs that store() would reach before its first invalid or duplicated name,
//     one file per worker. hashed[index] is the result of sha256File and errors[index] its error;
//     store() consumes them in the original order, so its manifest and its errors are unchanged.
// ES: Calcula el hash de las salidas que store() alcanzaría antes de su primer nombre inválido o
//     duplicado, un archivo por worker. hashed[index] es el resultado de sha256File y
//     errors[index] su error; store() los consume en el orden original, así que su manifiesto y
//     sus errores no cambian.
void hashBundleOutputs(
    const std::filesystem::path& outputDirectory,
    const std::vector<std::string>& outputNames,
    std::vector<basic::Sha256Digest>& digests,
    std::vector<std::uintmax_t>& sizes,
    std::vector<std::string>& errors,
    std::vector<std::uint8_t>& hashed)
{
    digests.assign(outputNames.size(), basic::Sha256Digest{});
    sizes.assign(outputNames.size(), 0);
    errors.assign(outputNames.size(), std::string{});
    hashed.assign(outputNames.size(), 0);
    std::set<std::string> names;
    std::vector<std::filesystem::path> paths;
    while (paths.size() < outputNames.size() &&
           validArtifactName(std::filesystem::path(outputNames[paths.size()])) &&
           names.insert(outputNames[paths.size()]).second)
    {
        paths.push_back(outputDirectory / std::filesystem::path(outputNames[paths.size()]));
    }
    hashFiles(paths, digests, sizes, errors, hashed);
}

// EN: Reads the artifact records of a manifest with the checks verify() applied to each line.
//     Returns false at the first malformed record with the reason and detail verify() reported;
//     records holds the records before it (cachedPath, destinationPath, size and digest).
// ES: Lee los registros de artefactos de un manifiesto con las comprobaciones que verify()
//     aplicaba a cada línea. Devuelve false en el primer registro mal formado con la razón y el
//     detalle que reportaba verify(); records guarda los registros anteriores.
bool readArtifactRecords(
    std::istream& input,
    const std::filesystem::path& entryDirectory,
    const std::filesystem::path& outputDirectory,
    std::set<std::string>& names,
    std::vector<CompilationBundleArtifact>& records,
    CompilationBundleMissReason& reason,
    std::string& detail)
{
    std::string line;
    while (std::getline(input, line))
    {
        reason = CompilationBundleMissReason::ManifestInvalid;
        if (!line.starts_with("artifact="))
        {
            detail = "artifact-record";
            return false;
        }
        const std::size_t firstTab = line.find('\t');
        const std::size_t secondTab = line.find('\t', firstTab + 1);
        if (firstTab == std::string::npos || secondTab == std::string::npos)
        {
            detail = "artifact-fields";
            return false;
        }
        std::string decodedName;
        if (!decodeHex(std::string_view(line).substr(9, firstTab - 9), decodedName))
        {
            detail = "artifact-name-encoding";
            return false;
        }
        const std::filesystem::path name = std::filesystem::path(decodedName).filename();
        std::uintmax_t expectedSize = 0;
        const std::string digest = line.substr(secondTab + 1);
        if (!validArtifactName(name) || name.generic_string() != decodedName ||
            !names.insert(decodedName).second ||
            !parseSize(
                std::string_view(line).substr(firstTab + 1, secondTab - firstTab - 1),
                expectedSize) || digest.size() != 64)
        {
            detail = decodedName;
            return false;
        }
        records.push_back({
            entryDirectory / "artifacts" / name, outputDirectory / name, expectedSize, digest});
    }
    reason = CompilationBundleMissReason::None;
    return true;
}

// EN: The per-artifact checks of verify(), in their original order, on a precomputed hash.
// ES: Las comprobaciones por artefacto de verify(), en su orden original, sobre un hash previo.
bool checkArtifactRecord(
    const CompilationBundleArtifact& record,
    bool hashed,
    std::uintmax_t actualSize,
    const basic::Sha256Digest& actualDigest,
    const std::string& hashError,
    CompilationBundleMissReason& reason,
    std::string& detail)
{
    detail = record.cachedPath.filename().generic_string();
    std::error_code fileError;
    if (!std::filesystem::is_regular_file(record.cachedPath, fileError))
    {
        reason = fileError
            ? CompilationBundleMissReason::ArtifactUnreadable
            : CompilationBundleMissReason::ArtifactMissing;
        return false;
    }
    if (!hashed)
    {
        reason = CompilationBundleMissReason::ArtifactUnreadable;
        detail += ": " + hashError;
        return false;
    }
    if (actualSize != record.size)
    {
        reason = CompilationBundleMissReason::ArtifactSizeMismatch;
        return false;
    }
    if (basic::sha256Hex(actualDigest) != record.digest)
    {
        reason = CompilationBundleMissReason::ArtifactDigestMismatch;
        return false;
    }
    return true;
}
}

const char* compilationBundleMissReasonName(
    CompilationBundleMissReason reason) noexcept
{
    switch (reason)
    {
    case CompilationBundleMissReason::None: return "none";
    case CompilationBundleMissReason::Disabled: return "disabled";
    case CompilationBundleMissReason::NotFound: return "not-found";
    case CompilationBundleMissReason::ManifestUnreadable:
        return "manifest-unreadable";
    case CompilationBundleMissReason::ManifestInvalid: return "manifest-invalid";
    case CompilationBundleMissReason::IdentityMismatch: return "identity-mismatch";
    case CompilationBundleMissReason::ArtifactMissing: return "artifact-missing";
    case CompilationBundleMissReason::ArtifactUnreadable:
        return "artifact-unreadable";
    case CompilationBundleMissReason::ArtifactSizeMismatch:
        return "artifact-size-mismatch";
    case CompilationBundleMissReason::ArtifactDigestMismatch:
        return "artifact-digest-mismatch";
    case CompilationBundleMissReason::RequiredArtifactMissing:
        return "required-artifact-missing";
    case CompilationBundleMissReason::Hit: return "hit";
    }
    return "unknown";
}

void CompilationBundleCache::setRoot(std::filesystem::path root)
{
    root_ = std::move(root);
}

bool CompilationBundleCache::lookup(
    std::string_view identity,
    CompilationBundleLookup& result,
    std::string& error) const
{
    result = {};
    if (root_.empty())
    {
        result.reason = CompilationBundleMissReason::Disabled;
        return true;
    }
    if (identity.size() < 2)
    {
        error = "compilation bundle identity is incomplete";
        return false;
    }
    result.entryDirectory = root_ /
        std::string(identity.substr(0, 2)) / std::string(identity);
    result.manifestPath = result.entryDirectory / "bundle.manifest";
    std::error_code fileError;
    const bool present = std::filesystem::is_regular_file(
        result.manifestPath, fileError);
    if (fileError &&
        fileError != std::errc::no_such_file_or_directory &&
        fileError != std::errc::not_a_directory)
    {
        error = "cannot inspect compilation bundle manifest '" +
            result.manifestPath.generic_string() + "': " + fileError.message();
        return false;
    }
    result.kind = present
        ? CompilationBundleLookupKind::Candidate
        : CompilationBundleLookupKind::Missing;
    result.reason = present
        ? CompilationBundleMissReason::None
        : CompilationBundleMissReason::NotFound;
    return true;
}

void CompilationBundleCache::quarantine(
    CompilationBundleLookup& lookup,
    CompilationBundleMissReason reason,
    std::string detail) const noexcept
{
    lookup.kind = CompilationBundleLookupKind::Missing;
    lookup.reason = reason;
    lookup.detail = std::move(detail);
    lookup.artifacts.clear();
    lookup.resultOutput.clear();
    try
    {
        if (root_.empty() || lookup.entryDirectory.empty()) return;
        const std::filesystem::path quarantineRoot = root_ / "quarantine";
        std::error_code filesystemError;
        std::filesystem::create_directories(quarantineRoot, filesystemError);
        if (filesystemError) return;
        const std::string suffix = std::to_string(
            quarantineOrdinal.fetch_add(1, std::memory_order_relaxed));
        const std::filesystem::path destination = quarantineRoot /
            (lookup.entryDirectory.filename().generic_string() + "." +
             suffix + ".bundle");
        std::filesystem::rename(
            lookup.entryDirectory, destination, filesystemError);
    }
    catch (...)
    {
    }
}

bool CompilationBundleCache::verify(
    std::string_view identity,
    const std::filesystem::path& outputDirectory,
    const std::vector<std::string>& requiredOutputNames,
    CompilationBundleLookup& lookup,
    bool& hit,
    std::string& error) const
{
    (void)error;
    hit = false;
    lookup.artifacts.clear();
    if (lookup.kind != CompilationBundleLookupKind::Candidate) return true;
    std::ifstream input(lookup.manifestPath, std::ios::binary);
    if (!input)
    {
        quarantine(
            lookup,
            CompilationBundleMissReason::ManifestUnreadable,
            lookup.manifestPath.generic_string());
        return true;
    }
    const auto reject = [&](CompilationBundleMissReason reason, std::string detail)
    {
        input.close();
        quarantine(lookup, reason, std::move(detail));
    };
    std::string line;
    if (!std::getline(input, line) || line != kManifestHeader)
    {
        reject(
            CompilationBundleMissReason::ManifestInvalid,
            "header");
        return true;
    }
    if (!std::getline(input, line) ||
        line != std::string("identity=") + std::string(identity))
    {
        reject(
            CompilationBundleMissReason::IdentityMismatch,
            "identity");
        return true;
    }
    if (!std::getline(input, line) || !line.starts_with("result=") ||
        !decodeHex(std::string_view(line).substr(7), lookup.resultOutput))
    {
        reject(
            CompilationBundleMissReason::ManifestInvalid,
            "result-output");
        return true;
    }
    // EN: Every record is parsed first and the listed artifacts are hashed together on workers;
    //     the checks then run per record in the original order, so the first failing record and
    //     its reason are the ones the sequential loop reported. A parse failure stops the records
    //     exactly where the loop stopped, after the records before it were checked.
    // ES: Primero se interpretan todos los registros y los artefactos listados se hashean juntos
    //     en workers; luego las comprobaciones corren por registro en el orden original, así que
    //     el primer registro que falla y su razón son los que reportaba el ciclo secuencial. Una
    //     falla de interpretación detiene los registros justo donde se detenía el ciclo, después
    //     de comprobar los registros anteriores.
    std::set<std::string> names;
    std::vector<CompilationBundleArtifact> records;
    CompilationBundleMissReason parseReason = CompilationBundleMissReason::None;
    std::string parseDetail;
    const bool parsedAll = readArtifactRecords(
        input, lookup.entryDirectory, outputDirectory, names, records, parseReason, parseDetail);
    std::vector<std::filesystem::path> cachedPaths;
    cachedPaths.reserve(records.size());
    for (const CompilationBundleArtifact& record : records) cachedPaths.push_back(record.cachedPath);
    std::vector<basic::Sha256Digest> digests(records.size());
    std::vector<std::uintmax_t> sizes(records.size(), 0);
    std::vector<std::string> hashErrors(records.size());
    std::vector<std::uint8_t> hashed(records.size(), 0);
    hashFiles(cachedPaths, digests, sizes, hashErrors, hashed);
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        CompilationBundleMissReason reason = CompilationBundleMissReason::None;
        std::string detail;
        if (!checkArtifactRecord(
                records[index], hashed[index] != 0, sizes[index], digests[index], hashErrors[index],
                reason, detail))
        {
            reject(reason, std::move(detail));
            return true;
        }
        lookup.artifacts.push_back(records[index]);
    }
    if (!parsedAll)
    {
        reject(parseReason, std::move(parseDetail));
        return true;
    }
    for (const std::string& required : requiredOutputNames)
    {
        if (!names.contains(required))
        {
            reject(
                CompilationBundleMissReason::RequiredArtifactMissing,
                required);
            return true;
        }
    }
    if (lookup.artifacts.empty() || requiredOutputNames.empty())
    {
        reject(
            CompilationBundleMissReason::ManifestInvalid,
            "empty-artifact-set");
        return true;
    }
    lookup.kind = CompilationBundleLookupKind::Verified;
    lookup.reason = CompilationBundleMissReason::Hit;
    lookup.detail.clear();
    hit = true;
    return true;
}

bool CompilationBundleCache::restore(
    const CompilationBundleLookup& lookup,
    bool& restored,
    std::string& error) const
{
    restored = false;
    if (lookup.kind != CompilationBundleLookupKind::Verified) return true;
    basic::ArtifactTransaction transaction;
    for (const CompilationBundleArtifact& artifact : lookup.artifacts)
    {
        if (!transaction.addStaged(
                artifact.destinationPath, artifact.cachedPath, error))
        {
            return false;
        }
    }
    if (!transaction.commit(error)) return false;
    restored = true;
    return true;
}

bool CompilationBundleCache::store(
    std::string_view identity,
    const std::filesystem::path& outputDirectory,
    const std::vector<std::string>& outputNames,
    std::string_view resultOutput,
    std::string& error) const
{
    error.clear();
    if (root_.empty() || outputNames.empty()) return true;
    if (identity.size() < 2)
    {
        error = "compilation bundle identity is incomplete";
        return false;
    }
    const std::filesystem::path parent = root_ /
        std::string(identity.substr(0, 2));
    const std::filesystem::path entry = parent / std::string(identity);
    std::error_code filesystemError;
    if (std::filesystem::is_directory(entry, filesystemError) && !filesystemError)
    {
        return true;
    }
    filesystemError.clear();
    std::filesystem::create_directories(parent, filesystemError);
    if (filesystemError)
    {
        error = "cannot create compilation bundle cache directory '" +
            parent.generic_string() + "': " + filesystemError.message();
        return false;
    }
    const std::filesystem::path staging = parent /
        (std::string(identity) + ".tmp-" +
         std::to_string(stagingOrdinal.fetch_add(1, std::memory_order_relaxed)));
    const std::filesystem::path artifacts = staging / "artifacts";
    std::filesystem::create_directories(artifacts, filesystemError);
    if (filesystemError)
    {
        error = "cannot create compilation bundle staging directory '" +
            artifacts.generic_string() + "': " + filesystemError.message();
        return false;
    }
    std::ostringstream manifest;
    manifest << kManifestHeader << '\n'
             << "identity=" << identity << '\n'
             << "result=" << encodeHex(resultOutput) << '\n';
    std::vector<basic::Sha256Digest> digests;
    std::vector<std::uintmax_t> sizes;
    std::vector<std::string> hashErrors;
    std::vector<std::uint8_t> hashed;
    hashBundleOutputs(outputDirectory, outputNames, digests, sizes, hashErrors, hashed);
    std::set<std::string> names;
    for (std::size_t index = 0; index < outputNames.size(); ++index)
    {
        const std::string& outputName = outputNames[index];
        const std::filesystem::path name = outputName;
        if (!validArtifactName(name) || !names.insert(outputName).second)
        {
            std::filesystem::remove_all(staging, filesystemError);
            error = "compilation bundle output name is invalid or duplicated: '" +
                outputName + "'";
            return false;
        }
        const std::filesystem::path source = outputDirectory / name;
        if (hashed[index] == 0)
        {
            error = hashErrors[index];
            std::filesystem::remove_all(staging, filesystemError);
            return false;
        }
        std::filesystem::copy_file(
            source,
            artifacts / name,
            std::filesystem::copy_options::overwrite_existing,
            filesystemError);
        if (filesystemError)
        {
            const std::string copyError = filesystemError.message();
            std::error_code ignored;
            std::filesystem::remove_all(staging, ignored);
            error = "cannot stage compilation bundle artifact '" +
                source.generic_string() + "': " + copyError;
            return false;
        }
        manifest << "artifact=" << encodeHex(outputName) << '\t'
                 << sizes[index] << '\t' << basic::sha256Hex(digests[index]) << '\n';
    }
    const std::filesystem::path manifestPath = staging / "bundle.manifest";
    std::ofstream output(manifestPath, std::ios::binary | std::ios::trunc);
    output << manifest.str();
    output.close();
    if (!output)
    {
        std::filesystem::remove_all(staging, filesystemError);
        error = "cannot write compilation bundle manifest '" +
            manifestPath.generic_string() + "'";
        return false;
    }
    std::filesystem::rename(staging, entry, filesystemError);
    if (!filesystemError) return true;
    std::error_code existsError;
    if (std::filesystem::is_directory(entry, existsError) && !existsError)
    {
        std::filesystem::remove_all(staging, existsError);
        return true;
    }
    std::filesystem::remove_all(staging, existsError);
    error = "cannot publish compilation bundle '" + entry.generic_string() +
        "': " + filesystemError.message();
    return false;
}
}
