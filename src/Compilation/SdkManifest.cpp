#include "nativesharp/Compilation/SdkManifest.h"

#include "nativesharp/Compilation/SimpleXmlParser.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <string_view>
#include <system_error>
#include <unordered_set>

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
}

std::optional<SdkManifest> SdkManifestLoader::load(
    const std::filesystem::path& manifestPath,
    std::string& error) const
{
    error.clear();
    std::error_code filesystemError;
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(manifestPath, filesystemError);
    if (filesystemError || !std::filesystem::is_regular_file(canonical))
    {
        error = "SDK manifest does not exist or is not a regular file: '" +
            manifestPath.string() + "'";
        return std::nullopt;
    }
    if (canonical.extension() != ".nssdk")
    {
        error = "NativeSharp SDK manifest must use the canonical .nssdk extension";
        return std::nullopt;
    }

    constexpr std::uintmax_t kMaximumManifestBytes = 4U * 1024U * 1024U;
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

    SimpleXmlElement root;
    SimpleXmlParser parser;
    if (!parser.parse(source, root, error))
    {
        error = canonical.string() + ": " + error;
        return std::nullopt;
    }
    if (root.name != "NativeSharpSdk")
    {
        error = canonical.string() + ": SDK root element must be 'NativeSharpSdk'";
        return std::nullopt;
    }
    if (!trim(root.text).empty())
    {
        error = canonical.string() + ": NativeSharpSdk does not accept text";
        return std::nullopt;
    }
    for (const auto& [name, value] : root.attributes)
    {
        static_cast<void>(value);
        if (name != "Name" && name != "FormatVersion")
        {
            error = canonical.string() + ": NativeSharpSdk has unsupported attribute '" + name + "'";
            return std::nullopt;
        }
    }

    const std::string* name = attribute(root, "Name");
    const std::string* version = attribute(root, "FormatVersion");
    if (name == nullptr || !isIdentifier(trim(*name)))
    {
        error = canonical.string() + ": SDK Name must be a NativeSharp identifier";
        return std::nullopt;
    }
    if (version == nullptr)
    {
        error = canonical.string() + ": SDK FormatVersion is required";
        return std::nullopt;
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
        return std::nullopt;
    }
    SdkManifest manifest;
    manifest.manifestPath = canonical;
    manifest.name = trim(*name);
    manifest.formatVersion = parsedVersion;
    bool sawMappings = false;
    for (const SimpleXmlElement& child : root.children)
    {
        if (child.name != "Mappings")
        {
            error = canonical.string() + ": unsupported NativeSharpSdk child '" + child.name + "'";
            return std::nullopt;
        }
        if (sawMappings)
        {
            error = canonical.string() + ": SDK Mappings is declared more than once";
            return std::nullopt;
        }
        sawMappings = true;
        if (!parseMappings(child, manifest, error))
        {
            error = canonical.string() + ": " + error;
            return std::nullopt;
        }
    }
    return manifest;
}
}
