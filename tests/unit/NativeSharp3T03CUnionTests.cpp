#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::driver::Driver;
using nativesharp::ExitCode;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view outputType,
    std::string_view source,
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>" + std::string(outputType) +
        "</OutputType></PropertyGroup><ItemGroup>" + std::string(items) +
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
}

ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

bool compileAndRun(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    int expected,
    std::string& diagnostic)
{
    const auto project = writeProject(directory, name, "Exe", source);
    const auto executable = directory / (std::string(name) + ".exe");
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result != ExitCode::Success) return false;
    return std::system(("\"" + executableText + "\"").c_str()) == expected;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t03_c_unions";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    tests.expect(
        compileAndRun(root / "active", "ActiveMember", R"NS(
namespace ActiveMember
{
    public copy union Word { public int Signed; public uint Unsigned; }
    public class Program
    {
        public static int Main()
        {
            Word value;
            value.Signed = 42;
            int answer = value.Signed;
            Word copy = value;
            if (copy.Signed != answer) return 1;
            Word aggregate = { 42 };
            return aggregate.Signed;
        }
    }
}
)NS", 42, diagnostic),
        "a direct write, aggregate initializer, and union copy preserve the proven active member (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        compileAndRun(root / "reinterpret", "NativeReinterpret", R"NS(
namespace NativeReinterpret
{
    public union WordBytes { public uint Whole; public byte First; }
    public class Program
    {
        public static int Main()
        {
            WordBytes value;
            value.Whole = 42;
            return (int)value.First;
        }
    }
}
)NS", 42, diagnostic),
        "native union access permits an explicit little-endian reinterpretation (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        compileAndRun(root / "inactive", "InactiveMember", R"NS(
namespace InactiveMember
{
    public union Word { public int Signed; public uint Unsigned; }
    public class Program { public static int Main() {
        Word value; value.Unsigned = 42; return value.Signed;
    } }
}
)NS", 42, diagnostic),
        "a union member can reinterpret storage written through another member (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        compileAndRun(root / "branch", "BranchReinterpret", R"NS(
namespace BranchReinterpret
{
    public union Word { public int Signed; public uint Unsigned; }
    public class Program { public static int Main() {
        Word value;
        if (true) value.Signed = 42; else value.Unsigned = 42;
        return value.Signed;
    } }
}
)NS", 42, diagnostic),
        "control-flow joins preserve the shared union storage (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        compileAndRun(root / "join", "JoinedMember", R"NS(
namespace JoinedMember
{
    public union Word { public int Signed; public uint Unsigned; }
    public class Program
    {
        public static int Main()
        {
            Word value;
            if (true) value.Signed = 42; else value.Signed = 1;
            return value.Signed;
        }
    }
}
)NS", 42, diagnostic),
        "control-flow joins retain identical active-member proofs (" + diagnostic + ")");

    const std::filesystem::path producer = root / "dll" / "Producer";
    const auto producerProject = writeProject(producer, "CUnionAbi", "SharedLibrary", R"NS(
namespace CUnionAbi
{
    public copy union Word { public int Signed; public uint Unsigned; }
    public class Api
    {
        public static export extern C int Read(Word value)
        {
            return value.Signed;
        }
    }
}
)NS");
    const std::string producerProjectText = producerProject.string();
    const std::string dllText = (producer / "CUnionAbi.dll").string();
    const std::array<std::string_view, 5> dllArguments = {
        "--emit-project-oop-dll", producerProjectText, "-O0", "-o", dllText};
    std::string output;
    std::string error;
    const ExitCode dllResult = runDriver(dllArguments, output, error);

    const std::filesystem::path imported = root / "dll" / "Imported";
    const auto importedProject = writeProject(
        imported,
        "Imported",
        "Exe",
        R"NS(
using CUnionAbi;
class Program { public static int Main() {
    Word value; value.Unsigned = 42; return value.Signed;
} }
)NS",
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/CUnionAbi.lib\" />");
    const std::string importedProjectText = importedProject.string();
    const std::array<std::string_view, 3> importedArguments = {
        "--check-project-oop-hir", importedProjectText, "-O0"};
    output.clear();
    error.clear();
    const ExitCode importedResult = dllResult == ExitCode::Success
        ? runDriver(importedArguments, output, error)
        : ExitCode::CompilationError;
    tests.expect(
        importedResult == ExitCode::Success,
        "an imported C union preserves native shared-storage access through NSMETA (" +
            error + output + ")");

    const std::string headerText = (producer / "CUnionAbi.h").string();
    const std::array<std::string_view, 4> headerArguments = {
        "--emit-project-host-header", producerProjectText, "-o", headerText};
    const ExitCode headerResult = runDriver(headerArguments, output, error);
    std::ostringstream headerBuffer;
    if (headerResult == ExitCode::Success)
        headerBuffer << std::ifstream(headerText, std::ios::binary).rdbuf();
    const std::string header = headerBuffer.str();
    tests.expect(
        headerResult == ExitCode::Success &&
            header.find("union NS_Word") != std::string::npos &&
            header.find("sizeof(NS_Word) == 4") != std::string::npos &&
            header.find("NS_ALIGNOF(NS_Word) == 4") != std::string::npos,
        "the generated C header publishes the equivalent union with exact Windows x64 size and alignment (" +
            error + output + ")");

    const std::filesystem::path consumer = root / "dll" / "Consumer";
    const auto consumerProject = writeProject(
        consumer,
        "CUnionConsumer",
        "Exe",
        R"NS(
using CUnionAbi;
class Program { public static int Main() {
    Word value; value.Signed = 42; return Api.Read(value);
} }
)NS",
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/CUnionAbi.lib\" />");
    const std::string consumerProjectText = consumerProject.string();
    const std::string consumerExecutable = (consumer / "CUnionConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArguments = {
        "--emit-project-oop-exe", consumerProjectText, "-O0", "-o", consumerExecutable};
    output.clear();
    error.clear();
    const ExitCode consumerResult = dllResult == ExitCode::Success
        ? runDriver(consumerArguments, output, error)
        : ExitCode::CompilationError;
    if (dllResult == ExitCode::Success && std::filesystem::exists(producer / "CUnionAbi.dll"))
        std::filesystem::copy_file(
            producer / "CUnionAbi.dll", consumer / "CUnionAbi.dll",
            std::filesystem::copy_options::overwrite_existing, cleanupError);
    tests.expect(
        consumerResult == ExitCode::Success &&
            std::system(("\"" + consumerExecutable + "\"").c_str()) == 42,
        "a C-compatible union crosses a NativeSharp DLL/import-library boundary at O0 (" +
            error + output + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
