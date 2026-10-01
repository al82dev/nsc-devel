#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace fs = std::filesystem;
using nativesharp::ExitCode;

void writeFile(const fs::path& path, std::string_view text)
{
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

ExitCode run(const std::vector<std::string>& owned, std::string& output, std::string& error)
{
    std::vector<std::string_view> arguments;
    for (const std::string& value : owned) arguments.push_back(value);
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode code = nativesharp::driver::Driver{}.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return code;
}
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;
    const fs::path root = fs::path(NSC_BINARY_DIR) / "ns3_t33_package_coherence";
    const fs::path package = root / "package";
    const fs::path consumer = root / "consumer";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(package, ignored);
    const std::string runtime =
        (fs::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    std::string output;
    std::string error;
    const std::string coreProject =
        (fs::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").string();
    const std::string coreLibrary = (package / "NativeSharpCore.lib").string();
    const ExitCode core = run({"--emit-project-oop-lib", coreProject, "-O0", "-j", "12",
        "-o", coreLibrary}, output, error);
    tests.expect(core == ExitCode::Success && fs::is_regular_file(package / "NativeSharpCore.nsmeta"),
        "real NativeSharp.Core produces versionable .lib + .nsmeta at O0 (" + error + output + ")");

    output.clear();
    error.clear();
    const std::string ioProject =
        (fs::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core.IO/NativeSharp.Core.IO.nsproj").string();
    const std::string ioLibrary = (package / "NativeSharpCoreIO.lib").string();
    const ExitCode io = run({"--emit-project-oop-lib", ioProject,
        "--library-reference-root", package.string(), "-O0", "-j", "12", "-o", ioLibrary},
        output, error);
    tests.expect(io == ExitCode::Success && fs::is_regular_file(package / "NativeSharpCoreIO.nsmeta"),
        "real NativeSharp.Core.IO consumes precompiled Core and produces .lib + .nsmeta (" +
            error + output + ")");

    writeFile(package / "NativeSharp.package.json", R"JSON({
  "packageId": "NativeSharp.Core.IO",
  "version": "3.0.0",
  "libraries": [
    { "library": "NativeSharpCore.lib", "metadata": "NativeSharpCore.nsmeta" },
    { "library": "NativeSharpCoreIO.lib", "metadata": "NativeSharpCoreIO.nsmeta" }
  ]
})JSON");
    writeFile(consumer / "CoreIOConsumer.nsproj", R"XML(<Project>
  <PropertyGroup><Name>CoreIOConsumer</Name></PropertyGroup>
  <ItemGroup><Compile Include="**/*.ns" /></ItemGroup>
</Project>
)XML");
    writeFile(consumer / "Main.ns", R"NS(
using NativeSharp.Core.IO;
public class Program
{
    public static int Main()
    {
        return Path.DirectorySeparatorChar() == '\\' ? 0 : 1;
    }
}
)NS");
    const std::string consumerProject = (consumer / "CoreIOConsumer.nsproj").string();
    output.clear();
    error.clear();
    tests.expect(run({"add", "package", "NativeSharp.Core.IO", "--version", "3.0.0",
            "--source", package.string(), consumerProject}, output, error) == ExitCode::Success,
        "consumer records the versioned Core.IO package intent (" + error + output + ")");
    output.clear();
    error.clear();
    tests.expect(run({"restore", consumerProject}, output, error) ==
            ExitCode::Success,
        "Core.IO closure restores transactionally under a deterministic lock (" + error + output + ")");

    fs::remove_all(package, ignored);
    output.clear();
    error.clear();
    const ExitCode built = run({"build", consumerProject, "--precompiled-only", "--no-restore",
        "--dependency-report", "--output-format=json", "--linker", "lld-link", "-O0"},
        output, error);
    tests.expect(built == ExitCode::Success &&
            output.find("\"identity\":\"NativeSharp.Core.IO@3.0.0\"") != std::string::npos &&
            output.find("\"kind\":\"library-reference\"") != std::string::npos &&
            output.find("NativeSharpCoreIO") != std::string::npos,
        "precompiled-only/no-restore consumes the complete Core.IO store closure with provenance (" +
            error + output + ")");
    const fs::path executable = consumer / "bin/Debug/CoreIOConsumer.exe";
    if (built == ExitCode::Success)
    {
        const std::string command = "cmd.exe /d /s /c \"\"" + executable.string() + "\"\"";
        tests.expect(std::system(command.c_str()) == 0,
            "the exclusively precompiled Core.IO consumer executes successfully");
    }

    fs::remove_all(root, ignored);
    return tests.result();
}
