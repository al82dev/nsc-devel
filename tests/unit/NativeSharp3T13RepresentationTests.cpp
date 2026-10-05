#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source)
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
}

std::string diagnosticsFor(const nativesharp::frontend::ProjectCompilation& compilation)
{
    std::string text;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        text += diagnostic.message() + " | ";
    return text;
}

bool rejectsWith(
    const std::filesystem::path& project,
    std::string_view expected,
    std::string& diagnostic)
{
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(project, error);
    diagnostic = error + diagnosticsFor(compilation);
    return !built && diagnostic.find(expected) != std::string::npos;
}

bool compileAndRun(const std::filesystem::path& project, int expected, std::string& diagnostic)
{
    const std::filesystem::path executable = project.parent_path() / "Representation.exe";
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    nativesharp::driver::Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    const nativesharp::ExitCode result = driver.run(arguments, output, error);
    diagnostic = error.str() + output.str();
    if (result != nativesharp::ExitCode::Success) return false;
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed);
    return false;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t13-f01-directed";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    const std::filesystem::path positive = writeProject(root / "positive", "Representation", R"NS(
namespace Representation
{
    repr(C) pack(1) public struct Packet
    {
        public byte Tag;
        public int Value;
    }
    repr(transparent) public struct Handle
    {
        public int Value;
    }
    repr(native) public struct NativePair
    {
        public int Left;
        public int Right;
    }
    public class Program
    {
        public static int Main()
        {
            Packet packet = { 1, 41 };
            Handle handle = { packet.Value + packet.Tag };
            NativePair pair = { handle.Value, 0 };
            return pair.Left;
        }
    }
}
)NS");
    std::string diagnostic;
    tests.expect(compileAndRun(positive, 42, diagnostic),
        "repr(C), repr(transparent), and repr(native) execute at O0 through NSLink (" +
            diagnostic + ")");

    const std::filesystem::path twoFields = writeProject(root / "two-fields", "TwoFields", R"NS(
repr(transparent) public struct Broken { public int Left; public int Right; }
public class Program { public static int Main() { return 0; } }
)NS");
    tests.expect(rejectsWith(twoFields,
            "repr(transparent) requires exactly one non-zero-sized instance field", diagnostic),
        "repr(transparent) rejects two represented fields (" + diagnostic + ")");

    const std::filesystem::path packedTransparent = writeProject(
        root / "packed-transparent", "PackedTransparent", R"NS(
pack(1) repr(transparent) public struct Broken { public int Value; }
public class Program { public static int Main() { return 0; } }
)NS");
    tests.expect(rejectsWith(packedTransparent,
            "repr(transparent) cannot be a union or override pack, align, or size", diagnostic),
        "transparent representation cannot rewrite its carrier layout (" + diagnostic + ")");

    const std::filesystem::path duplicate = writeProject(root / "duplicate", "Duplicate", R"NS(
repr(C) repr(native) public struct Broken { public int Value; }
public class Program { public static int Main() { return 0; } }
)NS");
    tests.expect(rejectsWith(duplicate,
            "an aggregate may declare exactly one representation attribute", diagnostic),
        "duplicate representation attributes are rejected deterministically (" + diagnostic + ")");

    const std::filesystem::path invalidTarget = writeProject(root / "class", "ClassRepr", R"NS(
repr(C) public class Broken {}
public class Program { public static int Main() { return 0; } }
)NS");
    tests.expect(rejectsWith(invalidTarget, "repr applies only to struct and union", diagnostic),
        "representation attributes cannot decorate reference classes (" + diagnostic + ")");

    return tests.result();
}
