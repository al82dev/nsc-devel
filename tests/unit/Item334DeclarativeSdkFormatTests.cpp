#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Compilation/SdkManifest.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
using nativesharp::compilation::ProjectLoader;
using nativesharp::compilation::SdkManifestLoader;
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
        std::filesystem::temp_directory_path() / "nativesharp_item334_declarative_sdk";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);

    const std::filesystem::path manifest = root / "Windows.nssdk";
    writeFile(manifest,
        "<?xml version=\"1.0\"?>\n"
        "<NativeSharpSdk Name=\"Windows\" FormatVersion=\"1\"/>\n");
    SdkManifestLoader loader;
    std::string error;
    const auto sdk = loader.load(manifest, error);
    tests.expect(sdk.has_value(), "canonical declarative SDK manifest loads");
    if (sdk.has_value())
    {
        tests.expect(sdk->name == "Windows", "SDK identity is preserved");
        tests.expect(sdk->formatVersion == 1, "SDK format version is explicit");
        tests.expect(sdk->manifestPath == std::filesystem::weakly_canonical(manifest),
            "SDK manifest path is canonicalized");
    }

    writeFile(root / "Future.nssdk",
        "<NativeSharpSdk Name=\"Future\" FormatVersion=\"2\"/>\n");
    tests.expect(!loader.load(root / "Future.nssdk", error).has_value() &&
            error.find("unsupported SDK FormatVersion") != std::string::npos,
        "future SDK formats fail closed with a deterministic diagnostic");
    writeFile(root / "Body.nssdk",
        "<NativeSharpSdk Name=\"Windows\" FormatVersion=\"1\"><Mapping/></NativeSharpSdk>\n");
    tests.expect(!loader.load(root / "Body.nssdk", error).has_value() &&
            error.find("body is reserved") != std::string::npos,
        "format 1 rejects undeclared extension elements");

    writeFile(root / "Main.ns", "int main() { return 0; }\n");
    writeFile(root / "App.nsproj",
        "<Project><ItemGroup><SdkReference Include=\"Windows.nssdk\"/>"
        "</ItemGroup></Project>\n");
    ProjectLoader projectLoader;
    const auto project = projectLoader.load(root / "App.nsproj", error);
    tests.expect(project.has_value(), "project accepts a declarative SDK reference");
    tests.expect(project.has_value() && project->sdkReferences().size() == 1 &&
            project->sdkReferences()[0].name == "Windows",
        "validated SDK identity reaches the project compilation model");

    writeFile(root / "Duplicate.nsproj",
        "<Project><ItemGroup><SdkReference Include=\"Windows.nssdk\"/>"
        "<SdkReference Include=\"Windows.nssdk\"/></ItemGroup></Project>\n");
    tests.expect(!projectLoader.load(root / "Duplicate.nsproj", error).has_value() &&
            error.find("duplicate SdkReference") != std::string::npos,
        "duplicate canonical SDK references are rejected");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
