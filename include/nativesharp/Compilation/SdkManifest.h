#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
inline constexpr std::uint32_t kSdkManifestFormatVersion = 1;

struct SdkHeaderMapping final
{
    std::string include;
    std::string targetNamespace;
};

struct SdkTypeMapping final
{
    std::string nativeName;
    std::string nativeSharpName;
};

struct SdkFieldMapping final
{
    std::string containingType;
    std::string nativeName;
    std::string nativeSharpName;
};

struct SdkDefineMapping final
{
    std::string name;
    std::string value;
};

struct SdkManifest final
{
    std::filesystem::path manifestPath;
    std::string name;
    std::uint32_t formatVersion = kSdkManifestFormatVersion;
    std::vector<SdkHeaderMapping> headers;
    std::vector<SdkTypeMapping> types;
    std::vector<SdkFieldMapping> fields;
    std::vector<SdkDefineMapping> defines;
};

class SdkManifestLoader final
{
public:
    std::optional<SdkManifest> load(
        const std::filesystem::path& manifestPath,
        std::string& error) const;
};
}
