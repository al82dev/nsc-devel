#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
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

struct SdkTargetVariant final
{
    std::string architecture;
    std::string minimumVersion;
    std::string maximumVersion;
};

struct SdkLibraryDependency final
{
    std::string name;
    std::filesystem::path relativePath;
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
    std::vector<SdkTargetVariant> targets;
    std::vector<SdkLibraryDependency> libraries;
};

class SdkTargetSelector final
{
public:
    const SdkTargetVariant* select(
        const SdkManifest& manifest,
        std::string_view architecture,
        std::string_view version,
        std::string& error) const;
};

class SdkManifestLoader final
{
public:
    std::optional<SdkManifest> load(
        const std::filesystem::path& manifestPath,
        std::string& error) const;
};
}
