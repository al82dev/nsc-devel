#pragma once

#include "nativesharp/Compilation/SdkDefineMapping.h"
#include "nativesharp/Compilation/SdkFieldMapping.h"
#include "nativesharp/Compilation/SdkHeaderMapping.h"
#include "nativesharp/Compilation/SdkLibraryDependency.h"
#include "nativesharp/Compilation/SdkTargetVariant.h"
#include "nativesharp/Compilation/SdkTypeMapping.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
inline constexpr std::uint32_t kSdkManifestFormatVersion = 1;

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
}

#include "nativesharp/Compilation/SdkManifestLoader.h"
#include "nativesharp/Compilation/SdkTargetSelector.h"
