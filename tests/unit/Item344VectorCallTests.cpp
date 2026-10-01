#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

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

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
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
        std::filesystem::path(NSC_BINARY_DIR) / "item344_vectorcall";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);

    const std::string source = R"NS(
public extern vectorcall delegate float VectorSum(
    float a, float b, float c, float d, float e, float f);

public class Program
{
    public static float Sum(float a, float b, float c, float d, float e, float f)
    {
        return a + b + c + d + e + f;
    }

    public static int Main()
    {
        VectorSum sum = Sum;
        return (int)sum(
            (float)1.0, (float)2.0, (float)3.0,
            (float)4.0, (float)5.0, (float)27.0);
    }
}
)NS";
    writeFile(root / "VectorCall.nsproj",
        "<Project><PropertyGroup><Name>VectorCall</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", source);

    std::string diagnostic;
    tests.expect(compileAndRun(
        root / "VectorCall.nsproj", root / "VectorCallO0.exe", "-O0", diagnostic),
        "six-register vectorcall executes at O0 (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(compileAndRun(
        root / "VectorCall.nsproj", root / "VectorCallO2.exe", "-O2", diagnostic),
        "six-register vectorcall executes at O2 (" + diagnostic + ")");

    const std::string projectText = (root / "VectorCall.nsproj").string();
    const std::array<std::string_view, 2> irArguments = {"--dump-project-oop-llvm", projectText};
    std::string out;
    std::string err;
    tests.expect(run(irArguments, out, err) == ExitCode::Success &&
            out.find("x86_vectorcallcc") != std::string::npos &&
            out.find("__vectorcall_boundary") != std::string::npos,
        "LLVM IR marks both vectorcall thunk and indirect call (" + err + ")");

    const auto contract = root / "contract";
    writeFile(contract / "VectorContract.nsproj",
        "<Project><PropertyGroup><Name>VectorContract</Name><OutputType>Library</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(contract / "Api.ns", R"NS(
public extern vectorcall delegate float VectorSum(
    float a, float b, float c, float d, float e, float f);
public class Api
{
    public static export extern C float Apply(VectorSum callback)
    {
        return callback(
            (float)1.0, (float)2.0, (float)3.0,
            (float)4.0, (float)5.0, (float)27.0);
    }
}
)NS");
    const std::string contractProject = (contract / "VectorContract.nsproj").string();
    const std::string library = (contract / "VectorContract.lib").string();
    const std::array<std::string_view, 5> libArguments = {
        "-O0", "--emit-project-oop-lib", contractProject, "-o", library};
    out.clear();
    err.clear();
    const bool emitted = run(libArguments, out, err) == ExitCode::Success;
    tests.expect(emitted, "vectorcall contract emits library, header, and metadata (" + err + ")");
    const std::string header = readFile(contract / "VectorContract.h");
    tests.expect(header.find("NS_VECTORCALL *NS_VectorSum") != std::string::npos &&
            header.find("#define NS_VECTORCALL __vectorcall") != std::string::npos,
        "generated C header publishes the vectorcall function-pointer typedef");

    nativesharp::interop::LibraryMetadata metadata;
    nativesharp::interop::LibraryMetadataCodec codec;
    std::string metadataError;
    const bool metadataRead = emitted &&
        codec.read(contract / "VectorContract.nsmeta", metadata, metadataError);
    bool foundVectorCall = false;
    for (const auto& type : metadata.types)
        foundVectorCall = foundVectorCall || (type.fullyQualifiedName == "VectorSum" &&
            (type.delegateFlags & 3U) == 3U);
    tests.expect(metadataRead && metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor && foundVectorCall,
        "current NSMETA preserves the vectorcall delegate flag (" + metadataError + ")");
    metadata.formatMinor = 19U;
    metadataError.clear();
    tests.expect(metadataRead &&
            !codec.writeModel(metadata, contract / "unsupported-1.19.nsmeta", metadataError) &&
            metadataError.find("before 1.20") != std::string::npos,
        "NSMETA rejects a lossy vectorcall downlevel (" + metadataError + ")");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
