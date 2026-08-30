#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef NSC_SOURCE_DIR
#error "NSC_SOURCE_DIR must be defined for Stage 48.6 integration tests"
#endif

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::interop::LibraryMetadata;
using nativesharp::interop::LibraryMetadataCodec;
using nativesharp::linker::PeExecutableVerifier;
using nativesharp::tests::TestContext;

std::filesystem::path sourceRoot()
{
    return std::filesystem::path(NSC_SOURCE_DIR);
}

std::filesystem::path exampleRoot()
{
    return sourceRoot() / "examples" / "project_stage48_6";
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream text;
    text << stream.rdbuf();
    return text.str();
}

std::string quote(const std::filesystem::path& path)
{
    return "\"" + path.string() + "\"";
}

bool runCommand(std::string command, const std::filesystem::path& logPath)
{
    command += " >" + quote(logPath) + " 2>&1";
#ifdef _WIN32
    // See the matching comment in Stage47HostCAbiTests.cpp -- cmd.exe /c misparses a
    // command line with more than two quote characters, wrapping the whole thing in one
    // more outer pair of quotes is the documented workaround.
    command = "\"" + command + "\"";
#endif
    return std::system(command.c_str()) == 0;
}

bool commandAvailable(std::string_view command)
{
#ifdef _WIN32
    return std::system(("where " + std::string(command) + " >nul 2>nul").c_str()) == 0;
#else
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
#endif
}

std::size_t countOccurrences(std::string_view text, std::string_view needle)
{
    std::size_t count = 0;
    std::size_t position = 0;
    while (!needle.empty() && (position = text.find(needle, position)) != std::string_view::npos)
    {
        ++count;
        position += needle.size();
    }
    return count;
}

ExitCode runDriver(
    std::span<const std::string_view> args,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    output = out.str();
    error = err.str();
    return result;
}

ExitCode emitProduct(
    const std::filesystem::path& project,
    const std::filesystem::path& outputPath,
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::string outputText = outputPath.string();
    const std::array<std::string_view, 5> args = {
        "-O2", "--emit-project-oop-dll", projectText, "-o", outputText};
    return runDriver(args, output, error);
}

bool contains(const std::vector<std::string>& values, std::string_view expected)
{
    return std::find(values.begin(), values.end(), expected) != values.end();
}

bool containsExport(const LibraryMetadata& metadata, std::string_view name)
{
    return std::find_if(metadata.exports.begin(), metadata.exports.end(), [name](const auto& item) {
        return item.cAbiName == name;
    }) != metadata.exports.end();
}

void testNativeProduct(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path outputDirectory = root / "NativeProduct";
    std::filesystem::create_directories(outputDirectory);
    const std::filesystem::path dll = outputDirectory / "Stage48_6HostApi.dll";

    std::string output;
    std::string error;
    tests.expect(
        emitProduct(exampleRoot() / "Native" / "Stage48_6HostApi.nsproj", dll, output, error) == ExitCode::Success,
        "Stage 48.6 emits the native DLL consumed by the C++/CLI bridge");
    tests.expect(error.empty(), "valid Stage 48.6 native product emission keeps stderr empty");
    tests.expect(std::filesystem::is_regular_file(dll), "Stage 48.6 emits the requested DLL");
    tests.expect(std::filesystem::is_regular_file(outputDirectory / "Stage48_6HostApi.lib"),
        "Stage 48.6 emits the import library used by MSVC C++/CLI");
    tests.expect(std::filesystem::is_regular_file(outputDirectory / "Stage48_6HostApi.h"),
        "Stage 48.6 emits the same generated C/C++ ABI header");
    tests.expect(std::filesystem::is_regular_file(outputDirectory / "Stage48_6HostApi.nsmeta"),
        "Stage 48.6 emits compiler metadata beside the native product");
    tests.expect(output.find("exports: 21") != std::string::npos,
        "Stage 48.6 native product reports exactly 21 stable C ABI exports");

    const std::string header = readFile(outputDirectory / "Stage48_6HostApi.h");
    tests.expect(header.find("#ifdef __cplusplus") != std::string::npos &&
            header.find("extern \"C\"") != std::string::npos,
        "generated Stage 48.6 header remains C++ compatible through extern C");
    tests.expect(header.find("NS_API int32_t NS_CALL NS486_Add") != std::string::npos,
        "generated header exposes the scalar bridge entry point through C ABI");
    tests.expect(header.find("NS_API uint8_t NS_CALL NS486_EchoBool") != std::string::npos,
        "generated header keeps bool as the one-byte ABI carrier");
    tests.expect(header.find("NS_API int32_t NS_CALL NS486_BufferCount(int32_t* values, int32_t count)") != std::string::npos,
        "generated header exposes explicit pointer-plus-count buffer ABI");
    tests.expect(header.find("typedef int32_t (NS_CALL *NS_IntCallback)(int32_t value);") != std::string::npos,
        "generated header exposes the callback as a C function pointer");
    tests.expect(header.find("typedef struct NS_Counter NS_Counter;") != std::string::npos,
        "generated header keeps class state behind an opaque handle");
    tests.expect(header.find("NS_API NS_Counter* NS_CALL NS486_Counter_Create") != std::string::npos &&
            header.find("NS_API void NS_CALL NS486_Counter_Destroy") != std::string::npos,
        "generated header preserves explicit create/destroy ownership for managed bridging");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verifyDynamicLibrary(dll), "Stage 48.6 native product is a valid PE32+ x64 DLL");
    if (verifier.verifyDynamicLibrary(dll))
    {
        const auto& info = verifier.info();
        tests.expect(info.machine == 0x8664, "Stage 48.6 NativeSharp DLL targets AMD64");
        tests.expect(info.optionalHeaderMagic == 0x20b, "Stage 48.6 NativeSharp DLL uses PE32+");
        tests.expect(info.clrDirectoryRva == 0, "NativeSharp side of the C++/CLI bridge has no CLR header");
        tests.expect(info.exportedSymbols.size() == 21, "Stage 48.6 native DLL exports exactly the bridge ABI surface");
        tests.expect(contains(info.exportedSymbols, "NS486_Add") && contains(info.exportedSymbols, "NS486_Invoke") &&
                contains(info.exportedSymbols, "NS486_Counter_Destroy"),
            "representative scalar, callback and lifetime exports remain undecorated");
    }

    LibraryMetadata metadata;
    LibraryMetadataCodec codec;
    tests.expect(codec.read(outputDirectory / "Stage48_6HostApi.nsmeta", metadata, error),
        "Stage 48.6 .nsmeta round-trips successfully");
    tests.expect(error.empty(), "valid Stage 48.6 .nsmeta verification keeps diagnostics empty");
    tests.expect(metadata.producerVersion == nativesharp::kCompilerVersion,
        "Stage 48.6 metadata records the current compiler version");
    tests.expect(metadata.moduleName == "Stage48_6HostApi", "Stage 48.6 metadata records the expected module name");
    tests.expect(metadata.exports.size() == 21, "Stage 48.6 metadata records all 21 C ABI exports");
    tests.expect(containsExport(metadata, "NS486_Add") && containsExport(metadata, "NS486_BufferCount") &&
            containsExport(metadata, "NS486_Counter_Create"),
        "Stage 48.6 metadata agrees with scalar, buffer and handle bridge entry points");
}

void testCppCliProjectContract(TestContext& tests)
{
    const std::filesystem::path bridge = exampleRoot() / "Bridge";
    const std::string project = readFile(bridge / "Stage48_6CppCliBridge.vcxproj");
    const std::string bridgeHeader = readFile(bridge / "NativeSharpBridge.h");
    const std::string bridgeSource = readFile(bridge / "NativeSharpBridge.cpp");
    const std::string counterHeader = readFile(bridge / "Counter.h");
    const std::string counterSource = readFile(bridge / "Counter.cpp");
    const std::string callback = readFile(bridge / "IntCallback.h");

    tests.expect(project.find("<ConfigurationType>DynamicLibrary</ConfigurationType>") != std::string::npos,
        "C++/CLI .NET bridge is packaged as a DLL rather than an unsupported managed executable");
    tests.expect(project.find("<CLRSupport>NetCore</CLRSupport>") != std::string::npos,
        "C++/CLI bridge enables the modern /clr:netcore toolchain");
    tests.expect(project.find("<TargetFramework>net9.0</TargetFramework>") != std::string::npos,
        "C++/CLI bridge targets the same .NET 9 baseline as Stage 48.5");
    tests.expect(project.find("Release|x64") != std::string::npos,
        "C++/CLI bridge pins the supported host architecture to x64");
    tests.expect(project.find("$(NativeSharpProductDir)") != std::string::npos,
        "C++/CLI project receives NativeSharp SDK products through an explicit product directory");
    tests.expect(project.find("Stage48_6HostApi.lib") != std::string::npos,
        "C++/CLI bridge links the NativeSharp import library directly");
    tests.expect(project.find("<LanguageStandard>stdcpp20</LanguageStandard>") != std::string::npos,
        "native portion of the C++/CLI bridge uses C++20");
    tests.expect(project.find("<TreatWarningAsError>true</TreatWarningAsError>") != std::string::npos,
        "C++/CLI bridge treats MSVC warnings as errors");

    tests.expect(bridgeSource.find("#include \"Stage48_6HostApi.h\"") != std::string::npos,
        "C++/CLI implementation consumes the generated Stage 48.1 header");
    tests.expect(bridgeSource.find("DllImport") == std::string::npos && bridgeHeader.find("DllImport") == std::string::npos,
        "C++/CLI bridge does not invent a second P/Invoke boundary");
    tests.expect(countOccurrences(bridgeSource, "NS486_") >= 14,
        "C++/CLI bridge calls the stable C ABI rather than NativeSharp internal symbols");
    tests.expect(bridgeSource.find("Marshal::GetFunctionPointerForDelegate") != std::string::npos,
        "managed callbacks are projected to the C ABI through explicit delegate thunks");
    tests.expect(bridgeSource.find("storedCallback_ = callback") != std::string::npos &&
            bridgeSource.find("GC::KeepAlive(storedCallback_)") != std::string::npos,
        "persistent managed callback lifetime is explicitly rooted across the native boundary");
    tests.expect(bridgeSource.find("pin_ptr<int>") != std::string::npos &&
            bridgeSource.find("NS486_BufferCount(pinned, values->Length)") != std::string::npos,
        "managed arrays cross the boundary only while pinned and as pointer-plus-count");
    tests.expect(bridgeSource.find("NS486_EchoBool(value ? 1u : 0u) != 0") != std::string::npos,
        "C++/CLI bridge explicitly respects NativeSharp's one-byte bool carrier");
    tests.expect(bridgeSource.find("NS_Pair nativeValue") != std::string::npos,
        "managed value structs are copied explicitly into the generated native POD representation");

    tests.expect(counterHeader.find("public ref class Counter sealed") != std::string::npos,
        "opaque NativeSharp class handle is projected as a sealed managed reference type");
    tests.expect(counterHeader.find("~Counter();") != std::string::npos && counterHeader.find("!Counter();") != std::string::npos,
        "C++/CLI wrapper exposes deterministic Dispose semantics plus finalizer fallback");
    tests.expect(counterHeader.find("IntPtr handle_;") != std::string::npos,
        "managed Counter stores only a pointer-sized opaque handle");
    tests.expect(counterSource.find("NS486_Counter_Create") != std::string::npos &&
            counterSource.find("NS486_Counter_Destroy") != std::string::npos,
        "managed Counter owns exactly the C ABI create/destroy lifetime pair");
    tests.expect(counterSource.find("ObjectDisposedException") != std::string::npos,
        "managed wrapper rejects use after deterministic disposal");

    tests.expect(callback.find("[UnmanagedFunctionPointer(CallingConvention::Winapi)]") != std::string::npos,
        "C++/CLI callback declaration pins the Windows C ABI calling convention");
}

void testManagedConsumerAndScript(TestContext& tests)
{
    const std::filesystem::path host = exampleRoot() / "Host";
    const std::string project = readFile(host / "Stage48_6ManagedHost.csproj");
    const std::string source = readFile(host / "Program.cs");
    const std::string script = readFile(exampleRoot() / "build_and_run_windows_cppcli_host.cmd");

    tests.expect(project.find("<TargetFramework>net9.0</TargetFramework>") != std::string::npos,
        "managed consumer targets .NET 9");
    tests.expect(project.find("<PlatformTarget>x64</PlatformTarget>") != std::string::npos,
        "managed consumer is pinned to x64 to match NativeSharp pointers");
    tests.expect(project.find("Stage48_6CppCliBridge") != std::string::npos &&
            project.find("$(BridgePath)") != std::string::npos,
        "managed consumer references the built C++/CLI bridge assembly explicitly");
    tests.expect(source.find("using NativeSharp.Stage48_6.Managed") != std::string::npos,
        "managed consumer sees the C++/CLI projection rather than NativeSharp internals");
    tests.expect(source.find("DllImport") == std::string::npos,
        "C# consumer no longer needs direct P/Invoke when exercising the C++/CLI bridge");
    tests.expect(source.find("GC.Collect()") != std::string::npos && source.find("CallCallback(21)") != std::string::npos,
        "end-to-end host deliberately stresses persistent callback rooting across GC");
    tests.expect(source.find("using Counter counter = new(40)") != std::string::npos,
        "managed host exercises deterministic disposal of an opaque native resource");

    tests.expect(script.find("where msbuild") != std::string::npos,
        "Windows gate explicitly requires the MSVC/MSBuild C++/CLI toolchain");
    tests.expect(script.find("where dotnet") != std::string::npos,
        "Windows gate explicitly requires the .NET 9 SDK");
    tests.expect(script.find("--emit-project-oop-dll") != std::string::npos &&
            script.find("--verify-library-metadata") != std::string::npos,
        "Windows gate builds and verifies the NativeSharp library before the bridge");
    tests.expect(script.find("Stage48_6CppCliBridge.vcxproj") != std::string::npos &&
            script.find("/p:NativeSharpProductDir=") != std::string::npos,
        "Windows gate passes generated header/import library location into MSBuild");
    tests.expect(script.find("dotnet build") != std::string::npos &&
            script.find("Stage48_6ManagedHost.exe") != std::string::npos,
        "Windows gate builds and executes the final managed consumer");
    tests.expect(script.find("powershell") == std::string::npos && script.find("PowerShell") == std::string::npos,
        "Stage 48.6 Windows validation remains pure CMD with no PowerShell dependency");
}

void testNativeHeaderCompilerProbe(TestContext& tests, const std::filesystem::path& root)
{
    if (!commandAvailable("clang++"))
    {
        tests.expect(true, "clang++ is unavailable; C++ header probe remains covered by Stage 48.4 regression");
        return;
    }

    const std::filesystem::path outputDirectory = root / "HeaderProbe";
    std::filesystem::create_directories(outputDirectory);
    const std::filesystem::path dll = outputDirectory / "Stage48_6HostApi.dll";
    std::string output;
    std::string error;
    if (emitProduct(exampleRoot() / "Native" / "Stage48_6HostApi.nsproj", dll, output, error) != ExitCode::Success)
    {
        tests.expect(false, "header probe requires successful NativeSharp product emission");
        return;
    }

    const std::filesystem::path source = outputDirectory / "probe.cpp";
    const std::filesystem::path object = outputDirectory / "probe.obj";
    {
        std::ofstream stream(source);
        stream << "#include \"Stage48_6HostApi.h\"\n"
               << "static_assert(sizeof(NS_Pair) == 8);\n"
               << "static_assert(sizeof(NS_LayoutProbe) == 8);\n"
               << "int32_t Probe(NS_Counter* counter) { return NS486_Counter_Get(counter) + NS486_Add(20, 22); }\n";
    }
    const std::string command =
        "clang++ --target=x86_64-pc-windows-msvc -fms-extensions -fdeclspec -std=c++20 "
        "-Wall -Wextra -Werror -Wpedantic -ffreestanding -fno-exceptions -fno-rtti -c -I" +
        quote(outputDirectory) + " " + quote(source) + " -o " + quote(object);
    tests.expect(runCommand(command, outputDirectory / "probe.log"),
        "the exact generated Stage 48.6 C ABI header compiles as strict Windows-targeted C++20");
    tests.expect(std::filesystem::is_regular_file(object),
        "C++20 header probe produces the expected Windows COFF object");
}

void testConsolidatedStage48Surface(TestContext& tests)
{
    const std::filesystem::path testsRoot = sourceRoot() / "tests" / "unit";
    tests.expect(std::filesystem::is_regular_file(testsRoot / "Stage48_1CHeaderGeneratorTests.cpp"),
        "Stage 48 consolidation retains 48.1 header certification");
    tests.expect(std::filesystem::is_regular_file(testsRoot / "Stage48_2LibraryMetadataTests.cpp"),
        "Stage 48 consolidation retains 48.2 metadata certification");
    tests.expect(std::filesystem::is_regular_file(testsRoot / "Stage48_3CHostIntegrationTests.cpp"),
        "Stage 48 consolidation retains 48.3 C host certification");
    tests.expect(std::filesystem::is_regular_file(testsRoot / "Stage48_4CppHostIntegrationTests.cpp"),
        "Stage 48 consolidation retains 48.4 C++ host certification");
    tests.expect(std::filesystem::is_regular_file(testsRoot / "Stage48_5CSharpPInvokeHostTests.cpp"),
        "Stage 48 consolidation retains 48.5 C# host certification");
    tests.expect(nativesharp::kCompilerVersion.starts_with("0.1.0-dev") && !nativesharp::kCompilerStage.empty(),
        "compiler identity remains on the user-selected NativeSharp 0.1 development line after the Stage 48 host-SDK closure");
}

void testCppCliBuildWhenAvailable(TestContext& tests, const std::filesystem::path& root)
{
#ifdef _WIN32
    if (!commandAvailable("msbuild") || !commandAvailable("dotnet"))
    {
        tests.expect(true,
            "MSBuild/.NET SDK are unavailable; packaged Windows C++/CLI end-to-end gate remains external");
        return;
    }
    const std::string command = "cmd /c " + quote(exampleRoot() / "build_and_run_windows_cppcli_host.cmd");
    tests.expect(runCommand(command, root / "cppcli_windows_gate.log"),
        "when MSVC C++/CLI and .NET 9 are installed, the packaged Stage 48.6 end-to-end bridge gate passes");
#else
    (void)root;
    tests.expect(true,
        "C++/CLI for modern .NET is Windows/MSVC-only; source and native ABI are validated here and the end-to-end gate is packaged for Windows");
#endif
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp-stage48_6-cppcli-stage48-closure-tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);

    testNativeProduct(tests, root);
    testCppCliProjectContract(tests);
    testManagedConsumerAndScript(tests);
    testNativeHeaderCompilerProbe(tests, root);
    testConsolidatedStage48Surface(tests);
    testCppCliBuildWhenAvailable(tests, root);

    std::filesystem::remove_all(root, ec);
    return tests.result();
}
