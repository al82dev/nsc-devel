#include "nativesharp/Basic/ExitCode.h"
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
#error "NSC_SOURCE_DIR must be defined for Stage 48.5 integration tests"
#endif

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::interop::LibraryMetadata;
using nativesharp::interop::LibraryMetadataCodec;
using nativesharp::linker::PeExecutableVerifier;
using nativesharp::tests::TestContext;

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream text;
    text << stream.rdbuf();
    return text.str();
}

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
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

std::size_t countOccurrences(std::string_view text, std::string_view needle)
{
    if (needle.empty()) return 0;
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string_view::npos)
    {
        ++count;
        position += needle.size();
    }
    return count;
}

void replaceFirst(std::string& text, std::string_view from, std::string_view to)
{
    const std::size_t position = text.find(from);
    if (position == std::string::npos) return;
    text.replace(position, from.size(), to);
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

bool validateManagedContract(
    std::string_view nativeMethods,
    std::string_view pair,
    std::string_view layout,
    std::string& error)
{
    if (countOccurrences(nativeMethods, "[DllImport(") != 21)
    {
        error = "managed host must declare exactly 21 P/Invoke imports";
        return false;
    }
    if (countOccurrences(nativeMethods, "ExactSpelling = true") != 21 ||
        countOccurrences(nativeMethods, "CallingConvention = CallingConvention.Winapi") != 21)
    {
        error = "every P/Invoke declaration must pin the exact C ABI name and Windows ABI convention";
        return false;
    }
    if (countOccurrences(nativeMethods, "[UnmanagedFunctionPointer(CallingConvention.Winapi)]") != 2)
    {
        error = "callbacks must use the same Windows ABI convention as NativeSharp extern C delegates";
        return false;
    }
    if (countOccurrences(nativeMethods, "MarshalAs(UnmanagedType.I1)") != 2)
    {
        error = "NativeSharp bool is a one-byte carrier and requires I1 marshaling in C#";
        return false;
    }
    if (nativeMethods.find("internal static extern nint CounterCreate") == std::string_view::npos ||
        nativeMethods.find("internal static extern int CounterGet(nint counter)") == std::string_view::npos ||
        nativeMethods.find("internal static extern void CounterDestroy(nint counter)") == std::string_view::npos)
    {
        error = "opaque NativeSharp class handles must remain pointer-sized managed nint values";
        return false;
    }
    if (nativeMethods.find("internal static extern int* EchoIntPointer(int* values)") == std::string_view::npos ||
        nativeMethods.find("internal static extern int InvokeBuffer(BufferCallback callback, int* values, int count)") ==
            std::string_view::npos)
    {
        error = "typed raw pointers and pointer+count buffers must remain explicit unsafe C# pointers";
        return false;
    }
    if (pair.find("[StructLayout(LayoutKind.Sequential)]") == std::string_view::npos ||
        layout.find("[StructLayout(LayoutKind.Sequential)]") == std::string_view::npos ||
        pair.find("Pack =") != std::string_view::npos || layout.find("Pack =") != std::string_view::npos)
    {
        error = "POD structs must use the native sequential layout without host-side packing overrides";
        return false;
    }
    error.clear();
    return true;
}

std::filesystem::path sourceRoot()
{
    return std::filesystem::path(NSC_SOURCE_DIR);
}

std::filesystem::path exampleRoot()
{
    return sourceRoot() / "examples" / "project_stage48_5";
}

void testNativeProductAndHeader(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path outputDirectory = root / "NativeProduct";
    std::filesystem::create_directories(outputDirectory);
    const std::filesystem::path dll = outputDirectory / "Stage48_5HostApi.dll";

    std::string output;
    std::string error;
    tests.expect(
        emitProduct(exampleRoot() / "Shared" / "Stage48_5HostApi.nsproj", dll, output, error) == ExitCode::Success,
        "Stage 48.5 emits the NativeSharp DLL consumed by the C# P/Invoke host");
    tests.expect(error.empty(), "valid Stage 48.5 library emission keeps stderr empty");
    tests.expect(
        std::filesystem::is_regular_file(outputDirectory / "Stage48_5HostApi.lib") &&
            std::filesystem::is_regular_file(outputDirectory / "Stage48_5HostApi.h") &&
            std::filesystem::is_regular_file(outputDirectory / "Stage48_5HostApi.nsmeta"),
        "C# host product still carries the common import library, C header and .nsmeta sidecars");
    tests.expect(output.find("exports: 46") != std::string::npos,
        "Stage 48.5 library reports 21 stable C wrappers plus its complete NativeSharp callable/type surface");

    const std::string header = readFile(outputDirectory / "Stage48_5HostApi.h");
    tests.expect(header.find("ABI: Windows x64 C. bool/byte/char use one-byte uint8_t carriers") != std::string::npos,
        "generated header explicitly documents one-byte NativeSharp bool/byte/char carriers");
    tests.expect(header.find("NS_API uint8_t NS_CALL NS485_EchoBool(uint8_t value);") != std::string::npos,
        "generated C contract proves NativeSharp bool is uint8_t at the host ABI");
    tests.expect(header.find("NS_API uint32_t NS_CALL NS485_EchoUInt(uint32_t value);") != std::string::npos &&
            header.find("NS_API int64_t NS_CALL NS485_EchoLong(int64_t value);") != std::string::npos &&
            header.find("NS_API uint64_t NS_CALL NS485_EchoULong(uint64_t value);") != std::string::npos,
        "generated C contract fixes 32-bit and 64-bit integer widths for managed interop");
    tests.expect(header.find("NS_API float NS_CALL NS485_EchoFloat(float value);") != std::string::npos &&
            header.find("NS_API double NS_CALL NS485_EchoDouble(double value);") != std::string::npos,
        "generated C contract fixes floating-point carriers for managed interop");
    tests.expect(header.find("typedef int32_t (NS_CALL *NS_IntCallback)(int32_t value);") != std::string::npos &&
            header.find("typedef int32_t (NS_CALL *NS_BufferCallback)(int32_t* values, int32_t count);") != std::string::npos,
        "generated header describes C callback signatures consumed by C# delegates");
    tests.expect(header.find("NS_STATIC_ASSERT(sizeof(NS_LayoutProbe) == 8") != std::string::npos &&
            header.find("offsetof(NS_LayoutProbe, Value) == 4") != std::string::npos,
        "generated header carries the POD size/offset contract that managed layout guards mirror");
    tests.expect(header.find("typedef struct NS_Counter NS_Counter;") != std::string::npos &&
            header.find("NS_API NS_Counter* NS_CALL NS485_Counter_Create") != std::string::npos,
        "class references remain opaque pointer-sized handles for P/Invoke");
    tests.expect(header.find("No hidden marshaling, retention or GC occurs at this boundary") != std::string::npos,
        "C ABI contract remains independent of the managed host runtime");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verifyDynamicLibrary(dll), "Stage 48.5 NativeSharp product is a valid PE32+ x64 DLL");
    if (verifier.verifyDynamicLibrary(dll))
    {
        const auto& info = verifier.info();
        tests.expect(info.machine == 0x8664 && info.optionalHeaderMagic == 0x20b,
            "P/Invoke target DLL is PE32+ AMD64");
        tests.expect(info.clrDirectoryRva == 0,
            "NativeSharp P/Invoke target contains no CLR header and remains fully native");
        tests.expect(info.exportedSymbols.size() == 46,
            "P/Invoke target DLL exports the 21-entry host contract and the complete NativeSharp surface");
        tests.expect(contains(info.exportedSymbols, "NS485_Add") &&
                contains(info.exportedSymbols, "NS485_Counter_Destroy") &&
                contains(info.exportedSymbols, "NS485_InvokeBuffer"),
            "representative scalar, ownership and callback entry points are exported undecorated");
        tests.expect(std::count_if(info.exportedSymbols.begin(), info.exportedSymbols.end(),
                         [](const std::string& symbol) { return symbol.find("HostApi") != std::string::npos; }) == 25,
            "every public HostApi method and its ABI support symbols have mangled NativeSharp exports");
    }
}

void testMetadataContract(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path outputDirectory = root / "Metadata";
    std::filesystem::create_directories(outputDirectory);
    const std::filesystem::path dll = outputDirectory / "Stage48_5HostApi.dll";
    std::string output;
    std::string error;
    tests.expect(
        emitProduct(exampleRoot() / "Shared" / "Stage48_5HostApi.nsproj", dll, output, error) == ExitCode::Success,
        "Stage 48.5 emits a second product for independent .nsmeta verification");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    tests.expect(codec.read(outputDirectory / "Stage48_5HostApi.nsmeta", metadata, error),
        "Stage 48.5 reads the emitted NativeSharp library metadata");
    tests.expect(error.empty(), "valid Stage 48.5 metadata has no read diagnostics");
    tests.expect(codec.verifyCurrentCompiler(metadata, error),
        "Stage 48.5 metadata is compatible with the current compiler metadata schema");
    tests.expect(metadata.moduleName == "Stage48_5HostApi" && metadata.pointerSize == 8 &&
            metadata.pointerAlignment == 8,
        "managed host metadata identifies the expected module and pointer-sized ABI");
    tests.expect(metadata.exports.size() == 21,
        ".nsmeta and the generated DLL agree on the 21-entry stable host surface");
    tests.expect(containsExport(metadata, "NS485_EchoBool") && containsExport(metadata, "NS485_EchoPair") &&
            containsExport(metadata, "NS485_Counter_Create") && containsExport(metadata, "NS485_SetCallback"),
        ".nsmeta records representative bool, POD, opaque-handle and callback exports");
    tests.expect(LibraryMetadataCodec::fingerprintHex(metadata.abiFingerprint).size() == 64,
        "managed consumers can pin the same deterministic 256-bit NativeSharp ABI fingerprint");
}

void testManagedSourceContract(TestContext& tests)
{
    const auto host = exampleRoot() / "Host";
    const std::string project = readFile(host / "Stage48_5CSharpHost.csproj");
    const std::string nativeMethods = readFile(host / "NativeMethods.cs");
    const std::string pair = readFile(host / "Pair.cs");
    const std::string layout = readFile(host / "LayoutProbe.cs");
    const std::string program = readFile(host / "Program.cs");
    const std::string handle = readFile(host / "CounterHandle.cs");
    const std::string mode = readFile(host / "HostMode.cs");
    const std::string script = readFile(exampleRoot() / "build_and_run_windows_csharp_host.cmd");

    tests.expect(project.find("<TargetFramework>net9.0</TargetFramework>") != std::string::npos &&
            project.find("<PlatformTarget>x64</PlatformTarget>") != std::string::npos,
        "managed host is explicitly a .NET 9 x64 console application");
    tests.expect(project.find("<AllowUnsafeBlocks>true</AllowUnsafeBlocks>") != std::string::npos &&
            project.find("<TreatWarningsAsErrors>true</TreatWarningsAsErrors>") != std::string::npos,
        "managed host enables explicit raw-pointer interop and warning-clean compilation");

    std::string error;
    tests.expect(validateManagedContract(nativeMethods, pair, layout, error),
        "C# declarations satisfy the Stage 48.5 managed ABI contract checker");
    tests.expect(error.empty(), "valid managed ABI contract checker returns no diagnostics");
    tests.expect(countOccurrences(nativeMethods, "[DllImport(") == 21,
        "C# host declares one exact DllImport per NativeSharp export");
    tests.expect(countOccurrences(nativeMethods, "ExactSpelling = true") == 21,
        "every P/Invoke declaration disables entry-point name rewriting");
    tests.expect(countOccurrences(nativeMethods, "CallingConvention = CallingConvention.Winapi") == 21,
        "every managed import names the Windows ABI convention explicitly");
    tests.expect(countOccurrences(nativeMethods, "MarshalAs(UnmanagedType.I1)") == 2,
        "C# bool parameter and return value use one-byte I1 marshaling rather than Win32 BOOL");
    tests.expect(nativeMethods.find("internal static extern byte EchoByte(byte value)") != std::string::npos &&
            nativeMethods.find("internal static extern uint EchoUInt(uint value)") != std::string::npos &&
            nativeMethods.find("internal static extern long EchoLong(long value)") != std::string::npos &&
            nativeMethods.find("internal static extern ulong EchoULong(ulong value)") != std::string::npos,
        "managed scalar declarations preserve NativeSharp carrier widths exactly");
    tests.expect(nativeMethods.find("internal static extern float EchoFloat(float value)") != std::string::npos &&
            nativeMethods.find("internal static extern double EchoDouble(double value)") != std::string::npos,
        "managed floating declarations map directly to native float/double");
    tests.expect(nativeMethods.find("internal static extern int* EchoIntPointer(int* values)") != std::string::npos &&
            nativeMethods.find("InvokeBuffer(BufferCallback callback, int* values, int count)") != std::string::npos,
        "raw NativeSharp pointers and pointer+count buffers stay explicit in unsafe C#");
    tests.expect(nativeMethods.find("internal static extern nint EchoPointer(nint value)") != std::string::npos &&
            nativeMethods.find("internal static extern nint CounterCreate(int value)") != std::string::npos,
        "void* and opaque object references use pointer-sized nint carriers");
    tests.expect(countOccurrences(nativeMethods, "[UnmanagedFunctionPointer(CallingConvention.Winapi)]") == 2,
        "extern C NativeSharp delegates map to explicitly attributed managed delegate types");
    tests.expect(nativeMethods.find("extern string") == std::string::npos,
        "Stage 48.5 avoids implicit string marshaling before the NativeSharp String contract is completed");

    tests.expect(pair.find("[StructLayout(LayoutKind.Sequential)]") != std::string::npos &&
            layout.find("[StructLayout(LayoutKind.Sequential)]") != std::string::npos &&
            pair.find("Pack =") == std::string::npos && layout.find("Pack =") == std::string::npos,
        "POD managed structs preserve native sequential alignment rather than forcing packing");
    tests.expect(mode.find("internal enum HostMode : int") != std::string::npos,
        "managed enum pins the NativeSharp enum carrier to signed 32-bit int");
    tests.expect(program.find("Marshal.SizeOf<Pair>() == 8") != std::string::npos &&
            program.find("Marshal.SizeOf<LayoutProbe>() == 8") != std::string::npos &&
            program.find("Marshal.OffsetOf<LayoutProbe>(nameof(LayoutProbe.Value)).ToInt64() == 4") !=
                std::string::npos,
        "managed executable checks ABI-visible struct sizes and offsets before invoking native code");
    tests.expect(program.find("fixed (int* valuesPointer = values)") != std::string::npos &&
            program.find("GC.KeepAlive(sum)") != std::string::npos,
        "managed buffer example pins array storage and keeps synchronous callback delegates live");
    tests.expect(program.find("private static readonly NativeMethods.IntCallback s_persistentCallback") !=
            std::string::npos && program.find("GC.Collect();") != std::string::npos &&
            program.find("GC.KeepAlive(s_persistentCallback)") != std::string::npos,
        "persistent callback example explicitly roots the delegate across forced GC activity");
    tests.expect(handle.find("internal sealed class CounterHandle : SafeHandle") != std::string::npos &&
            handle.find("NativeMethods.CounterDestroy(handle)") != std::string::npos,
        "C# host wraps NativeSharp create/destroy ownership with a managed SafeHandle");
    tests.expect(program.find("using CounterHandle counter = CounterHandle.Create(40);") != std::string::npos,
        "managed host demonstrates deterministic disposal of an opaque NativeSharp object handle");
    tests.expect(program.find("OperatingSystem.IsWindows()") != std::string::npos &&
            program.find("Architecture.X64") != std::string::npos,
        "managed executable rejects unsupported host OS/architecture before loading the NativeSharp DLL");

    tests.expect(script.find("--emit-project-oop-dll") != std::string::npos &&
            script.find("--verify-library-metadata") != std::string::npos &&
            script.find("dotnet build") != std::string::npos &&
            script.find("Stage48_5CSharpHost.exe") != std::string::npos,
        "Windows CMD example performs NativeSharp build, metadata verification, managed build and host execution");
    tests.expect(script.find("powershell") == std::string::npos && script.find("PowerShell") == std::string::npos,
        "Stage 48.5 Windows workflow requires no PowerShell");
}

void testManagedNegativeContracts(TestContext& tests)
{
    const auto host = exampleRoot() / "Host";
    const std::string validNativeMethods = readFile(host / "NativeMethods.cs");
    const std::string validPair = readFile(host / "Pair.cs");
    const std::string validLayout = readFile(host / "LayoutProbe.cs");
    std::string error;

    std::string badBool = validNativeMethods;
    while (badBool.find("[return: MarshalAs(UnmanagedType.I1)]\n") != std::string::npos)
    {
        replaceFirst(badBool, "[return: MarshalAs(UnmanagedType.I1)]\n", "");
    }
    while (badBool.find("[MarshalAs(UnmanagedType.I1)] ") != std::string::npos)
    {
        replaceFirst(badBool, "[MarshalAs(UnmanagedType.I1)] ", "");
    }
    tests.expect(!validateManagedContract(badBool, validPair, validLayout, error) &&
            error.find("one-byte") != std::string::npos,
        "contract checker rejects default C# bool marshaling because NativeSharp bool is one byte");

    std::string badLayout = validLayout;
    replaceFirst(badLayout, "[StructLayout(LayoutKind.Sequential)]", "[StructLayout(LayoutKind.Sequential, Pack = 1)]");
    tests.expect(!validateManagedContract(validNativeMethods, validPair, badLayout, error) &&
            error.find("packing") != std::string::npos,
        "contract checker rejects host-side Pack=1 that would corrupt NativeSharp POD layout");

    std::string badCallback = validNativeMethods;
    replaceFirst(
        badCallback,
        "[UnmanagedFunctionPointer(CallingConvention.Winapi)]",
        "[UnmanagedFunctionPointer(CallingConvention.Cdecl)]");
    tests.expect(!validateManagedContract(badCallback, validPair, validLayout, error) &&
            error.find("callbacks") != std::string::npos,
        "contract checker rejects a managed callback convention that differs from the declared host ABI");

    std::string badHandle = validNativeMethods;
    replaceFirst(
        badHandle,
        "internal static extern nint CounterCreate(int value)",
        "internal static extern int CounterCreate(int value)");
    tests.expect(!validateManagedContract(badHandle, validPair, validLayout, error) &&
            error.find("pointer-sized") != std::string::npos,
        "contract checker rejects truncating an opaque NativeSharp x64 handle to managed int");
}

bool dotnetAvailable(const std::filesystem::path& root)
{
#ifdef _WIN32
    return runCommand("where dotnet", root / "dotnet_probe.log");
#else
    return runCommand("command -v dotnet", root / "dotnet_probe.log");
#endif
}

void testManagedCompilationWhenSdkExists(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path managed = root / "ManagedBuild";
    std::error_code ec;
    std::filesystem::copy(
        exampleRoot() / "Host",
        managed,
        std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
        ec);
    tests.expect(!ec, "Stage 48.5 copies the packaged C# host into an isolated build directory");
    if (ec) return;

    if (!dotnetAvailable(root))
    {
        tests.expect(true,
            "dotnet SDK is unavailable in this sandbox; the C# source contract remains fully validated and the Windows CMD gate is packaged");
        return;
    }

    // Keep the integration gate independent from the developer profile. The sample has no
    // package dependencies, so an empty source set is the complete and deterministic input.
    const std::filesystem::path isolatedProfile = root / "DotNetProfile";
    const std::filesystem::path isolatedAppData = isolatedProfile / "AppData" / "Roaming";
    writeFile(isolatedAppData / "NuGet" / "NuGet.Config", R"XML(<?xml version="1.0" encoding="utf-8"?>
<configuration>
  <packageSources>
    <clear />
  </packageSources>
</configuration>
)XML");

#ifdef _WIN32
    const std::string command =
        "set \"APPDATA=" + isolatedAppData.string() + "\" && set \"DOTNET_CLI_HOME=" +
        isolatedProfile.string() +
        "\" && set \"DOTNET_CLI_TELEMETRY_OPTOUT=1\" && set \"DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1\" && dotnet build " +
        quote(managed / "Stage48_5CSharpHost.csproj") + " -c Release --nologo";
#else
    const std::string command =
        "DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1 dotnet build " +
        quote(managed / "Stage48_5CSharpHost.csproj") + " -c Release --nologo";
#endif
    const std::filesystem::path buildLogPath = root / "dotnet_build.log";
    const bool managedBuild = runCommand(command, buildLogPath);
    if (!managedBuild)
    {
        const std::string buildLog = readFile(buildLogPath);
        const bool targetingPacksUnavailable =
            buildLog.find("error NU1100") != std::string::npos &&
            buildLog.find("Microsoft.NETCore.App.Ref") != std::string::npos;
        if (targetingPacksUnavailable)
        {
            tests.expect(true,
                "dotnet is present without the .NET 9 targeting packs; the managed source contract remains fully validated");
            return;
        }
    }
    tests.expect(managedBuild,
        "when a usable .NET 9 SDK is installed, the packaged strict C# 13 P/Invoke host compiles successfully");
    tests.expect(managedBuild &&
            std::filesystem::is_regular_file(managed / "bin" / "Release" / "net9.0" / "Stage48_5CSharpHost.dll"),
        "managed build produces the expected .NET host assembly");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp-stage48_5-csharp-pinvoke-host-tests");
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);

    testNativeProductAndHeader(tests, root);
    testMetadataContract(tests, root);
    testManagedSourceContract(tests);
    testManagedNegativeContracts(tests);
    testManagedCompilationWhenSdkExists(tests, root);

    return tests.resultKeeping(root);
}
