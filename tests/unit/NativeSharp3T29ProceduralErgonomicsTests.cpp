#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
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

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode code = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return code;
}

bool compileAndRun(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "-O0", "--emit-project-oop-exe", projectText, "-o", executableText};
    std::string output;
    if (runDriver(arguments, output, diagnostic) != nativesharp::ExitCode::Success)
    {
        return false;
    }
    const std::string command = "\"" + executableText + "\"";
    const int result = std::system(command.c_str());
    if (result != 0)
    {
        diagnostic = "process result " + std::to_string(result);
        return false;
    }
    return true;
}

void writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view references = {})
{
    writeFile(root / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\"/>" + std::string(references) +
        "</ItemGroup></Project>");
    writeFile(root / "Main.ns", source);
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t29_procedural_ergonomics";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);

    const std::filesystem::path procedural = root / "procedural";
    writeProject(procedural, "T29Procedural", R"NS(
namespace T29
{
    public class Box
    {
        public int Value;
        public Box(int value) { Value = value; }
    }

    int ConstructValue()
    {
        raii Box box = new Box(42);
        return box.Value;
    }

    long ExtendedGcd(long a, long b, out long x, out long y)
    {
        if (b == 0L)
        {
            x = 1L;
            y = 0L;
            return a;
        }
        long previousX = 0L;
        long previousY = 0L;
        long divisor = ExtendedGcd(b, a % b, out previousX, out previousY);
        x = previousY;
        y = previousX - (a / b) * previousY;
        return divisor;
    }

    int Classify(int value)
    {
        if (value < 0) return -1;
        else if (value == 0) return 0;
        else return 1;
    }

    int DanglingElse()
    {
        int result = 0;
        if (true)
            if (false) result = 1;
            else result = 2;
        return result;
    }

    int EmptyBody()
    {
        int result = 1;
        if (false); else result = 2;
        return result;
    }

    int OrderedFor()
    {
        int trace = 0;
        int index = 0;
        for (trace = 1, index = 1; index < 3; trace = trace * 10 + index, index = index + 1)
        {
        }
        return trace;
    }

    int Main()
    {
        if (ConstructValue() != 42) return 1;
        if (Classify(-3) != -1 || Classify(0) != 0 || Classify(3) != 1) return 2;
        if (DanglingElse() != 2) return 3;
        if (EmptyBody() != 2) return 4;
        if (OrderedFor() != 112) return 5;

        long x = 0L;
        long y = 0L;
        long divisor = ExtendedGcd(240L, 46L, out x, out y);
        if (divisor != 2L || x != -9L || y != 47L) return 6;

        long swappedX = 0L;
        long swappedY = 0L;
        divisor = ExtendedGcd(240L, 46L, out swappedY, out swappedX);
        if (divisor != 2L || swappedX != 47L || swappedY != -9L) return 7;

        long aliased = 0L;
        divisor = ExtendedGcd(240L, 46L, out aliased, out aliased);
        if (divisor != 2L || aliased != 47L) return 8;
        return 0;
    }
}
)NS");

    std::string diagnostic;
    const bool proceduralOk = compileAndRun(
        procedural / "T29Procedural.nsproj", procedural / "T29Procedural.exe", diagnostic);
    tests.expect(proceduralOk,
        "free construction, recursive out, ordered for lists, and braceless if execute at O0 (" +
            diagnostic + ")");

    const std::filesystem::path interpolation = root / "interpolation";
    const std::filesystem::path coreSource =
        std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core";
    const std::filesystem::path core3 = root / "NativeSharp.Core3";
    std::filesystem::create_directories(core3, filesystemError);
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(coreSource))
    {
        const std::string filename = entry.path().filename().string();
        if (entry.is_regular_file() &&
            (filename == "String.ns" || filename == "NumericLimits.ns"))
        {
            std::filesystem::copy_file(
                entry.path(),
                core3 / entry.path().filename(),
                std::filesystem::copy_options::overwrite_existing,
                filesystemError);
        }
    }
    writeFile(core3 / "NativeSharp.Core3.nsproj",
        "<Project><PropertyGroup><Name>NativeSharpCore3</Name><Language>3</Language>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    const std::string coreProject = (core3 / "NativeSharp.Core3.nsproj").generic_string();
    writeProject(interpolation, "T29Interpolation", R"NS(
using NativeSharp.Core;

public class Program
{
    private static int Mark(ref int trace, int digit)
    {
        trace = trace * 10 + digit;
        return digit;
    }

    public static int Main()
    {
        int trace = 0;
        int value = 31;
        int negative = -42;
        raii string ordered = $"{Mark(ref trace, 1)}{Mark(ref trace, 2)}";
        if (trace != 12) return 1;
        if (ordered != "12") return 2;
        raii string hexadecimal = $"{value:X4}";
        if (hexadecimal != "001F") return 3;
        raii string lowerHexadecimal = $"{value:x4}";
        if (lowerHexadecimal != "001f") return 4;
        raii string negativeHexadecimal = $"{-1:X}";
        if (negativeHexadecimal != "FFFFFFFF") return 5;
        raii string decimal = $"{negative:D5}";
        if (decimal != "-0042") return 6;
        raii string fixedPoint = $"{2.5:F2}";
        if (fixedPoint != "2.50") return 7;
        raii string fixedDefault = $"{2.5:F}";
        if (fixedDefault != "2.50") return 8;
        raii string general = $"{2.5:G}";
        if (general != "2.5") return 9;
        raii string text = $"A{value}B{value:X4}C{2.5:F2}";
        if (text != "A31B001FC2.50") return 10;
        return 0;
    }
}
)NS", "<ProjectReference Include=\"" + coreProject + "\"/>");

    diagnostic.clear();
    const bool interpolationOk = compileAndRun(
        interpolation / "T29Interpolation.nsproj",
        interpolation / "T29Interpolation.exe",
        diagnostic);
    tests.expect(interpolationOk,
        "runtime interpolation evaluates holes once in source order and formats at O0 (" +
            diagnostic + ")");

    const std::filesystem::path noHeapStatic = root / "noheap-static";
    writeProject(noHeapStatic, "T29NoHeapStatic", R"NS(
public class Program
{
    public static noheap int Length()
    {
        string text = $"value={42:D4}";
        return text.Length;
    }
}
)NS");
    const std::string noHeapStaticProject =
        (noHeapStatic / "T29NoHeapStatic.nsproj").string();
    const std::array<std::string_view, 3> noHeapStaticArguments = {
        "--dump-project-oop-hir", noHeapStaticProject, "-O0"};
    std::string output;
    diagnostic.clear();
    tests.expect(runDriver(noHeapStaticArguments, output, diagnostic) == ExitCode::Success,
        "fully static interpolation remains allocation-free in a noheap callable (" +
            diagnostic + ")");

    const std::filesystem::path noHeapRuntime = root / "noheap-runtime";
    writeProject(noHeapRuntime, "T29NoHeapRuntime", R"NS(
using NativeSharp.Core;
public class Program
{
    public static noheap int Length(int value)
    {
        raii string text = $"value={value}";
        return text.Length;
    }
}
)NS", "<ProjectReference Include=\"" + coreProject + "\"/>");
    const std::string noHeapRuntimeProject =
        (noHeapRuntime / "T29NoHeapRuntime.nsproj").string();
    const std::array<std::string_view, 3> noHeapRuntimeArguments = {
        "--dump-project-oop-hir", noHeapRuntimeProject, "-O0"};
    output.clear();
    diagnostic.clear();
    tests.expect(runDriver(noHeapRuntimeArguments, output, diagnostic) != ExitCode::Success &&
            diagnostic.find("NS3030") != std::string::npos,
        "runtime interpolation cannot hide allocation inside a noheap callable (" +
            diagnostic + ")");

    const std::filesystem::path invalidFormat = root / "invalid-format";
    writeProject(invalidFormat, "T29InvalidFormat", R"NS(
public class Program
{
    public static int Format(int value)
    {
        string text = $"value={value:Q9}";
        return text.Length;
    }
}
)NS");
    const std::string invalidFormatProject =
        (invalidFormat / "T29InvalidFormat.nsproj").string();
    const std::array<std::string_view, 3> invalidFormatArguments = {
        "--dump-project-method-bodies", invalidFormatProject, "-O0"};
    output.clear();
    diagnostic.clear();
    tests.expect(runDriver(invalidFormatArguments, output, diagnostic) != ExitCode::Success &&
            diagnostic.find("runtime interpolation format must be D or X") !=
                std::string::npos,
        "invalid runtime interpolation formats fail with a precise diagnostic (" +
            diagnostic + ")");

    const std::filesystem::path declaration = root / "declaration";
    writeProject(declaration, "T29Declaration", R"NS(
public class Program
{
    public static int Main()
    {
        if (true) int value = 1;
        return 0;
    }
}
)NS");
    const std::string declarationProject = (declaration / "T29Declaration.nsproj").string();
    const std::array<std::string_view, 3> declarationArguments = {
        "--dump-project-method-bodies", declarationProject, "-O0"};
    output.clear();
    diagnostic.clear();
    tests.expect(runDriver(declarationArguments, output, diagnostic) != ExitCode::Success &&
            diagnostic.find("requires an explicit '{ ... }' block") != std::string::npos,
        "a local declaration after if requires braces with a precise diagnostic (" +
            diagnostic + ")");

    return tests.resultKeeping(root);
}
