#include "nativesharp/Compilation/SdkManifest.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
using nativesharp::compilation::SdkManifestLoader;
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
        std::filesystem::temp_directory_path() / "nativesharp_item335_sdk_mapping";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);

    const std::filesystem::path manifestPath = root / "Win32.nssdk";
    writeFile(manifestPath, R"SDK(<NativeSharpSdk Name="Win32" FormatVersion="1">
  <Mappings>
    <Header Include="windows.h" Namespace="Windows"/>
    <Type Native="DWORD" NativeSharp="uint"/>
    <Field Type="POINT" Native="x" NativeSharp="X"/>
    <Define Name="MAX_PATH" Value="260"/>
  </Mappings>
</NativeSharpSdk>)SDK");
    SdkManifestLoader loader;
    std::string error;
    const auto manifest = loader.load(manifestPath, error);
    tests.expect(manifest.has_value(), "header, type, field, and define mappings load");
    if (manifest.has_value())
    {
        tests.expect(manifest->headers.size() == 1 &&
                manifest->headers[0].include == "windows.h" &&
                manifest->headers[0].targetNamespace == "Windows",
            "header mapping preserves include and namespace");
        tests.expect(manifest->types.size() == 1 &&
                manifest->types[0].nativeName == "DWORD" &&
                manifest->types[0].nativeSharpName == "uint",
            "type mapping preserves both names");
        tests.expect(manifest->fields.size() == 1 &&
                manifest->fields[0].containingType == "POINT" &&
                manifest->fields[0].nativeSharpName == "X",
            "field mapping is scoped by its containing native type");
        tests.expect(manifest->defines.size() == 1 &&
                manifest->defines[0].name == "MAX_PATH" &&
                manifest->defines[0].value == "260",
            "define mapping preserves its declarative value");
    }

    writeFile(root / "Duplicate.nssdk", R"SDK(<NativeSharpSdk Name="Win32" FormatVersion="1">
  <Mappings><Type Native="DWORD" NativeSharp="uint"/>
  <Type Native="DWORD" NativeSharp="int"/></Mappings>
</NativeSharpSdk>)SDK");
    tests.expect(!loader.load(root / "Duplicate.nssdk", error).has_value() &&
            error.find("duplicate SDK mapping key") != std::string::npos,
        "duplicate native mapping keys are rejected");

    writeFile(root / "Unknown.nssdk", R"SDK(<NativeSharpSdk Name="Win32" FormatVersion="1">
  <Mappings><Function Native="CreateFileW"/></Mappings>
</NativeSharpSdk>)SDK");
    tests.expect(!loader.load(root / "Unknown.nssdk", error).has_value() &&
            error.find("unsupported SDK mapping") != std::string::npos,
        "unknown mapping kinds fail closed");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
