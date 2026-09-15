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

ExitCode run(std::span<const std::string_view> arguments, std::string& outText, std::string& errText)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    outText = out.str();
    errText = err.str();
    return result;
}

bool compileAndRun(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string_view optimization,
    std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, optimization, "-o", executableText};
    std::string out;
    std::string err;
    if (run(arguments, out, err) != ExitCode::Success)
    {
        diagnostic = err + out;
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
        std::filesystem::path(NSC_BINARY_DIR) / "item345_vectorcall_hva";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);

    const std::string coreProject =
        (std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj")
            .generic_string();
    writeFile(root / "VectorCallHva.nsproj",
        "<Project><PropertyGroup><Name>VectorCallHva</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<ProjectReference Include=\"" + coreProject + "\"/>"
        "</ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Numerics;

public struct VectorPair
{
    public Float32x4 First;
    public Float32x4 Second;
}

public extern vectorcall delegate Float32x4 EchoVector(Float32x4 value);
public extern vectorcall delegate VectorPair EchoPair(VectorPair value);

public class Program
{
    public static Float32x4 ReturnVector(Float32x4 value) { return value; }
    public static VectorPair ReturnPair(VectorPair value) { return value; }

    public static int Main()
    {
        Float32x4 first;
        first.X = (float)1.0;
        first.Y = (float)2.0;
        first.Z = (float)3.0;
        first.W = (float)4.0;
        Float32x4 second;
        second.X = (float)5.0;
        second.Y = (float)6.0;
        second.Z = (float)7.0;
        second.W = (float)37.0;
        VectorPair pair;
        pair.First = first;
        pair.Second = second;
        EchoVector echoVector = ReturnVector;
        EchoPair echoPair = ReturnPair;
        Float32x4 vectorResult = echoVector(first);
        VectorPair pairResult = echoPair(pair);
        return (int)(vectorResult.W + pairResult.First.X + pairResult.Second.W);
    }
}
)NS");

    std::string diagnostic;
    tests.expect(compileAndRun(
        root / "VectorCallHva.nsproj", root / "VectorCallHvaO0.exe", "-O0", diagnostic),
        "vector and HVA returns execute at O0 (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(compileAndRun(
        root / "VectorCallHva.nsproj", root / "VectorCallHvaO2.exe", "-O2", diagnostic),
        "vector and HVA returns execute at O2 (" + diagnostic + ")");

    const std::string projectText = (root / "VectorCallHva.nsproj").string();
    const std::array<std::string_view, 2> irArguments = {
        "--dump-project-oop-llvm", projectText};
    std::string out;
    std::string err;
    const bool dumped = run(irArguments, out, err) == ExitCode::Success;
    tests.expect(dumped &&
            out.find("x86_vectorcallcc <4 x float>") != std::string::npos,
        "Float32x4 crosses vectorcall directly in XMM form (" + err + ")");
    tests.expect(dumped &&
            out.find("{ <4 x float>, <4 x float> }") != std::string::npos &&
            out.find("inreg") != std::string::npos,
        "two-member HVA is direct and carries Microsoft's inreg contract");
    tests.expect(dumped && out.find("vectorcall.return.bridge") != std::string::npos &&
            out.find("vectorcall.carrier.value") != std::string::npos,
        "vectorcall thunks bridge external vectors and stable internal carriers");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
