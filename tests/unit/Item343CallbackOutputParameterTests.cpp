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

bool compileAndRun(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string_view optimization,
    std::string& diagnostic)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, optimization, "-o", executableText};
    if (driver.run(arguments, out, err) != ExitCode::Success)
    {
        diagnostic = err.str() + out.str();
        return false;
    }
    const int result = std::system(("\"" + executableText + "\"").c_str());
    if (result != 42)
    {
        diagnostic = "process result " + std::to_string(result) + ", expected 42";
        return false;
    }
    return true;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "item343_callback_output_parameters";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    writeFile(root / "CallbackOutputs.nsproj",
        "<Project><PropertyGroup><Name>CallbackOutputs</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
public struct Pair { public int Left; public int Right; }
public extern C delegate int NativeCallback(ref int counter, out Pair output, in Pair input);

public class Program
{
    public static int Transform(ref int counter, out Pair output, in Pair input)
    {
        counter = counter + 1;
        output.Left = input.Left + counter;
        output.Right = input.Right + counter;
        return 4;
    }

    public static int Main()
    {
        NativeCallback callback = Transform;
        int counter = 1;
        Pair input;
        input.Left = 16;
        input.Right = 18;
        Pair output;
        int status = callback(ref counter, out output, in input);
        if (counter != 2) { return 1; }
        if (input.Left != 16 || input.Right != 18) { return 2; }
        return output.Left + output.Right + status;
    }
}
)NS");

    std::string diagnostic;
    tests.expect(compileAndRun(
        root / "CallbackOutputs.nsproj", root / "CallbackOutputsO0.exe", "-O0", diagnostic),
        "callback ref/out/in ABI executes at O0 (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(compileAndRun(
        root / "CallbackOutputs.nsproj", root / "CallbackOutputsO2.exe", "-O2", diagnostic),
        "callback ref/out/in ABI executes at O2 (" + diagnostic + ")");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
