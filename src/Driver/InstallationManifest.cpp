#include "nativesharp/Driver/InstallationManifest.h"
#include "Internal/ManifestEntry.h"

#include "nativesharp/Basic/Sha256.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <set>
#include <string_view>
#include <system_error>
#include <vector>

namespace nativesharp::driver {
namespace {
constexpr std::size_t kMaximumEntries = 10'000;
constexpr std::size_t kMaximumLineLength = 16'384;

void removeCarriageReturn(std::string &line) {
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
}

bool isManagedPath(std::string_view path) {
  return path.starts_with("bin/") || path.starts_with("lib/") ||
         path.starts_with("sdk/win64/") ||
         path.starts_with("share/nativesharp/");
}

bool isCanonicalRelativePath(std::string_view text) {
  if (text.empty() || text.front() == '/' ||
      text.find('\\') != std::string_view::npos ||
      text.find('|') != std::string_view::npos ||
      text.find('\r') != std::string_view::npos ||
      text.find('\n') != std::string_view::npos ||
      text.find(':') != std::string_view::npos || !isManagedPath(text)) {
    return false;
  }

  const std::filesystem::path path{std::string(text)};
  if (path.is_absolute() || path.generic_string() != text ||
      path.lexically_normal().generic_string() != text) {
    return false;
  }
  return std::none_of(path.begin(), path.end(),
                      [](const std::filesystem::path &part) {
                        return part == "." || part == "..";
                      });
}

bool readHeader(std::ifstream &stream, std::string_view kind,
                std::string_view expected, std::string &error) {
  std::string header;
  if (!std::getline(stream, header)) {
    error = std::string(kind) + " is empty";
    return false;
  }
  removeCarriageReturn(header);
  if (header != expected) {
    error = "unsupported " + std::string(kind) + " header '" + header +
            "'; expected '" + std::string(expected) + "'";
    return false;
  }
  return true;
}

bool readExpectedFiles(const std::filesystem::path &source,
                       std::vector<std::string> &paths, std::string &error) {
  std::ifstream stream(source, std::ios::binary);
  if (!stream) {
    error = "cannot open installation file list '" + source.string() + "'";
    return false;
  }
  const std::string expectedHeader =
      "NSFILES " + std::to_string(kInstallationFileListFormatVersion);
  if (!readHeader(stream, "installation file list", expectedHeader, error)) {
    return false;
  }

  std::string line;
  while (std::getline(stream, line)) {
    removeCarriageReturn(line);
    if (line.size() > kMaximumLineLength || !isCanonicalRelativePath(line)) {
      error = "non-canonical installation file list path '" + line + "'";
      return false;
    }
    if (!paths.empty() && line <= paths.back()) {
      error = line == paths.back()
                  ? "duplicate installation file list path '" + line + "'"
                  : "installation file list path '" + line +
                        "' is out of canonical order";
      return false;
    }
    paths.push_back(line);
    if (paths.size() > kMaximumEntries) {
      error = "installation file list exceeds 10000 entries";
      return false;
    }
  }
  if (stream.bad()) {
    error = "cannot read installation file list '" + source.string() + "'";
    return false;
  }
  if (paths.empty()) {
    error = "installation file list has no entries";
    return false;
  }
  return true;
}

bool isLowerHexDigest(std::string_view value) {
  return value.size() == 64U &&
         std::all_of(value.begin(), value.end(), [](char character) {
           return (character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f');
         });
}

bool parseSize(std::string_view text, std::uintmax_t &size) {
  if (text.empty()) {
    return false;
  }
  const char *begin = text.data();
  const char *end = begin + text.size();
  const auto result = std::from_chars(begin, end, size);
  return result.ec == std::errc{} && result.ptr == end &&
         std::to_string(size) == text;
}

bool readManifest(const std::filesystem::path &source,
                  std::vector<ManifestEntry> &entries, std::string &error) {
  std::ifstream stream(source, std::ios::binary);
  if (!stream) {
    error = "cannot open installation manifest '" + source.string() + "'";
    return false;
  }
  const std::string expectedHeader =
      "NSINSTALL " + std::to_string(kInstallationManifestFormatVersion);
  if (!readHeader(stream, "installation manifest", expectedHeader, error)) {
    return false;
  }

  std::string line;
  std::size_t lineNumber = 1;
  while (std::getline(stream, line)) {
    ++lineNumber;
    removeCarriageReturn(line);
    const std::size_t first = line.find('|');
    const std::size_t second =
        first == std::string::npos ? first : line.find('|', first + 1U);
    if (line.size() > kMaximumLineLength || first == std::string::npos ||
        second == std::string::npos ||
        line.find('|', second + 1U) != std::string::npos) {
      error =
          "malformed installation manifest line " + std::to_string(lineNumber);
      return false;
    }

    ManifestEntry entry;
    entry.sha256 = line.substr(0, first);
    const std::string_view sizeText(line.data() + first + 1U,
                                    second - first - 1U);
    entry.relativePath = line.substr(second + 1U);
    if (!isLowerHexDigest(entry.sha256)) {
      error = "invalid SHA-256 in installation manifest for '" +
              entry.relativePath + "'";
      return false;
    }
    if (!parseSize(sizeText, entry.size)) {
      error = "invalid size in installation manifest for '" +
              entry.relativePath + "'";
      return false;
    }
    if (!isCanonicalRelativePath(entry.relativePath)) {
      error = "non-canonical installation manifest path '" +
              entry.relativePath + "'";
      return false;
    }
    if (!entries.empty() && entry.relativePath <= entries.back().relativePath) {
      error = entry.relativePath == entries.back().relativePath
                  ? "duplicate installation manifest path '" +
                        entry.relativePath + "'"
                  : "installation manifest path '" + entry.relativePath +
                        "' is out of canonical order";
      return false;
    }
    entries.push_back(std::move(entry));
    if (entries.size() > kMaximumEntries) {
      error = "installation manifest exceeds 10000 entries";
      return false;
    }
  }
  if (stream.bad()) {
    error = "cannot read installation manifest '" + source.string() + "'";
    return false;
  }
  if (entries.empty()) {
    error = "installation manifest has no entries";
    return false;
  }
  return true;
}

bool relativeManifestPath(const std::filesystem::path &prefix,
                          const std::filesystem::path &manifest,
                          std::string &relative) {
  std::error_code filesystemError;
  const std::filesystem::path path =
      std::filesystem::relative(manifest, prefix, filesystemError);
  if (filesystemError) {
    return false;
  }
  relative = path.generic_string();
  return isCanonicalRelativePath(relative);
}

bool rejectUnexpectedFiles(const std::filesystem::path &prefix,
                           const std::filesystem::path &manifest,
                           const std::set<std::string> &expected,
                           std::string &error) {
  std::string manifestRelative;
  const bool manifestInsidePrefix =
      relativeManifestPath(prefix, manifest, manifestRelative);
  // bin/ and lib/ are shared package directories. Authenticate their expected
  // files exactly, but scan for residue only below directory namespaces that
  // this package actually selected. This keeps NSLink/NativeAsm executables and
  // archives from becoming accidental property of the compiler package.
  std::set<std::string> roots = {"share/nativesharp/toolchain", "sdk/win64"};
  for (const std::string &relative : expected) {
    if (!relative.starts_with("lib/")) {
      continue;
    }
    const std::size_t componentEnd = relative.find('/', 4U);
    if (componentEnd != std::string::npos) {
      roots.insert(relative.substr(0, componentEnd));
    }
  }
  for (const std::string &root : roots) {
    const std::filesystem::path directory =
        prefix / std::filesystem::path(root);
    std::error_code filesystemError;
    if (!std::filesystem::exists(directory, filesystemError)) {
      if (filesystemError) {
        error = "cannot inspect installed directory '" + directory.string() +
                "': " + filesystemError.message();
        return false;
      }
      continue;
    }

    std::filesystem::recursive_directory_iterator iterator(directory,
                                                           filesystemError);
    const std::filesystem::recursive_directory_iterator end;
    while (!filesystemError && iterator != end) {
      const std::filesystem::directory_entry entry = *iterator;
      iterator.increment(filesystemError);
      std::error_code statusError;
      if (entry.is_directory(statusError)) {
        continue;
      }
      if (statusError || !entry.is_regular_file(statusError) || statusError) {
        error = "unsupported non-regular installation entry '" +
                entry.path().string() + "'";
        return false;
      }

      std::error_code relativeError;
      const std::string relative =
          std::filesystem::relative(entry.path(), prefix, relativeError)
              .generic_string();
      if (relativeError) {
        error =
            "cannot normalize installed path '" + entry.path().string() + "'";
        return false;
      }
      if (manifestInsidePrefix && relative == manifestRelative) {
        continue;
      }
      if (!expected.contains(relative)) {
        error = "unexpected installed file '" + relative + "'";
        return false;
      }
    }
    if (filesystemError) {
      error = "cannot enumerate installed directory '" + directory.string() +
              "': " + filesystemError.message();
      return false;
    }
  }
  return true;
}
} // namespace

bool writeInstallationManifest(const std::filesystem::path &prefix,
                               const std::filesystem::path &expectedFiles,
                               const std::filesystem::path &destination,
                               std::string &error) {
  error.clear();
  if (prefix.empty() || destination.empty()) {
    error = "installation manifest prefix and destination must not be empty";
    return false;
  }

  std::vector<std::string> paths;
  if (!readExpectedFiles(expectedFiles, paths, error)) {
    return false;
  }
  std::string destinationRelative;
  if (relativeManifestPath(prefix, destination, destinationRelative) &&
      std::binary_search(paths.begin(), paths.end(), destinationRelative)) {
    error = "installation manifest cannot inventory itself at '" +
            destinationRelative + "'";
    return false;
  }

  // Read and hash the whole selected payload before opening the destination. A
  // missing or unreadable input must not destroy the last known-good manifest
  // with a truncated replacement.
  std::vector<ManifestEntry> entries;
  entries.reserve(paths.size());
  for (const std::string &relative : paths) {
    const std::filesystem::path path = prefix / std::filesystem::path(relative);
    std::error_code regularError;
    if (!std::filesystem::is_regular_file(path, regularError) || regularError) {
      error = "cannot inventory missing installed file '" + relative + "'";
      return false;
    }
    ManifestEntry entry;
    basic::Sha256Digest digest{};
    if (!basic::sha256File(path, digest, entry.size, error)) {
      return false;
    }
    entry.sha256 = basic::sha256Hex(digest);
    entry.relativePath = relative;
    entries.push_back(std::move(entry));
  }

  const std::filesystem::path parent = destination.parent_path();
  std::error_code filesystemError;
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, filesystemError);
    if (filesystemError) {
      error = "cannot create installation manifest directory '" +
              parent.string() + "': " + filesystemError.message();
      return false;
    }
  }

  std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
  if (!stream) {
    error = "cannot open installation manifest destination '" +
            destination.string() + "'";
    return false;
  }
  stream << "NSINSTALL " << kInstallationManifestFormatVersion << '\n';
  for (const ManifestEntry &entry : entries) {
    stream << entry.sha256 << '|' << entry.size << '|' << entry.relativePath
           << '\n';
    if (!stream) {
      error = "cannot write installation manifest destination '" +
              destination.string() + "'";
      return false;
    }
  }
  return true;
}

bool verifyInstallationManifest(const std::filesystem::path &prefix,
                                const std::filesystem::path &manifest,
                                std::string &error) {
  error.clear();
  if (prefix.empty() || manifest.empty()) {
    error = "installation manifest prefix and path must not be empty";
    return false;
  }
  std::vector<ManifestEntry> entries;
  if (!readManifest(manifest, entries, error)) {
    return false;
  }

  std::string manifestRelative;
  if (relativeManifestPath(prefix, manifest, manifestRelative) &&
      std::any_of(entries.begin(), entries.end(),
                  [&](const ManifestEntry &entry) {
                    return entry.relativePath == manifestRelative;
                  })) {
    error = "installation manifest cannot inventory itself at '" +
            manifestRelative + "'";
    return false;
  }

  std::set<std::string> expected;
  for (const ManifestEntry &entry : entries) {
    const std::filesystem::path path =
        prefix / std::filesystem::path(entry.relativePath);
    std::error_code regularError;
    if (!std::filesystem::is_regular_file(path, regularError) || regularError) {
      error =
          "installation manifest entry is missing '" + entry.relativePath + "'";
      return false;
    }
    basic::Sha256Digest digest{};
    std::uintmax_t actualSize = 0;
    if (!basic::sha256File(path, digest, actualSize, error)) {
      return false;
    }
    if (actualSize != entry.size) {
      error = "installation manifest size mismatch for '" + entry.relativePath +
              "': found " + std::to_string(actualSize) + ", expected " +
              std::to_string(entry.size);
      return false;
    }
    const std::string actualHash = basic::sha256Hex(digest);
    if (actualHash != entry.sha256) {
      error = "installation manifest SHA-256 mismatch for '" +
              entry.relativePath + "': found " + actualHash + ", expected " +
              entry.sha256;
      return false;
    }
    expected.insert(entry.relativePath);
  }
  return rejectUnexpectedFiles(prefix, manifest, expected, error);
}
} // namespace nativesharp::driver
