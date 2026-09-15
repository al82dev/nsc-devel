#include "nativesharp/Compilation/ProjectLoader.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
using nativesharp::compilation::ProjectLoader;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_item337_sdk_diagnostics";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    writeFile(root / "Main.ns", "int main() { return 0; }\n");

    ProjectLoader loader;
    std::string error;
    writeFile(root / "MissingSdk.nsproj",
        "<Project><ItemGroup><SdkReference Include=\"Missing.nssdk\"/></ItemGroup></Project>");
    tests.expect(!loader.load(root / "MissingSdk.nsproj", error).has_value() &&
            error.find("NSC-SDK-001") != std::string::npos &&
            error.find("Missing.nssdk") != std::string::npos,
        "missing SDK reference reports a stable code and physical path");

    writeFile(root / "Windows.nssdk", R"SDK(<NativeSharpSdk Name="Windows" FormatVersion="1">
  <Libraries><Library Name="user32" Path="Lib/x64/user32.lib"/></Libraries>
</NativeSharpSdk>)SDK");
    writeFile(root / "MissingLibrary.nsproj",
        "<Project><ItemGroup><SdkReference Include=\"Windows.nssdk\"/></ItemGroup></Project>");
    tests.expect(!loader.load(root / "MissingLibrary.nsproj", error).has_value() &&
            error.find("NSC-SDK-004") != std::string::npos &&
            error.find("user32") != std::string::npos &&
            error.find("user32.lib") != std::string::npos,
        "missing SDK library reports code, logical name, and resolved path");

    writeFile(root / "Lib" / "x64" / "user32.lib", "archive");
    const auto project = loader.load(root / "MissingLibrary.nsproj", error);
    tests.expect(project.has_value(), "project loads when every declared SDK library exists");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
