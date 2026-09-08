#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

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
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

#ifndef _WIN32
bool commandAvailable(std::string_view command)
{
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
}
#endif

int execute(const std::filesystem::path& executable)
{
#ifdef _WIN32
    return std::system(("\"" + executable.string() + "\"").c_str());
#else
    std::string wine;
    if (std::filesystem::exists("/usr/lib/wine/wine64")) wine = "/usr/lib/wine/wine64";
    else if (commandAvailable("wine64")) wine = "wine64";
    else if (commandAvailable("wine")) wine = "wine";
    if (wine.empty()) return -1;
    const int raw = std::system(
        ("WINEDEBUG=-all timeout 25 " + wine + " \"" + executable.string() +
         "\" >/dev/null 2>&1").c_str());
#ifdef WIFEXITED
    return WIFEXITED(raw) ? WEXITSTATUS(raw) : (raw & 0xFF);
#else
    return raw & 0xFF;
#endif
#endif
}

void compileAndRun(
    TestContext& tests,
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string_view optimization)
{
    Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, optimization, "-o", executableText};
    const ExitCode result = driver.run(arguments, output, error);
    tests.expect(
        result == ExitCode::Success,
        "E11 shared-cell fixture compiles at " + std::string(optimization) + ": " + error.str());
    if (result != ExitCode::Success) return;

    const int observed = execute(executable);
    if (observed < 0)
    {
        tests.expect(true, "E11 execution host unavailable; PE construction remains covered");
        return;
    }
    tests.expect(
        observed == 42,
        "E11 shared/nested captures execute to 42 at " + std::string(optimization) +
            " (observed " + std::to_string(observed) + ")");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() /
        "nativesharp_stage67_e11_lambda_execution";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const auto project = root / "E11Runtime.nsproj";
    writeFile(project, R"NSPROJ(<Project>
  <PropertyGroup><Name>E11Runtime</Name><OutputType>Exe</OutputType></PropertyGroup>
  <ItemGroup><Compile Include="**/*.ns" /></ItemGroup>
</Project>)NSPROJ");
    writeFile(root / "Main.ns", R"NS(
namespace NativeSharp.Core
{
    public class Exception { }
}
using NativeSharp.Core;

delegate int Transform(int value);
delegate Transform Factory(int seed);
delegate void Mutate(ref int value);
delegate int ReadValue();

struct LargeValue
{
    public int A;
    public int B;
    public int C;
    public int D;
}

class Boom : Exception
{
}

class Program
{
    public static int SharedMutation()
    {
        int state = 1;
        raii Transform read = (int value) => state + value;
        raii Transform write = (int value) => { state = value; return state; };
        state = 4;
        int before = read(1);
        write(10);
        return before + state + read(2);
    }

    public static Transform Make(int methodSeed)
    {
        raii Factory outer = (int outerSeed) =>
        {
            int localSeed = 3;
            return (int value) => value + outerSeed + localSeed + methodSeed;
        };
        return outer(2);
    }

    public static ReadValue CaptureLarge(LargeValue value)
    {
        return () => value.A + value.B + value.C + value.D;
    }

    public static ReadValue CaptureFixed()
    {
        int[4] values;
        values[0] = 1;
        values[1] = 2;
        values[2] = 3;
        values[3] = 4;
        ReadValue read = () => values[2];
        values[2] = 6;
        return read;
    }

    public static int LocalCatchKeepsCell()
    {
        int state = 1;
        raii Transform read = value => state + value;
        try
        {
            throw new Boom();
        }
        catch (Boom problem)
        {
            state = 9;
        }
        return read(1);
    }

    public static void ThrowAfterCapture()
    {
        int state = 1;
        raii Transform read = value => state + value;
        throw new Boom();
    }

    public static int Main()
    {
        Transform nested = Make(5);
        int nestedResult = nested(5);
        delete nested;
        raii Mutate mutate = (ref int value) => { value = value + 2; };
        int byReference = 40;
        mutate(ref byReference);

        LargeValue large;
        large.A = 1;
        large.B = 2;
        large.C = 3;
        large.D = 4;
        ReadValue largeRead = CaptureLarge(large);
        int largeResult = largeRead();
        delete largeRead;

        ReadValue fixedRead = CaptureFixed();
        int fixedResult = fixedRead();
        delete fixedRead;

        int exceptionResult = 0;
        try
        {
            ThrowAfterCapture();
        }
        catch (Boom problem)
        {
            exceptionResult = LocalCatchKeepsCell();
        }

        return SharedMutation() + nestedResult + byReference - 42 + largeResult + fixedResult - 16 +
            exceptionResult - 10;
    }
}
)NS");

    compileAndRun(tests, project, root / "e11-o0.exe", "-O0");
    compileAndRun(tests, project, root / "e11-o2.exe", "-O2");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
