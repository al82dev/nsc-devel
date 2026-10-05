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
    const auto project = directory / (std::string(name) + ".nsproj");
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

bool rejectsWith(const std::filesystem::path& project, std::string_view expected, std::string& text)
{
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(project, error);
    text = error + diagnosticsFor(compilation);
    return !built && text.find(expected) != std::string::npos;
}

bool compileAndRun(const std::filesystem::path& project, int expected, std::string& text)
{
    const auto executable = project.parent_path() / "FixedIntegers.exe";
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    nativesharp::driver::Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    const auto result = driver.run(arguments, output, error);
    text = error.str() + output.str();
    if (result != nativesharp::ExitCode::Success)
    {
        text += "driver exit code " + std::to_string(static_cast<int>(result));
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    text += "process exit code " + std::to_string(observed);
    return false;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "ns3-t13-f02-directed";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    const auto positive = writeProject(root / "positive", "FixedIntegers", R"NS(
namespace FixedIntegers
{
    repr(C) public struct Flags
    {
        public volatile u32 Ready : 1;
        public i32 Code : 7;
        public i128 Wide;
        public u128 UnsignedWide;
    }
    public class Program
    {
        public static int Main()
        {
            Flags flags = { 1, -2, 40, 2 };
            flags.Code += 3;
            i128 total = flags.Wide + 2;
            if (sizeof(i8) != 1 || sizeof(u16) != 2 || sizeof(i32) != 4 ||
                sizeof(u64) != 8 || sizeof(isize) != 8 || sizeof(usize) != 8 ||
                sizeof(i128) != 16 || sizeof(u128) != 16) return 1;
            return (int)total;
        }
    }
}
)NS");
    std::string diagnostic;
    tests.expect(compileAndRun(positive, 42, diagnostic),
        "fixed-width aliases, i128/u128, signed and volatile bit-fields execute O0 through NSLink (" +
            diagnostic + ")");

    const auto implicitLayout = writeProject(root / "implicit-layout", "ImplicitLayout", R"NS(
public struct Flags { public u32 Ready : 1; }
public class Program { public static int Main() { return 0; } }
)NS");
    tests.expect(rejectsWith(implicitLayout,
            "NativeSharp 3 bit-fields require an explicit repr(native) or repr(C) layout", diagnostic),
        "edition-3 bit-fields reject implicit layout (" + diagnostic + ")");

    const auto overflow = writeProject(root / "overflow", "BitOverflow", R"NS(
repr(C) public struct Flags { public u8 Broken : 9; }
public class Program { public static int Main() { return 0; } }
)NS");
    tests.expect(rejectsWith(overflow, "bit-field width exceeds its storage type", diagnostic),
        "bit-field width is bounded by its fixed storage type (" + diagnostic + ")");

    const auto transparent = writeProject(root / "transparent", "TransparentBits", R"NS(
repr(transparent) public struct Flags { public u32 Value : 3; }
public class Program { public static int Main() { return 0; } }
)NS");
    tests.expect(rejectsWith(transparent, "repr(transparent)", diagnostic),
        "transparent wrappers cannot hide bit allocation units (" + diagnostic + ")");

    return tests.result();
}
