#include "nativesharp/Compilation/CompilationBundleCache.h"

#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/Sha256.h"

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
    if (fileError)
    {
        error = "cannot inspect compilation bundle manifest '" +
            result.manifestPath.generic_string() + "': " + fileError.message();
        return false;
    }
    result.kind = present
        ? CompilationBundleLookupKind::Candidate
        : CompilationBundleLookupKind::Missing;
    return true;
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
        lookup.kind = CompilationBundleLookupKind::Missing;
        return true;
    }
    std::string line;
    if (!std::getline(input, line) || line != kManifestHeader ||
        !std::getline(input, line) ||
        line != std::string("identity=") + std::string(identity))
    {
        lookup.kind = CompilationBundleLookupKind::Missing;
        return true;
    }
    if (!std::getline(input, line) || !line.starts_with("result=") ||
        !decodeHex(std::string_view(line).substr(7), lookup.resultOutput))
    {
        lookup.kind = CompilationBundleLookupKind::Missing;
        return true;
    }
    std::set<std::string> names;
    while (std::getline(input, line))
    {
        if (!line.starts_with("artifact="))
        {
            lookup.kind = CompilationBundleLookupKind::Missing;
            return true;
        }
        const std::size_t firstTab = line.find('\t');
        const std::size_t secondTab = line.find('\t', firstTab + 1);
        if (firstTab == std::string::npos || secondTab == std::string::npos)
        {
            lookup.kind = CompilationBundleLookupKind::Missing;
            return true;
        }
        std::string decodedName;
        if (!decodeHex(
                std::string_view(line).substr(9, firstTab - 9), decodedName))
        {
            lookup.kind = CompilationBundleLookupKind::Missing;
            return true;
        }
        const std::filesystem::path name =
            std::filesystem::path(decodedName).filename();
        std::uintmax_t expectedSize = 0;
        const std::string digest = line.substr(secondTab + 1);
        if (!validArtifactName(name) || name.generic_string() != decodedName ||
            !names.insert(decodedName).second ||
            !parseSize(
                std::string_view(line).substr(
                    firstTab + 1, secondTab - firstTab - 1),
                expectedSize) || digest.size() != 64)
        {
            lookup.kind = CompilationBundleLookupKind::Missing;
            return true;
        }
        const std::filesystem::path cachedPath =
            lookup.entryDirectory / "artifacts" / name;
        basic::Sha256Digest actualDigest{};
        std::uintmax_t actualSize = 0;
        std::string hashError;
        if (!basic::sha256File(
                cachedPath, actualDigest, actualSize, hashError) ||
            actualSize != expectedSize || basic::sha256Hex(actualDigest) != digest)
        {
            lookup.kind = CompilationBundleLookupKind::Missing;
            lookup.artifacts.clear();
            return true;
        }
        lookup.artifacts.push_back({
            cachedPath, outputDirectory / name, expectedSize, digest});
    }
    for (const std::string& required : requiredOutputNames)
    {
        if (!names.contains(required))
        {
            lookup.kind = CompilationBundleLookupKind::Missing;
            lookup.artifacts.clear();
            return true;
        }
    }
    if (lookup.artifacts.empty() || requiredOutputNames.empty())
    {
        lookup.kind = CompilationBundleLookupKind::Missing;
        lookup.artifacts.clear();
        return true;
    }
    lookup.kind = CompilationBundleLookupKind::Verified;
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
    std::set<std::string> names;
    for (const std::string& outputName : outputNames)
    {
        const std::filesystem::path name = outputName;
        if (!validArtifactName(name) || !names.insert(outputName).second)
        {
            std::filesystem::remove_all(staging, filesystemError);
            error = "compilation bundle output name is invalid or duplicated: '" +
                outputName + "'";
            return false;
        }
        const std::filesystem::path source = outputDirectory / name;
        basic::Sha256Digest digest{};
        std::uintmax_t size = 0;
        if (!basic::sha256File(source, digest, size, error))
        {
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
                 << size << '\t' << basic::sha256Hex(digest) << '\n';
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
