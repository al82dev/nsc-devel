#include "nativesharp/Compilation/SdkManifest.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
using nativesharp::compilation::SdkManifestLoader;
using nativesharp::compilation::SdkTargetSelector;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_item336_sdk_targets";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::filesystem::path path = root / "Windows.nssdk";
    writeFile(path, R"SDK(<NativeSharpSdk Name="Windows" FormatVersion="1">
  <Targets>
    <Target Architecture="arm64" MinVersion="10.0" MaxVersion="10.0.99999"/>
    <Target Architecture="x64" MinVersion="10.0" MaxVersion="10.0.21999"/>
    <Target Architecture="x64" MinVersion="10.0.22000" MaxVersion="11.0"/>
  </Targets>
</NativeSharpSdk>)SDK");
    SdkManifestLoader loader;
    std::string error;
    const auto manifest = loader.load(path, error);
    tests.expect(manifest.has_value() && manifest->targets.size() == 3,
        "versioned architecture variants load and are retained");
    if (manifest.has_value())
    {
        SdkTargetSelector selector;
        const auto* oldX64 = selector.select(*manifest, "x64", "10.0.19041", error);
        tests.expect(oldX64 != nullptr && oldX64->maximumVersion == "10.0.21999",
            "x64 selects the matching older SDK range");
        const auto* newX64 = selector.select(*manifest, "x64", "10.0.22621", error);
        tests.expect(newX64 != nullptr && newX64->minimumVersion == "10.0.22000",
            "x64 selects the matching newer SDK range");
        tests.expect(selector.select(*manifest, "arm64", "10.0.19041.0", error) != nullptr,
            "version comparison normalizes trailing zero components");
        tests.expect(selector.select(*manifest, "x86", "10.0", error) == nullptr &&
                error.find("has no target") != std::string::npos,
            "missing architecture fails with a deterministic diagnostic");
    }

    writeFile(root / "Overlap.nssdk", R"SDK(<NativeSharpSdk Name="Windows" FormatVersion="1">
  <Targets><Target Architecture="x64" MinVersion="10.0" MaxVersion="10.5"/>
  <Target Architecture="x64" MinVersion="10.5" MaxVersion="11.0"/></Targets>
</NativeSharpSdk>)SDK");
    tests.expect(!loader.load(root / "Overlap.nssdk", error).has_value() &&
            error.find("ranges overlap") != std::string::npos,
        "ambiguous target ranges are rejected at manifest load time");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
