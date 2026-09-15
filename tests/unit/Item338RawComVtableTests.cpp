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
        std::filesystem::path(NSC_BINARY_DIR) / "item338_raw_com_vtable";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    writeFile(root / "RawCom.nsproj",
        "<Project><PropertyGroup><Name>RawCom</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
public extern C delegate int ComMethod(void* self, int value);

public struct RawComInterface
{
    public nint* VTable;
    public int Bias;
}

public class Program
{
    public static int Add(void* self, int value)
    {
        RawComInterface* instance = (RawComInterface*)self;
        return value + instance->Bias;
    }

    public static int Main()
    {
        nint* table = stackalloc nint[3];
        table[0] = 0;
        table[1] = 0;
        ComMethod implementation = Add;
        table[2] = (nint)implementation;

        RawComInterface instance;
        instance.VTable = table;
        instance.Bias = 2;
        ComMethod method = (ComMethod)instance.VTable[2];
        return method(&instance, 40);
    }
}
)NS");

    std::string diagnostic;
    tests.expect(compileAndRun(root / "RawCom.nsproj", root / "RawComO0.exe", "-O0", diagnostic),
        "raw COM vtable dispatch executes at O0 (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(compileAndRun(root / "RawCom.nsproj", root / "RawComO2.exe", "-O2", diagnostic),
        "raw COM vtable dispatch executes at O2 (" + diagnostic + ")");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
