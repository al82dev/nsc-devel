#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
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

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t30_console_input";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);

    const std::filesystem::path coreRoot = root / "core";
    const std::string coreProject = (coreRoot / "NativeSharp.Core.nsproj").generic_string();
    writeFile(coreRoot / "NativeSharp.Core.nsproj",
        "<Project><PropertyGroup><Name>NativeSharpCore</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(coreRoot / "Console.ns", R"NS(
namespace NativeSharp.Core
{
    public class Console
    {
        public static void Write(string value) { }
        public static void WriteLine(string value) { }
        public static void ErrorWrite(string value) { }
        public static void ErrorWriteLine(string value) { }
        public static void Write() { Write(""); }
        public static void WriteLine() { WriteLine(""); }
        public static void ErrorWrite() { ErrorWrite(""); }
        public static void ErrorWriteLine() { ErrorWriteLine(""); }

        public static owned string ReadLine(out int status)
        {
            status = -3;
            return null;
        }

        public static int ReadLine(char[] destination)
        {
            int status = 0;
            raii string line = ReadLine(out status);
            if (status != 0) { return status; }
            if (destination.Length < line.Length) { return -2; }
            int index = 0;
            while (index < line.Length)
            {
                destination[index] = line[index];
                index = index + 1;
            }
            return line.Length;
        }
    }
}
)NS");
    writeFile(root / "ConsoleInput.nsproj",
        "<Project><PropertyGroup><Name>ConsoleInput</Name></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\"/><ProjectReference Include=\"" + coreProject +
        "\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core;

public class Program
{
    public static int Main()
    {
        int status = 0;
        raii string first = Console.ReadLine(out status);
        if (status != 0) { return 10; }
        if (first != "alpha") { return 11; }

        char[] small = new char[2];
        int smallResult = Console.ReadLine(small);
        delete[] small;
        if (smallResult != -2) { return 20; }

        raii string empty = Console.ReadLine(out status);
        if (status != 0) { return 30; }
        if (empty == null || empty.Length != 0) { return 31; }

        raii string unicode = Console.ReadLine(out status);
        if (status != 0) { return 40; }
        if (unicode != "México") { return 41; }

        raii string eof = Console.ReadLine(out status);
        if (status != -1) { return 50; }
        if (eof != null) { return 51; }

        Console.Write();
        Console.Write("true ");
        Console.WriteLine("42");
        Console.WriteLine();
        Console.ErrorWrite();
        Console.ErrorWriteLine("7");
        Console.ErrorWriteLine();
        return 0;
    }
}
)NS");

    const std::string project = (root / "ConsoleInput.nsproj").string();
    const std::string executable = (root / "ConsoleInput.exe").string();
    const std::string developmentRuntime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", developmentRuntime.c_str());
    const std::array<std::string_view, 5> arguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    std::string output;
    std::string error;
    const bool emitted = runDriver(arguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "T30 Console fixture compiles and links at O0 (" + error + ")");

    if (emitted)
    {
        const std::filesystem::path input = root / "input.txt";
        const std::filesystem::path standardOutput = root / "stdout.txt";
        const std::filesystem::path standardError = root / "stderr.txt";
        writeFile(input, "alpha\r\ntoolong\n\nMéxico\r\n");
        const std::string command =
            "cmd.exe /d /s /c \"\"" + executable + "\" < \"" + input.string() +
            "\" > \"" + standardOutput.string() + "\" 2> \"" + standardError.string() + "\"\"";
        const int processResult = std::system(command.c_str());
        tests.expect(processResult == 0,
            "redirected stdin distinguishes lines, empty input, insufficient buffer, Unicode and EOF");
        tests.expect(readFile(standardOutput) == "true 42\r\n\r\n",
            "primitive and zero-argument Console overloads preserve the canonical formatting");
        tests.expect(readFile(standardError) == "7\r\n\r\n",
            "primitive and zero-argument Console error overloads preserve canonical formatting");

        const std::filesystem::path pipeOutput = root / "pipe-stdout.txt";
        const std::filesystem::path pipeError = root / "pipe-stderr.txt";
        const std::string pipeCommand =
            "cmd.exe /d /s /c \"type \"" + input.string() + "\" | \"" + executable +
            "\" > \"" + pipeOutput.string() + "\" 2> \"" + pipeError.string() + "\"\"";
        const int pipeResult = std::system(pipeCommand.c_str());
        tests.expect(pipeResult == 0 && readFile(pipeOutput) == "true 42\r\n\r\n" &&
                readFile(pipeError) == "7\r\n\r\n",
            "stdin consumes a redirected pipe with the same deterministic line contract");
    }

    nativesharp::runtime::WindowsRuntimeBuilder runtimeBuilder;
    std::string runtimeError;
    const std::unique_ptr<nativesharp::llvm_backend::LlvmModule> runtime =
        runtimeBuilder.buildSupport(runtimeError);
    tests.expect(runtime != nullptr && runtimeError.empty() &&
            runtime->text().find("define ptr @__nativesharp_console_readline(ptr") !=
                std::string::npos,
        "Windows runtime publishes the versioned Console.ReadLine hook (" + runtimeError + ")");

    const std::string consoleSource = readFile(
        std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/Console.ns");
    const std::array<std::string_view, 8> primitiveTypes = {
        "bool", "char", "int", "uint", "long", "ulong", "float", "double"};
    bool completePrimitiveSurface = true;
    for (const std::string_view type : primitiveTypes)
    {
        for (const std::string_view operation :
            {"Write", "WriteLine", "ErrorWrite", "ErrorWriteLine"})
        {
            const std::string signature =
                "public static void " + std::string(operation) + "(" + std::string(type) + " value)";
            completePrimitiveSurface =
                completePrimitiveSurface && consoleSource.find(signature) != std::string::npos;
        }
    }
    tests.expect(completePrimitiveSurface,
        "NativeSharp.Core.Console publishes all primitive stdout/stderr overloads");
    tests.expect(consoleSource.find("public static void Write()") != std::string::npos &&
            consoleSource.find("public static void WriteLine()") != std::string::npos &&
            consoleSource.find("public static void ErrorWrite()") != std::string::npos &&
            consoleSource.find("public static void ErrorWriteLine()") != std::string::npos,
        "NativeSharp.Core.Console publishes zero-argument stdout/stderr overloads");

    return tests.resultKeeping(root);
}
