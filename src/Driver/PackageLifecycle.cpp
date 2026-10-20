#include "nativesharp/Driver/PackageLifecycle.h"
#include "Internal/PackageLifecycleManifestEntry.h"
#include "Internal/PlanEntry.h"
#include "Internal/RemovalTarget.h"

#include "nativesharp/Basic/Sha256.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>
#include <set>
#include <string_view>
#include <system_error>
#include <vector>

namespace nativesharp::driver
{
namespace
{
constexpr std::size_t kMaximumEntries = 10'000;
constexpr std::size_t kMaximumLineLength = 16'384;

void removeCarriageReturn(std::string &line)
{
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }
}

bool isCanonicalManagedPath(std::string_view text)
{
    if (text.empty() || text.front() == '/' || text.find('\\') != std::string_view::npos ||
        text.find('|') != std::string_view::npos || text.find(':') != std::string_view::npos ||
        !(text.starts_with("bin/") || text.starts_with("lib/") || text.starts_with("sdk/win64/") ||
            text.starts_with("share/nativesharp/")))
    {
        return false;
    }
    const std::filesystem::path path{std::string(text)};
    return !path.is_absolute() && path.generic_string() == text && path.lexically_normal().generic_string() == text &&
           std::none_of(path.begin(), path.end(), [](const auto &part) { return part == "." || part == ".."; });
}

bool isLowerDigest(std::string_view digest)
{
    return digest.size() == 64U && std::all_of(digest.begin(), digest.end(), [](char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

bool parseSize(std::string_view text, std::uintmax_t &size)
{
    if (text.empty())
    {
        return false;
    }
    const auto result = std::from_chars(text.data(), text.data() + text.size(), size);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && std::to_string(size) == text;
}

bool readManifest(const std::filesystem::path &source, std::vector<ManifestEntry> &entries, std::string &error)
{
    entries.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open installation manifest '" + source.string() + "'";
        return false;
    }
    std::string line;
    if (!std::getline(stream, line))
    {
        error = "installation manifest is empty";
        return false;
    }
    removeCarriageReturn(line);
    if (line != "NSINSTALL 1")
    {
        error = "unsupported installation manifest header '" + line + "'; expected 'NSINSTALL 1'";
        return false;
    }

    std::size_t lineNumber = 1;
    while (std::getline(stream, line))
    {
        ++lineNumber;
        removeCarriageReturn(line);
        const std::size_t first = line.find('|');
        const std::size_t second = first == std::string::npos ? first : line.find('|', first + 1U);
        if (line.size() > kMaximumLineLength || first == std::string::npos || second == std::string::npos ||
            line.find('|', second + 1U) != std::string::npos)
        {
            error = "malformed installation manifest line " + std::to_string(lineNumber);
            return false;
        }
        ManifestEntry entry;
        entry.digest = line.substr(0, first);
        entry.path = line.substr(second + 1U);
        if (!isLowerDigest(entry.digest) ||
            !parseSize(std::string_view(line).substr(first + 1U, second - first - 1U), entry.size) ||
            !isCanonicalManagedPath(entry.path))
        {
            error = "invalid installation manifest entry for '" + entry.path + "'";
            return false;
        }
        if (!entries.empty() && entry.path <= entries.back().path)
        {
            error = "installation manifest path '" + entry.path + "' is duplicate or out of canonical order";
            return false;
        }
        entries.push_back(std::move(entry));
        if (entries.size() > kMaximumEntries)
        {
            error = "installation manifest exceeds 10000 entries";
            return false;
        }
    }
    if (stream.bad() || entries.empty())
    {
        error = stream.bad() ? "cannot read installation manifest '" + source.string() + "'"
                             : "installation manifest has no entries";
        return false;
    }
    return true;
}

bool fileDigest(const std::filesystem::path &path, std::string &digest, std::string &error)
{
    basic::Sha256Digest value{};
    std::uintmax_t size = 0;
    if (!basic::sha256File(path, value, size, error))
    {
        return false;
    }
    digest = basic::sha256Hex(value);
    return true;
}

bool fileIdentity(const std::filesystem::path &path, std::string &digest, std::uintmax_t &size, std::string &error)
{
    basic::Sha256Digest value{};
    if (!basic::sha256File(path, value, size, error))
    {
        return false;
    }
    digest = basic::sha256Hex(value);
    return true;
}

bool isBuildIdentityPath(std::string_view path)
{
    return path == "bin/nsc.exe" || path == "bin/nsc.pdb";
}

bool writeLifecycleTemporary(
    const std::filesystem::path& temporary,
    std::string_view text,
    std::string& error)
{
    std::error_code filesystemError;
    std::filesystem::remove(temporary, filesystemError);
    filesystemError.clear();
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "cannot open package lifecycle temporary file '" + temporary.string() + "'";
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (stream) return true;
    std::filesystem::remove(temporary, filesystemError);
    error = "cannot write package lifecycle temporary file '" + temporary.string() + "'";
    return false;
}

bool preserveLifecycleDestination(
    const std::filesystem::path& destination,
    const std::filesystem::path& temporary,
    const std::filesystem::path& backup,
    bool& hadDestination,
    std::string& error)
{
    std::error_code filesystemError;
    hadDestination = std::filesystem::exists(destination, filesystemError);
    if (filesystemError)
    {
        const std::string inspectMessage = filesystemError.message();
        std::filesystem::remove(temporary, filesystemError);
        error = "cannot inspect package lifecycle destination '" + destination.string() +
            "': " + inspectMessage;
        return false;
    }
    if (!hadDestination) return true;
    std::filesystem::remove(backup, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(destination, backup, filesystemError);
    if (!filesystemError) return true;
    const std::string preserveMessage = filesystemError.message();
    std::filesystem::remove(temporary, filesystemError);
    error = "cannot preserve previous package lifecycle file '" + destination.string() +
        "': " + preserveMessage;
    return false;
}

bool commitLifecycleDestination(
    const std::filesystem::path& destination,
    const std::filesystem::path& temporary,
    const std::filesystem::path& backup,
    bool hadDestination,
    std::string& error)
{
    std::error_code filesystemError;
    std::filesystem::rename(temporary, destination, filesystemError);
    if (filesystemError)
    {
        const std::string commitMessage = filesystemError.message();
        std::filesystem::remove(temporary, filesystemError);
        if (hadDestination)
        {
            filesystemError.clear();
            std::filesystem::rename(backup, destination, filesystemError);
            if (filesystemError)
            {
                error = "cannot commit package lifecycle file '" + destination.string() +
                    "' and cannot restore its previous contents: " + filesystemError.message();
                return false;
            }
        }
        error = "cannot commit package lifecycle file '" + destination.string() +
            "': " + commitMessage;
        return false;
    }
    if (!hadDestination) return true;
    std::filesystem::remove(backup, filesystemError);
    if (!filesystemError) return true;
    error = "package lifecycle file was committed but its backup cannot be removed: " +
        filesystemError.message();
    return false;
}

bool writeTextAtomically(const std::filesystem::path &destination, std::string_view text, std::string &error)
{
    if (destination.empty())
    {
        error = "package lifecycle destination is empty";
        return false;
    }
    std::error_code filesystemError;
    const std::filesystem::path parent = destination.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError)
        {
            error = "cannot create package lifecycle directory '" + parent.string() + "': " + filesystemError.message();
            return false;
        }
    }
    const std::filesystem::path temporary = destination.string() + ".tmp";
    const std::filesystem::path backup = destination.string() + ".previous";
    if (!writeLifecycleTemporary(temporary, text, error)) return false;
    bool hadDestination = false;
    if (!preserveLifecycleDestination(
            destination, temporary, backup, hadDestination, error))
        return false;
    return commitLifecycleDestination(
        destination, temporary, backup, hadDestination, error);
}

std::map<std::string, ManifestEntry> byPath(const std::vector<ManifestEntry> &entries)
{
    std::map<std::string, ManifestEntry> result;
    for (const ManifestEntry &entry : entries)
    {
        result.emplace(entry.path, entry);
    }
    return result;
}

std::vector<PlanEntry> expectedPlanEntries(
    const std::map<std::string, ManifestEntry> &current, const std::map<std::string, ManifestEntry> &next)
{
    std::set<std::string> paths;
    for (const auto &[path, entry] : current)
    {
        (void)entry;
        paths.insert(path);
    }
    for (const auto &[path, entry] : next)
    {
        (void)entry;
        paths.insert(path);
    }

    std::vector<PlanEntry> result;
    result.reserve(paths.size());
    for (const std::string &path : paths)
    {
        const auto before = current.find(path);
        const auto after = next.find(path);
        std::string action;
        if (before == current.end())
        {
            action = "add";
        }
        else if (after == next.end())
        {
            action = "remove";
        }
        else if (before->second.digest == after->second.digest && before->second.size == after->second.size)
        {
            action = "keep";
        }
        else
        {
            action = "replace";
        }
        result.push_back({std::move(action), path});
    }
    return result;
}

bool writePlan(const std::filesystem::path &currentManifest,
    const std::filesystem::path *nextManifest,
    const std::filesystem::path &destination,
    std::string &error)
{
    std::vector<ManifestEntry> currentEntries;
    if (!readManifest(currentManifest, currentEntries, error))
    {
        return false;
    }
    std::vector<ManifestEntry> nextEntries;
    if (nextManifest != nullptr && !readManifest(*nextManifest, nextEntries, error))
    {
        return false;
    }

    std::string currentDigest;
    if (!fileDigest(currentManifest, currentDigest, error))
    {
        return false;
    }
    std::string nextDigest = "none";
    if (nextManifest != nullptr && !fileDigest(*nextManifest, nextDigest, error))
    {
        return false;
    }

    const auto current = byPath(currentEntries);
    const auto next = byPath(nextEntries);

    std::string text = "NSUPDATE " + std::to_string(kInstallationUpdatePlanFormatVersion) +
                       "\ncurrent_manifest_sha256=" + currentDigest + "\nnext_manifest_sha256=" + nextDigest + "\n";
    for (const PlanEntry &entry : expectedPlanEntries(current, next))
    {
        text += entry.action + '|' + entry.path + '\n';
    }
    return writeTextAtomically(destination, text, error);
}

bool readPlanIdentities(
    std::ifstream& stream,
    std::string& currentDigest,
    std::string& nextDigest,
    std::string& error)
{
    std::string line;
    if (!std::getline(stream, line))
    {
        error = "installation update plan is empty";
        return false;
    }
    removeCarriageReturn(line);
    if (line != "NSUPDATE 1")
    {
        error = "unsupported installation update plan header '" + line + "'";
        return false;
    }
    if (!std::getline(stream, line))
    {
        error = "installation update plan is missing current manifest identity";
        return false;
    }
    removeCarriageReturn(line);
    constexpr std::string_view currentPrefix = "current_manifest_sha256=";
    if (!line.starts_with(currentPrefix) ||
        !isLowerDigest(std::string_view(line).substr(currentPrefix.size())))
    {
        error = "installation update plan has invalid current manifest identity";
        return false;
    }
    currentDigest = line.substr(currentPrefix.size());
    if (!std::getline(stream, line))
    {
        error = "installation update plan is missing next manifest identity";
        return false;
    }
    removeCarriageReturn(line);
    constexpr std::string_view nextPrefix = "next_manifest_sha256=";
    if (!line.starts_with(nextPrefix))
    {
        error = "installation update plan has invalid next manifest identity";
        return false;
    }
    const std::string_view nextValue = std::string_view(line).substr(nextPrefix.size());
    if (nextValue != "none" && !isLowerDigest(nextValue))
    {
        error = "installation update plan has invalid next manifest identity";
        return false;
    }
    nextDigest = std::string(nextValue);
    return true;
}

bool readPlanEntries(
    std::ifstream& stream,
    std::vector<PlanEntry>& entries,
    std::string& error)
{
    std::string line;
    std::string previousPath;
    while (std::getline(stream, line))
    {
        removeCarriageReturn(line);
        const std::size_t separator = line.find('|');
        if (line.size() > kMaximumLineLength || separator == std::string::npos ||
            line.find('|', separator + 1U) != std::string::npos)
        {
            error = "malformed installation update plan entry";
            return false;
        }
        PlanEntry entry{line.substr(0, separator), line.substr(separator + 1U)};
        if ((entry.action != "add" && entry.action != "keep" &&
             entry.action != "remove" && entry.action != "replace") ||
            !isCanonicalManagedPath(entry.path) ||
            (!previousPath.empty() && entry.path <= previousPath))
        {
            error = "invalid installation update plan entry for '" + entry.path + "'";
            return false;
        }
        previousPath = entry.path;
        entries.push_back(std::move(entry));
        if (entries.size() > kMaximumEntries)
        {
            error = "installation update plan exceeds 10000 entries";
            return false;
        }
    }
    if (!stream.bad() && !entries.empty()) return true;
    error = stream.bad()
        ? "cannot read installation update plan"
        : "installation update plan has no entries";
    return false;
}

bool readPlan(const std::filesystem::path &source,
    std::string &currentDigest,
    std::string &nextDigest,
    std::vector<PlanEntry> &entries,
    std::string &error)
{
    entries.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open installation update plan '" + source.string() + "'";
        return false;
    }
    if (!readPlanIdentities(stream, currentDigest, nextDigest, error)) return false;
    return readPlanEntries(stream, entries, error);
}

bool containsLinkComponent(const std::filesystem::path &prefix, std::string_view relative, std::string &error)
{
    std::filesystem::path current = prefix;
    const std::filesystem::path relativePath{std::string(relative)};
    for (const std::filesystem::path &component : relativePath)
    {
        current /= component;
        std::error_code statusError;
        const std::filesystem::file_status status = std::filesystem::symlink_status(current, statusError);
        if (statusError && statusError != std::errc::no_such_file_or_directory)
        {
            error = "cannot inspect installation path component '" + current.string() + "': " + statusError.message();
            return true;
        }
        if (!statusError && std::filesystem::is_symlink(status))
        {
            error = "installation removal refuses symbolic-link path component '" + current.string() + "'";
            return true;
        }
    }
    return false;
}

bool validateRemovalPrefix(
    const std::filesystem::path& prefix,
    std::string& error)
{
    if (prefix.empty())
    {
        error = "installation removal prefix is empty";
        return false;
    }
    std::error_code prefixError;
    const std::filesystem::file_status prefixStatus =
        std::filesystem::symlink_status(prefix, prefixError);
    if (prefixError || !std::filesystem::is_directory(prefixStatus))
    {
        error = "installation removal prefix is not an accessible directory '" +
            prefix.string() + "'";
        return false;
    }
    if (!std::filesystem::is_symlink(prefixStatus)) return true;
    error = "installation removal refuses symbolic-link prefix '" + prefix.string() + "'";
    return false;
}

bool loadRemovalPlanState(
    const std::filesystem::path& currentManifest,
    const std::filesystem::path& nextManifest,
    const std::filesystem::path& plan,
    std::vector<ManifestEntry>& currentEntries,
    std::vector<ManifestEntry>& nextEntries,
    std::vector<PlanEntry>& planEntries,
    std::string& expectedNextDigest,
    std::string& error)
{
    if (!readManifest(currentManifest, currentEntries, error)) return false;
    std::string expectedDigest;
    if (!readPlan(plan, expectedDigest, expectedNextDigest, planEntries, error)) return false;
    std::string actualDigest;
    if (!fileDigest(currentManifest, actualDigest, error)) return false;
    if (actualDigest != expectedDigest)
    {
        error = "installation update plan was created for manifest " + expectedDigest +
            ", not " + actualDigest;
        return false;
    }
    if (expectedNextDigest == "none")
    {
        if (nextManifest.empty()) return true;
        error = "retirement plan must not name a next installation manifest";
        return false;
    }
    if (nextManifest.empty())
    {
        error = "update plan requires its bound next installation manifest";
        return false;
    }
    if (!readManifest(nextManifest, nextEntries, error)) return false;
    std::string actualNextDigest;
    if (!fileDigest(nextManifest, actualNextDigest, error)) return false;
    if (actualNextDigest == expectedNextDigest) return true;
    error = "installation update plan was created for next manifest " +
        expectedNextDigest + ", not " + actualNextDigest;
    return false;
}

bool validateCanonicalRemovalPlan(
    const std::map<std::string, ManifestEntry>& current,
    const std::map<std::string, ManifestEntry>& next,
    const std::vector<PlanEntry>& planEntries,
    std::string& error)
{
    const std::vector<PlanEntry> canonicalPlan = expectedPlanEntries(current, next);
    if (canonicalPlan.size() != planEntries.size())
    {
        error = "installation update plan actions do not match its bound manifests";
        return false;
    }
    for (std::size_t index = 0; index < canonicalPlan.size(); ++index)
    {
        if (canonicalPlan[index].action == planEntries[index].action &&
            canonicalPlan[index].path == planEntries[index].path)
            continue;
        error = "installation update plan action for '" + canonicalPlan[index].path +
            "' does not match its bound manifests";
        return false;
    }
    return true;
}

bool collectRemovalTargets(
    const std::filesystem::path& prefix,
    const std::map<std::string, ManifestEntry>& current,
    const std::vector<PlanEntry>& planEntries,
    std::vector<RemovalTarget>& removals,
    std::string& error)
{
    for (const PlanEntry& entry : planEntries)
    {
        if (entry.action != "remove") continue;
        if (!current.contains(entry.path))
        {
            error = "installation update plan attempts to remove unowned path '" +
                entry.path + "'";
            return false;
        }
        if (containsLinkComponent(prefix, entry.path, error)) return false;
        const std::filesystem::path target = prefix / entry.path;
        std::error_code filesystemError;
        const bool exists = std::filesystem::exists(target, filesystemError);
        if (filesystemError)
        {
            error = "cannot inspect installation removal target '" + target.string() +
                "': " + filesystemError.message();
            return false;
        }
        if (!exists) continue;
        if (!std::filesystem::is_regular_file(target, filesystemError) || filesystemError)
        {
            error = "installation removal target is not a regular file '" + target.string() + "'";
            return false;
        }
        std::string actualFileDigest;
        std::uintmax_t actualFileSize = 0;
        if (!fileIdentity(target, actualFileDigest, actualFileSize, error)) return false;
        const ManifestEntry& owned = current.at(entry.path);
        if (actualFileDigest != owned.digest || actualFileSize != owned.size)
        {
            error = "installation removal refuses modified package-owned file '" +
                target.string() + "'";
            return false;
        }
        removals.push_back({target});
    }
    return true;
}

bool appendRetirementManifest(
    const std::filesystem::path& prefix,
    const std::filesystem::path& currentManifest,
    std::string_view expectedNextDigest,
    std::vector<RemovalTarget>& removals,
    std::string& error)
{
    // NSINSTALL cannot hash itself without becoming recursive. For a full retirement, the exact
    // manifest passed as authority is nevertheless package state and must disappear when it lives
    // under the target prefix. An external evidence copy remains untouched.
    if (expectedNextDigest != "none") return true;
    const std::filesystem::path relativeManifest =
        currentManifest.lexically_normal().lexically_relative(prefix.lexically_normal());
    const std::string relativeManifestText = relativeManifest.generic_string();
    if (relativeManifest.empty() || !isCanonicalManagedPath(relativeManifestText)) return true;
    if (containsLinkComponent(prefix, relativeManifestText, error)) return false;
    std::error_code manifestStatusError;
    const bool manifestExists = std::filesystem::exists(currentManifest, manifestStatusError);
    if (manifestStatusError ||
        (manifestExists &&
         !std::filesystem::is_regular_file(currentManifest, manifestStatusError)))
    {
        error = "installation manifest retirement target is not a regular file '" +
            currentManifest.string() + "'";
        return false;
    }
    if (manifestExists) removals.push_back({currentManifest});
    return true;
}

bool removeInstallationTargets(
    const std::vector<RemovalTarget>& removals,
    std::string& error)
{
    for (const RemovalTarget& removal : removals)
    {
        std::error_code filesystemError;
        if (!std::filesystem::remove(removal.path, filesystemError) || filesystemError)
        {
            error = "cannot remove installed file '" + removal.path.string() +
                "': " + filesystemError.message();
            return false;
        }
    }
    return true;
}

bool pruneInstallationDirectories(
    const std::filesystem::path& prefix,
    const std::vector<RemovalTarget>& removals,
    std::string& error)
{
    // Remove only empty package subdirectories. The shared prefix plus its bin/lib/share roots are
    // deliberate boundaries and always survive.
    std::set<std::filesystem::path, std::greater<>> candidateDirectories;
    for (const RemovalTarget& removal : removals)
    {
        std::filesystem::path directory = removal.path.parent_path();
        while (!directory.empty() && directory != prefix && directory != prefix / "bin" &&
               directory != prefix / "lib" && directory != prefix / "share")
        {
            candidateDirectories.insert(directory);
            directory = directory.parent_path();
        }
    }
    for (const std::filesystem::path& directory : candidateDirectories)
    {
        std::error_code directoryError;
        std::filesystem::remove(directory, directoryError);
        if (directoryError && directoryError != std::errc::directory_not_empty &&
            directoryError != std::errc::no_such_file_or_directory)
        {
            error = "cannot prune empty package directory '" + directory.string() +
                "': " + directoryError.message();
            return false;
        }
    }
    return true;
}
} // namespace

bool compareInstallationManifests(const std::filesystem::path &left,
    const std::filesystem::path &right,
    ReproducibilityComparison &comparison,
    std::string &error)
{
    comparison = {};
    error.clear();
    std::vector<ManifestEntry> leftEntries;
    std::vector<ManifestEntry> rightEntries;
    if (!readManifest(left, leftEntries, error) || !readManifest(right, rightEntries, error))
    {
        return false;
    }
    const auto leftByPath = byPath(leftEntries);
    const auto rightByPath = byPath(rightEntries);
    std::set<std::string> paths;
    for (const auto &[path, entry] : leftByPath)
    {
        (void)entry;
        paths.insert(path);
    }
    for (const auto &[path, entry] : rightByPath)
    {
        (void)entry;
        paths.insert(path);
    }

    std::string details;
    for (const std::string &path : paths)
    {
        const auto leftEntry = leftByPath.find(path);
        const auto rightEntry = rightByPath.find(path);
        if (leftEntry == leftByPath.end() || rightEntry == rightByPath.end())
        {
            ++comparison.differences;
            details += "difference=path-set|" + path + '\n';
            continue;
        }
        if (isBuildIdentityPath(path))
        {
            ++comparison.volatileEntries;
            details += "volatile=build-identity|" + path + '\n';
            continue;
        }
        ++comparison.deterministicEntries;
        if (leftEntry->second.digest != rightEntry->second.digest || leftEntry->second.size != rightEntry->second.size)
        {
            ++comparison.differences;
            details += "difference=content|" + path + '\n';
        }
    }
    comparison.reproducible = comparison.differences == 0;
    comparison.report = "NSREPRO " + std::to_string(kReproducibilityReportFormatVersion) +
                        "\nresult=" + std::string(comparison.reproducible ? "reproducible" : "different") +
                        "\ndeterministic_entries=" + std::to_string(comparison.deterministicEntries) +
                        "\nvolatile_entries=" + std::to_string(comparison.volatileEntries) +
                        "\ndifferences=" + std::to_string(comparison.differences) + "\n" + details;
    return true;
}

bool writeInstallationUpdatePlan(const std::filesystem::path &currentManifest,
    const std::filesystem::path &nextManifest,
    const std::filesystem::path &destination,
    std::string &error)
{
    return writePlan(currentManifest, &nextManifest, destination, error);
}

bool writeInstallationRemovalPlan(
    const std::filesystem::path &currentManifest, const std::filesystem::path &destination, std::string &error)
{
    return writePlan(currentManifest, nullptr, destination, error);
}

bool applyInstallationRemovalPlan(const std::filesystem::path &prefix,
    const std::filesystem::path &currentManifest,
    const std::filesystem::path &nextManifest,
    const std::filesystem::path &plan,
    std::string &error)
{
    error.clear();
    if (!validateRemovalPrefix(prefix, error)) return false;

    std::vector<ManifestEntry> currentEntries;
    std::vector<ManifestEntry> nextEntries;
    std::vector<PlanEntry> planEntries;
    std::string expectedNextDigest;
    if (!loadRemovalPlanState(
            currentManifest, nextManifest, plan, currentEntries, nextEntries,
            planEntries, expectedNextDigest, error))
        return false;

    const auto current = byPath(currentEntries);
    const auto next = byPath(nextEntries);
    if (!validateCanonicalRemovalPlan(current, next, planEntries, error)) return false;

    std::vector<RemovalTarget> removals;
    if (!collectRemovalTargets(prefix, current, planEntries, removals, error)) return false;
    if (!appendRetirementManifest(
            prefix, currentManifest, expectedNextDigest, removals, error))
        return false;
    if (!removeInstallationTargets(removals, error)) return false;
    return pruneInstallationDirectories(prefix, removals, error);
}

} // namespace nativesharp::driver
