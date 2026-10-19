#include "nativesharp/Compilation/SdkManifest.h"

#include "nativesharp/Compilation/SimpleXmlParser.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace nativesharp::compilation
{
namespace
{
std::string trim(std::string_view value)
{
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
    {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

const std::string* attribute(const SimpleXmlElement& element, std::string_view name)
{
    const auto found = std::find_if(
        element.attributes.begin(),
        element.attributes.end(),
        [name](const auto& item) { return item.first == name; });
    return found == element.attributes.end() ? nullptr : &found->second;
}

bool isIdentifier(std::string_view value)
{
    if (value.empty())
    {
        return false;
    }
    const auto isStart = [](unsigned char character)
    {
        return std::isalpha(character) != 0 || character == '_';
    };
    if (!isStart(static_cast<unsigned char>(value.front())))
    {
        return false;
    }
    return std::all_of(
        value.begin() + 1,
        value.end(),
        [isStart](unsigned char character)
        {
            return isStart(character) || std::isdigit(character) != 0;
        });
}

bool hasOnlyAttributes(
    const SimpleXmlElement& element,
    std::initializer_list<std::string_view> allowed,
    std::string& error)
{
    for (const auto& [name, value] : element.attributes)
    {
        static_cast<void>(value);
        if (std::find(allowed.begin(), allowed.end(), name) == allowed.end())
        {
            error = "SDK element '" + element.name + "' has unsupported attribute '" + name + "'";
            return false;
        }
    }
    return true;
}

bool requireAttribute(
    const SimpleXmlElement& element,
    std::string_view name,
    std::string& value,
    std::string& error)
{
    const std::string* found = attribute(element, name);
    if (found == nullptr || trim(*found).empty())
    {
        error = "SDK element '" + element.name + "' requires non-empty attribute '" +
            std::string(name) + "'";
        return false;
    }
    value = trim(*found);
    return true;
}

bool parseMappings(const SimpleXmlElement& mappings, SdkManifest& manifest, std::string& error)
{
    if (!mappings.attributes.empty() || !trim(mappings.text).empty())
    {
        error = "SDK Mappings does not accept attributes or text";
        return false;
    }
    std::unordered_set<std::string> keys;
    for (const SimpleXmlElement& mapping : mappings.children)
    {
        if (!mapping.children.empty() || !trim(mapping.text).empty())
        {
            error = "SDK mapping '" + mapping.name + "' must be empty";
            return false;
        }
        std::string first;
        std::string second;
        std::string third;
        std::string key;
        if (mapping.name == "Header")
        {
            if (!hasOnlyAttributes(mapping, {"Include", "Namespace"}, error) ||
                !requireAttribute(mapping, "Include", first, error) ||
                !requireAttribute(mapping, "Namespace", second, error)) return false;
            key = "H\n" + first;
            manifest.headers.push_back(SdkHeaderMapping{first, second});
        }
        else if (mapping.name == "Type")
        {
            if (!hasOnlyAttributes(mapping, {"Native", "NativeSharp"}, error) ||
                !requireAttribute(mapping, "Native", first, error) ||
                !requireAttribute(mapping, "NativeSharp", second, error)) return false;
            key = "T\n" + first;
            manifest.types.push_back(SdkTypeMapping{first, second});
        }
        else if (mapping.name == "Field")
        {
            if (!hasOnlyAttributes(mapping, {"Type", "Native", "NativeSharp"}, error) ||
                !requireAttribute(mapping, "Type", first, error) ||
                !requireAttribute(mapping, "Native", second, error) ||
                !requireAttribute(mapping, "NativeSharp", third, error)) return false;
            key = "F\n" + first + "\n" + second;
            manifest.fields.push_back(SdkFieldMapping{first, second, third});
        }
        else if (mapping.name == "Define")
        {
            if (!hasOnlyAttributes(mapping, {"Name", "Value"}, error) ||
                !requireAttribute(mapping, "Name", first, error) ||
                !requireAttribute(mapping, "Value", second, error)) return false;
            key = "D\n" + first;
            manifest.defines.push_back(SdkDefineMapping{first, second});
        }
        else
        {
            error = "unsupported SDK mapping '" + mapping.name + "'";
            return false;
        }
        if (!keys.insert(key).second)
        {
            error = "duplicate SDK mapping key '" + first + "'";
            return false;
        }
    }
    return true;
}

bool parseVersion(std::string_view text, std::vector<std::uint32_t>& parts)
{
    parts.clear();
    std::size_t begin = 0;
    while (begin <= text.size())
    {
        const std::size_t end = text.find('.', begin);
        const std::string_view part = text.substr(
            begin, end == std::string_view::npos ? text.size() - begin : end - begin);
        if (part.empty()) return false;
        std::uint32_t value = 0;
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size()) return false;
        parts.push_back(value);
        if (parts.size() > 8U) return false;
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    while (parts.size() > 1U && parts.back() == 0U) parts.pop_back();
    return !parts.empty();
}

int compareVersions(std::string_view left, std::string_view right)
{
    std::vector<std::uint32_t> leftParts;
    std::vector<std::uint32_t> rightParts;
    static_cast<void>(parseVersion(left, leftParts));
    static_cast<void>(parseVersion(right, rightParts));
    const std::size_t count = std::max(leftParts.size(), rightParts.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::uint32_t leftValue = index < leftParts.size() ? leftParts[index] : 0U;
        const std::uint32_t rightValue = index < rightParts.size() ? rightParts[index] : 0U;
        if (leftValue < rightValue) return -1;
        if (leftValue > rightValue) return 1;
    }
    return 0;
}

bool parseTargets(const SimpleXmlElement& targets, SdkManifest& manifest, std::string& error)
{
    if (!targets.attributes.empty() || !trim(targets.text).empty())
    {
        error = "SDK Targets does not accept attributes or text";
        return false;
    }
    for (const SimpleXmlElement& target : targets.children)
    {
        if (target.name != "Target" || !target.children.empty() || !trim(target.text).empty())
        {
            error = "SDK Targets accepts only empty Target elements";
            return false;
        }
        if (!hasOnlyAttributes(target, {"Architecture", "MinVersion", "MaxVersion"}, error))
            return false;
        std::string architecture;
        std::string minimum;
        std::string maximum;
        if (!requireAttribute(target, "Architecture", architecture, error) ||
            !requireAttribute(target, "MinVersion", minimum, error) ||
            !requireAttribute(target, "MaxVersion", maximum, error)) return false;
        std::vector<std::uint32_t> parts;
        if (!isIdentifier(architecture) || !parseVersion(minimum, parts) ||
            !parseVersion(maximum, parts) || compareVersions(minimum, maximum) > 0)
        {
            error = "SDK Target has invalid architecture or version range";
            return false;
        }
        for (const SdkTargetVariant& existing : manifest.targets)
        {
            if (existing.architecture == architecture &&
                compareVersions(minimum, existing.maximumVersion) <= 0 &&
                compareVersions(existing.minimumVersion, maximum) <= 0)
            {
                error = "SDK Target ranges overlap for architecture '" + architecture + "'";
                return false;
            }
        }
        manifest.targets.push_back(SdkTargetVariant{architecture, minimum, maximum});
    }
    std::sort(manifest.targets.begin(), manifest.targets.end(), [](const auto& left, const auto& right)
    {
        if (left.architecture != right.architecture) return left.architecture < right.architecture;
        return compareVersions(left.minimumVersion, right.minimumVersion) < 0;
    });
    return true;
}

bool parseLibraries(const SimpleXmlElement& libraries, SdkManifest& manifest, std::string& error)
{
    if (!libraries.attributes.empty() || !trim(libraries.text).empty())
    {
        error = "SDK Libraries does not accept attributes or text";
        return false;
    }
    std::unordered_set<std::string> names;
    for (const SimpleXmlElement& library : libraries.children)
    {
        if (library.name != "Library" || !library.children.empty() || !trim(library.text).empty())
        {
            error = "SDK Libraries accepts only empty Library elements";
            return false;
        }
        if (!hasOnlyAttributes(library, {"Name", "Path"}, error)) return false;
        std::string name;
        std::string pathText;
        if (!requireAttribute(library, "Name", name, error) ||
            !requireAttribute(library, "Path", pathText, error)) return false;
        const std::filesystem::path path(pathText);
        if (!isIdentifier(name) || path.is_absolute() || path.extension() != ".lib" ||
            std::find(path.begin(), path.end(), "..") != path.end())
        {
            error = "SDK Library requires an identifier and a relative .lib path without traversal";
            return false;
        }
        if (!names.insert(name).second)
        {
            error = "duplicate SDK Library name '" + name + "'";
            return false;
        }
        manifest.libraries.push_back(SdkLibraryDependency{name, path.lexically_normal()});
    }
    std::sort(manifest.libraries.begin(), manifest.libraries.end(), [](const auto& left, const auto& right)
    {
        return left.name < right.name;
    });
    return true;
}

std::optional<std::filesystem::path> canonicalManifestPath(
    const std::filesystem::path& manifestPath,
    std::string& error)
{
    std::error_code filesystemError;
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(manifestPath, filesystemError);
    if (filesystemError || !std::filesystem::is_regular_file(canonical))
    {
        error = "NSC-SDK-001: SDK manifest does not exist or is not a regular file: '" +
            manifestPath.string() + "'";
        return std::nullopt;
    }
    if (canonical.extension() == ".nssdk") return canonical;
    error = "NativeSharp SDK manifest must use the canonical .nssdk extension";
    return std::nullopt;
}

std::optional<std::string> readManifestSource(
    const std::filesystem::path& canonical,
    std::string& error)
{
    constexpr std::uintmax_t kMaximumManifestBytes = 4U * 1024U * 1024U;
    std::error_code filesystemError;
    const std::uintmax_t byteCount = std::filesystem::file_size(canonical, filesystemError);
    if (filesystemError || byteCount > kMaximumManifestBytes)
    {
        error = filesystemError ? "cannot determine SDK manifest size" :
            "SDK manifest exceeds the 4 MiB safety limit";
        return std::nullopt;
    }
    std::ifstream stream(canonical, std::ios::binary);
    std::string source(static_cast<std::size_t>(byteCount), '\0');
    stream.read(source.data(), static_cast<std::streamsize>(source.size()));
    if ((!stream && !source.empty()) ||
        stream.gcount() != static_cast<std::streamsize>(source.size()) ||
        stream.peek() != std::char_traits<char>::eof())
    {
        error = "cannot read SDK manifest '" + canonical.string() + "'";
        return std::nullopt;
    }
    return source;
}

bool validateManifestRoot(
    const SimpleXmlElement& root,
    const std::filesystem::path& canonical,
    std::string& error)
{
    if (root.name != "NativeSharpSdk")
    {
        error = canonical.string() + ": SDK root element must be 'NativeSharpSdk'";
        return false;
    }
    if (!trim(root.text).empty())
    {
        error = canonical.string() + ": NativeSharpSdk does not accept text";
        return false;
    }
    for (const auto& [name, value] : root.attributes)
    {
        static_cast<void>(value);
        if (name == "Name" || name == "FormatVersion") continue;
        error = canonical.string() + ": NativeSharpSdk has unsupported attribute '" + name + "'";
        return false;
    }
    return true;
}

bool initializeManifest(
    const SimpleXmlElement& root,
    const std::filesystem::path& canonical,
    SdkManifest& manifest,
    std::string& error)
{
    const std::string* name = attribute(root, "Name");
    const std::string* version = attribute(root, "FormatVersion");
    if (name == nullptr || !isIdentifier(trim(*name)))
    {
        error = canonical.string() + ": SDK Name must be a NativeSharp identifier";
        return false;
    }
    if (version == nullptr)
    {
        error = canonical.string() + ": SDK FormatVersion is required";
        return false;
    }
    const std::string versionText = trim(*version);
    std::uint32_t parsedVersion = 0;
    const auto parsed = std::from_chars(
        versionText.data(), versionText.data() + versionText.size(), parsedVersion);
    if (parsed.ec != std::errc{} || parsed.ptr != versionText.data() + versionText.size() ||
        parsedVersion != kSdkManifestFormatVersion)
    {
        error = canonical.string() + ": unsupported SDK FormatVersion '" + versionText +
            "'; expected 1";
        return false;
    }
    manifest.manifestPath = canonical;
    manifest.name = trim(*name);
    manifest.formatVersion = parsedVersion;
    return true;
}

bool parseManifestChildren(
    const SimpleXmlElement& root,
    const std::filesystem::path& canonical,
    SdkManifest& manifest,
    std::string& error)
{
    bool sawMappings = false;
    bool sawTargets = false;
    bool sawLibraries = false;
    for (const SimpleXmlElement& child : root.children)
    {
        bool parsed = false;
        if (child.name == "Mappings")
        {
            if (sawMappings)
            {
                error = canonical.string() + ": SDK Mappings is declared more than once";
                return false;
            }
            sawMappings = true;
            parsed = parseMappings(child, manifest, error);
        }
        else if (child.name == "Targets")
        {
            if (sawTargets)
            {
                error = canonical.string() + ": SDK Targets is declared more than once";
                return false;
            }
            sawTargets = true;
            parsed = parseTargets(child, manifest, error);
        }
        else if (child.name == "Libraries")
        {
            if (sawLibraries)
            {
                error = canonical.string() + ": SDK Libraries is declared more than once";
                return false;
            }
            sawLibraries = true;
            parsed = parseLibraries(child, manifest, error);
        }
        else
        {
            error = canonical.string() + ": unsupported NativeSharpSdk child '" + child.name + "'";
            return false;
        }
        if (parsed) continue;
        error = canonical.string() + ": " + error;
        return false;
    }
    return true;
}
}

std::optional<SdkManifest> SdkManifestLoader::load(
    const std::filesystem::path& manifestPath,
    std::string& error) const
{
    error.clear();
    const auto canonical = canonicalManifestPath(manifestPath, error);
    if (!canonical.has_value()) return std::nullopt;
    const auto source = readManifestSource(*canonical, error);
    if (!source.has_value()) return std::nullopt;

    SimpleXmlElement root;
    SimpleXmlParser parser;
    if (!parser.parse(*source, root, error))
    {
        error = canonical->string() + ": " + error;
        return std::nullopt;
    }
    if (!validateManifestRoot(root, *canonical, error)) return std::nullopt;
    SdkManifest manifest;
    if (!initializeManifest(root, *canonical, manifest, error)) return std::nullopt;
    if (!parseManifestChildren(root, *canonical, manifest, error)) return std::nullopt;
    return manifest;
}

const SdkTargetVariant* SdkTargetSelector::select(
    const SdkManifest& manifest,
    std::string_view architecture,
    std::string_view version,
    std::string& error) const
{
    error.clear();
    std::vector<std::uint32_t> parts;
    if (!isIdentifier(architecture) || !parseVersion(version, parts))
    {
        error = "invalid SDK target architecture or version";
        return nullptr;
    }
    const SdkTargetVariant* selected = nullptr;
    for (const SdkTargetVariant& target : manifest.targets)
    {
        if (target.architecture == architecture &&
            compareVersions(version, target.minimumVersion) >= 0 &&
            compareVersions(version, target.maximumVersion) <= 0)
        {
            selected = &target;
            break;
        }
    }
    if (selected == nullptr)
    {
        error = "NSC-SDK-003: SDK '" + manifest.name + "' has no target for architecture '" +
            std::string(architecture) + "' and version '" + std::string(version) + "'";
    }
    return selected;
}
}
