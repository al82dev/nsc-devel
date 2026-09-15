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
        std::filesystem::path(NSC_BINARY_DIR) / "item342_aggregate_abi_matrix";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    writeFile(root / "AggregateMatrix.nsproj",
        "<Project><PropertyGroup><Name>AggregateMatrix</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
public struct S1 { public byte a; }
public struct S2 { public short a; }
public struct S4 { public int a; }
public struct S8 { public long a; }
public struct S12 { public int a; public int b; public int c; }
public struct S16 { public long a; public long b; }
public struct S24 { public long a; public long b; public long c; }
public struct S64 {
    public long a; public long b; public long c; public long d;
    public long e; public long f; public long g; public long h;
}

public extern C delegate S1 Echo1(S1 value);
public extern C delegate S2 Echo2(S2 value);
public extern C delegate S4 Echo4(S4 value);
public extern C delegate S8 Echo8(S8 value);
public extern C delegate S12 Echo12(S12 value);
public extern C delegate S16 Echo16(S16 value);
public extern C delegate S24 Echo24(S24 value);
public extern C delegate S64 Echo64(S64 value);

public class Program
{
    public static S1 E1(S1 value) { return value; }
    public static S2 E2(S2 value) { return value; }
    public static S4 E4(S4 value) { return value; }
    public static S8 E8(S8 value) { return value; }
    public static S12 E12(S12 value) { return value; }
    public static S16 E16(S16 value) { return value; }
    public static S24 E24(S24 value) { return value; }
    public static S64 E64(S64 value) { return value; }

    public static int Main()
    {
        S1 v1; v1.a = 1; Echo1 f1 = E1; S1 r1 = f1(v1);
        S2 v2; v2.a = 2; Echo2 f2 = E2; S2 r2 = f2(v2);
        S4 v4; v4.a = 3; Echo4 f4 = E4; S4 r4 = f4(v4);
        S8 v8; v8.a = 4; Echo8 f8 = E8; S8 r8 = f8(v8);
        S12 v12; v12.a = 5; Echo12 f12 = E12; S12 r12 = f12(v12);
        S16 v16; v16.a = 6; Echo16 f16 = E16; S16 r16 = f16(v16);
        S24 v24; v24.a = 7; Echo24 f24 = E24; S24 r24 = f24(v24);
        S64 v64; v64.a = 14; Echo64 f64 = E64; S64 r64 = f64(v64);
        return r1.a + r2.a + r4.a + (int)r8.a + r12.a + (int)r16.a +
            (int)r24.a + (int)r64.a;
    }
}
)NS");

    std::string diagnostic;
    tests.expect(compileAndRun(
        root / "AggregateMatrix.nsproj", root / "AggregateMatrixO0.exe", "-O0", diagnostic),
        "aggregate ABI size matrix executes at O0 (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(compileAndRun(
        root / "AggregateMatrix.nsproj", root / "AggregateMatrixO2.exe", "-O2", diagnostic),
        "aggregate ABI size matrix executes at O2 (" + diagnostic + ")");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
