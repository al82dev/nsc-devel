#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::linker::LldLinker;
using nativesharp::linker::PeExecutableVerifier;
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
    std::string_view action,
    const std::filesystem::path& project,
    const std::filesystem::path& outputPath,
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::string outputText = outputPath.string();
    const std::array<std::string_view, 5> args = {
        "-O2", action, projectText, "-o", outputText};
    return runDriver(args, output, error);
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view outputType,
    std::string_view source)
{
    writeFile(
        directory / "Stage48_3HostApi.nsproj",
        "<Project><PropertyGroup><Name>Stage48_3HostApi</Name><OutputType>" +
            std::string(outputType) +
            "</OutputType></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
            "</ItemGroup></Project>");
    writeFile(directory / "Library.ns", source);
}

const char* apiSource()
{
    return R"NS(
namespace NativeSharp.Stage48_3
{
    extern C delegate int IntCallback(int value);
    extern C delegate int BufferCallback(int* values, int count);

    public enum HostMode
    {
        First = 1,
        Second = 2
    }

    public struct Pair
    {
        public int Left;
        public int Right;
    }

    public struct LayoutProbe
    {
        public byte Tag;
        public int Value;
    }

    public class Counter
    {
        public int Value;
        public Counter(int value) { Value = value; }
    }

    public class HostApi
    {
        private static IntCallback StoredCallback;

        public static export extern C int NS48_Add(int left, int right) { return left + right; }
        public static export extern C int* NS48_EchoIntPointer(int* values) { return values; }
        public static export extern C int NS48_InvokeBuffer(BufferCallback callback, int* values, int count)
        {
            return callback(values, count);
        }
        public static export extern C void* NS48_EchoPointer(void* value) { return value; }
        public static export extern C Pair NS48_EchoPair(Pair value) { return value; }
        public static export extern C HostMode NS48_EchoMode(HostMode value) { return value; }
        public static export extern C LayoutProbe NS48_EchoLayout(LayoutProbe value) { return value; }
        public static export extern C int NS48_Invoke(IntCallback callback, int value) { return callback(value); }
        public static export extern C void NS48_SetCallback(IntCallback callback) { StoredCallback = callback; }
        public static export extern C int NS48_CallCallback(int value) { return StoredCallback(value); }
        public static export extern C Counter NS48_Counter_Create(int value) { return new Counter(value); }
        public static export extern C int NS48_Counter_Get(Counter counter) { return counter.Value; }
        public static export extern C void NS48_Counter_Add(Counter counter, int delta)
        {
            counter.Value = counter.Value + delta;
        }
        public static export extern C void NS48_Counter_Destroy(Counter counter) { delete counter; }
    }
}
)NS";
}

const char* cHostSource()
{
    return R"C(
#include "Stage48_3HostApi.h"

static int32_t Twice(int32_t value)
{
    return value * 2;
}

static int32_t SumBuffer(int32_t* values, int32_t count)
{
    int32_t sum = 0;
    int32_t i;
    for (i = 0; i < count; ++i)
    {
        sum += values[i];
    }
    return sum;
}

int main(void)
{
    int32_t values[4] = {1, 2, 3, 4};
    NS_Pair pair = {20, 22};
    NS_LayoutProbe layout = {1, 42};
    NS_Counter* counter;
    int marker = 7;

    if (NS48_Add(20, 22) != 42) return 1;
    if (NS48_EchoIntPointer(values) != values) return 2;
    if (NS48_InvokeBuffer(SumBuffer, values, 4) != 10) return 3;
    if (NS48_EchoPointer(&marker) != &marker) return 4;
    pair = NS48_EchoPair(pair);
    if (pair.Left != 20 || pair.Right != 22) return 5;
    if (NS48_EchoMode((NS_HostMode)2) != (NS_HostMode)2) return 6;
    layout = NS48_EchoLayout(layout);
    if (layout.Tag != 1 || layout.Value != 42) return 7;
    if (NS48_Invoke(Twice, 21) != 42) return 8;
    NS48_SetCallback(Twice);
    if (NS48_CallCallback(21) != 42) return 9;

    counter = NS48_Counter_Create(40);
    if (counter == NULL) return 10;
    NS48_Counter_Add(counter, 2);
    if (NS48_Counter_Get(counter) != 42)
    {
        NS48_Counter_Destroy(counter);
        return 11;
    }
    NS48_Counter_Destroy(counter);
    return 0;
}
)C";
}

bool contains(const std::vector<std::string>& values, std::string_view expected)
{
    for (const std::string& value : values)
    {
        if (value == expected) return true;
    }
    return false;
}

bool importsSymbol(
    const nativesharp::linker::PeExecutableInfo& info,
    std::string_view library,
    std::string_view symbol)
{
    for (const auto& imported : info.importedSymbols)
    {
        if (imported.library == library && imported.name == symbol) return true;
    }
    return false;
}

bool createRuntimeImports(
    const LldLinker& linker,
    const std::filesystem::path& directory,
    std::vector<std::filesystem::path>& imports,
    std::string& error)
{
    const std::array<std::string_view, 62> kernel32Symbols = {
        "ExitProcess", "GetProcessHeap", "HeapAlloc", "HeapFree", "GetStdHandle",
        "WriteFile", "TlsAlloc", "TlsGetValue", "TlsSetValue", "RaiseException",
        // Stage 53.1 "Time" -- NativeClock's three raw wrappers import these directly.
        "QueryPerformanceCounter", "QueryPerformanceFrequency", "GetSystemTimeAsFileTime",
        "GetTickCount64", "GetActiveProcessorCount",
        "GetEnvironmentVariableA", "SetEnvironmentVariableA",
        // Stage 53.3 "Filesystem/Paths" -- File/Directory's Win32 wrappers import these directly.
        "GetFileAttributesA", "DeleteFileA", "CreateDirectoryA",
        // Stage 53.3b -- File.ReadAllText/WriteAllText/AppendAllText's Win32 wrappers.
        "CreateFileA", "GetFileSizeEx", "ReadFile", "CloseHandle", "SetFilePointer",
        // Stage 53.4 "Filesystem/Paths (cierre)" -- Directory.Delete/Path.GetFullPath's Win32
        // wrappers.
        "RemoveDirectoryA", "GetFullPathNameA",
        // Stage 53.5 "Command-line arguments" -- Environment.GetCommandLineArgCount/
        // GetCommandLineArgs's Win32 wrappers import these directly from KERNEL32.dll.
        // CommandLineToArgvW itself lives in SHELL32.dll -- see the separate shell32 import
        // library below. The runtime module always declares/calls all four regardless of
        // whether THIS test's own host program happens to use them, so any static-library host
        // link needs the full symbol surface, not just what the test program itself calls.
        "GetCommandLineW", "WideCharToMultiByte", "LocalFree",
        // Stage 53.6 "Enumeracion de directorios" -- Directory.GetFileCount/GetFiles/
        // GetDirectoryCount/GetDirectories's Win32 wrappers import these directly.
        "FindFirstFileA", "FindNextFileA", "FindClose",
        // Stage 54.1 "Threads" -- Thread's ThreadCreate/ThreadJoin/ThreadDetach's Win32
        // wrappers import these directly.
        "CreateThread", "WaitForSingleObject", "CreateMutexA", "ReleaseMutex", "GetCurrentThreadId",
        // Stage 54.4 "Condition Variables" -- Monitor's Win32 wrappers import these directly.
        "AcquireSRWLockExclusive", "ReleaseSRWLockExclusive", "WakeConditionVariable",
        "WakeAllConditionVariable", "SleepConditionVariableSRW",
        // Stage 54.5 "Otras primitivas de sincronizacion" -- Semaphore/Event's Win32 wrappers
        // import these directly (SemaphoreWait/EventWait/SemaphoreClose/EventClose reuse
        // WaitForSingleObject/CloseHandle above).
        "CreateSemaphoreA", "ReleaseSemaphore", "CreateEventA", "SetEvent", "ResetEvent",
        // Stage 55.1 "Process" -- Process's Win32 wrappers import these directly.
        "GetCurrentProcessId", "GetCurrentProcess", "K32GetProcessMemoryInfo", "GetProcessTimes",
        "GetPriorityClass", "SetPriorityClass",
        // Stage 55.2 "Hardware / System Info" -- SystemInfo's Win32 wrappers import these directly.
        "GetSystemInfo", "GlobalMemoryStatusEx",
        // Stage 55.3 "Diagnostics" -- Debug's Win32 wrappers import these directly.
        "OutputDebugStringA", "IsDebuggerPresent", "RtlCaptureStackBackTrace",
        // Pre-Stage-56 "utilidades basicas" round -- Thread.SleepRaw/File.CopyRaw/MoveRaw/
        // Directory.MoveRaw's Win32 wrappers import these directly.
        "Sleep", "CopyFileA", "MoveFileA"};
    const std::array<std::string_view, 1> kernelbaseSymbols = {"__chkstk"};
    const std::array<std::string_view, 1> shell32Symbols = {"CommandLineToArgvW"};
    const std::array<std::string_view, 5> ucrtSymbols = {
        "strlen", "__C_specific_handler", "memset", "memcpy", "memmove"};
    const std::filesystem::path kernel32 = directory / "kernel32.lib";
    const std::filesystem::path kernelbase = directory / "kernelbase.lib";
    const std::filesystem::path shell32 = directory / "shell32.lib";
    const std::filesystem::path ucrtbase = directory / "ucrtbase.lib";
    if (!linker.createImportLibrary("KERNEL32.dll", kernel32Symbols, kernel32, error) ||
        !linker.createImportLibrary("KERNELBASE.dll", kernelbaseSymbols, kernelbase, error) ||
        !linker.createImportLibrary("SHELL32.dll", shell32Symbols, shell32, error) ||
        !linker.createImportLibrary("ucrtbase.dll", ucrtSymbols, ucrtbase, error))
    {
        return false;
    }
    imports = {kernel32, kernelbase, shell32, ucrtbase};
    return true;
}

std::string clangCCommand(
    const std::filesystem::path& includeDirectory,
    const std::filesystem::path& source,
    const std::filesystem::path& object)
{
    return "clang --target=x86_64-pc-windows-msvc -fms-extensions -fdeclspec "
        "-ffreestanding -fno-stack-protector -std=c11 -Wall -Wextra -Werror -Wpedantic -c -I" +
        quote(includeDirectory) + " " + quote(source) + " -o " + quote(object);
}

void testGeneratedContract(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path shared = root / "Contract" / "Shared";
    writeProject(shared, "SharedLibrary", apiSource());

    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            shared / "Stage48_3HostApi.nsproj",
            shared / "Stage48_3HostApi.dll",
            output,
            error) == ExitCode::Success,
        "Stage 48.3 builds the C-host DLL product");
    tests.expect(error.empty(), "valid Stage 48.3 C-host product keeps stderr empty");
    tests.expect(std::filesystem::is_regular_file(shared / "Stage48_3HostApi.h") &&
            std::filesystem::is_regular_file(shared / "Stage48_3HostApi.lib") &&
            std::filesystem::is_regular_file(shared / "Stage48_3HostApi.nsmeta"),
        "C host product carries generated header, import library and NativeSharp metadata sidecars");

    const std::string header = readFile(shared / "Stage48_3HostApi.h");
    tests.expect(header.find("typedef int32_t (NS_CALL *NS_BufferCallback)(int32_t* values, int32_t count);") !=
            std::string::npos,
        "generated C contract expresses pointer+count buffer callbacks without marshaling");
    tests.expect(header.find("NS_API int32_t* NS_CALL NS48_EchoIntPointer(int32_t* values);") !=
            std::string::npos,
        "generated C contract exposes typed raw pointers directly");
    tests.expect(header.find("typedef struct NS_Counter NS_Counter;") != std::string::npos &&
            header.find("NS_API NS_Counter* NS_CALL NS48_Counter_Create") != std::string::npos &&
            header.find("NS_API void NS_CALL NS48_Counter_Destroy(NS_Counter* counter)") != std::string::npos,
        "C ownership pattern uses an opaque handle plus explicit create/destroy API");
    tests.expect(header.find("NS_STATIC_ASSERT(sizeof(NS_LayoutProbe) == 8") != std::string::npos &&
            header.find("offsetof(NS_LayoutProbe, Value) == 4") != std::string::npos,
        "C host header carries executable layout guards for ABI-visible POD structs");
    tests.expect(header.find("No hidden marshaling, retention or GC occurs at this boundary") != std::string::npos,
        "generated C contract states the zero-hidden-marshaling boundary rule");
}

void testSharedLibraryCHost(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "SharedHost";
    writeProject(directory, "SharedLibrary", apiSource());
    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            directory / "Stage48_3HostApi.nsproj",
            directory / "Stage48_3HostApi.dll",
            output,
            error) == ExitCode::Success,
        "Stage 48.3 emits the NativeSharp DLL used by the C host");

    writeFile(directory / "host.c", cHostSource());
    tests.expect(runCommand(
            clangCCommand(directory, directory / "host.c", directory / "host.obj"),
            directory / "clang_host.log"),
        "Clang C11 compiles a strict Windows x64 host using only the generated header");

    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    tests.expect(linker != nullptr, "Stage 48.3 discovers lld-link for the C host certification");
    if (linker == nullptr) return;

    tests.expect(runCommand(
            quote(linker->executablePath()) +
                " /entry:main /subsystem:console /nodefaultlib /machine:x64 /out:" +
                quote(directory / "host_shared.exe") + " " + quote(directory / "host.obj") + " " +
                quote(directory / "Stage48_3HostApi.lib"),
            directory / "link_host.log"),
        "C11 host links into a native executable through the NativeSharp DLL import library");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verify(directory / "host_shared.exe"),
        "shared-library C host is a structurally valid PE32+ executable");
    if (!verifier.verify(directory / "host_shared.exe")) return;
    const auto& info = verifier.info();
    tests.expect(info.machine == 0x8664 && info.optionalHeaderMagic == 0x020b && info.entryPointRva != 0,
        "shared C host is AMD64 PE32+ with a concrete entry point");
    tests.expect(info.subsystem == 3 && info.clrDirectoryRva == 0,
        "shared C host is a native Windows console image with no CLR header");
    tests.expect(contains(info.importedLibraries, "Stage48_3HostApi.dll"),
        "shared C host imports the NativeSharp DLL by product name");

    const std::array<std::string_view, 14> expectedSymbols = {
        "NS48_Add", "NS48_EchoIntPointer", "NS48_InvokeBuffer", "NS48_EchoPointer",
        "NS48_EchoPair", "NS48_EchoMode", "NS48_EchoLayout", "NS48_Invoke",
        "NS48_SetCallback", "NS48_CallCallback", "NS48_Counter_Create", "NS48_Counter_Get",
        "NS48_Counter_Add", "NS48_Counter_Destroy"};
    for (const std::string_view symbol : expectedSymbols)
    {
        tests.expect(importsSymbol(info, "Stage48_3HostApi.dll", symbol),
            std::string("shared C host imports exact C ABI symbol '") + std::string(symbol) + "'");
    }
}

void testStaticLibraryCHost(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "StaticHost";
    writeProject(directory, "StaticLibrary", apiSource());
    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-lib",
            directory / "Stage48_3HostApi.nsproj",
            directory / "Stage48_3HostApi.lib",
            output,
            error) == ExitCode::Success,
        "Stage 48.3 emits the NativeSharp static library used by the C host");

    const std::string header = readFile(directory / "Stage48_3HostApi.h");
    tests.expect(header.find("#define NS_API\n") != std::string::npos &&
            header.find("__declspec(dllimport)") == std::string::npos,
        "static C host contract has no DLL import decoration");

    writeFile(directory / "host.c", cHostSource());
    tests.expect(runCommand(
            clangCCommand(directory, directory / "host.c", directory / "host.obj"),
            directory / "clang_host.log"),
        "Clang C11 compiles the same host source against the generated static-library header");

    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    tests.expect(linker != nullptr, "Stage 48.3 discovers lld-link for static C host certification");
    if (linker == nullptr) return;

    std::vector<std::filesystem::path> imports;
    tests.expect(createRuntimeImports(*linker, directory, imports, error),
        "Stage 48.3 materializes the Windows runtime import surface for static host linkage");
    if (imports.size() != 4) return;

    const std::string dependencies = " " + quote(directory / "Stage48_3HostApi.lib") + " " +
        quote(imports[0]) + " " + quote(imports[1]) + " " + quote(imports[2]) + " " + quote(imports[3]);
    tests.expect(runCommand(
            quote(linker->executablePath()) +
                " /entry:main /subsystem:console /nodefaultlib /machine:x64 /out:" +
                quote(directory / "host_static.exe") + " " + quote(directory / "host.obj") + dependencies,
            directory / "link_host.log"),
        "C11 host links directly against the NativeSharp static library and runtime imports");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verify(directory / "host_static.exe"),
        "static-library C host is a structurally valid PE32+ executable");
    if (!verifier.verify(directory / "host_static.exe")) return;
    const auto& info = verifier.info();
    tests.expect(info.machine == 0x8664 && info.optionalHeaderMagic == 0x020b && info.entryPointRva != 0,
        "static C host is AMD64 PE32+ with a concrete entry point");
    tests.expect(info.subsystem == 3 && info.clrDirectoryRva == 0,
        "static C host remains a native Windows console image with no CLR header");
    tests.expect(!contains(info.importedLibraries, "Stage48_3HostApi.dll"),
        "static C host has no dependency on the NativeSharp DLL product");
    tests.expect(contains(info.importedLibraries, "KERNEL32.dll") &&
            contains(info.importedLibraries, "KERNELBASE.dll"),
        "static C host imports only the native Windows runtime dependencies it actually needs");
    tests.expect(info.exceptionDirectoryRva != 0,
        "static C host retains NativeSharp Win64 unwind metadata after direct library linkage");
}

void testCAbiMisuseGuards(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "Misuse";
    writeProject(directory, "SharedLibrary", apiSource());
    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            directory / "Stage48_3HostApi.nsproj",
            directory / "Stage48_3HostApi.dll",
            output,
            error) == ExitCode::Success,
        "Stage 48.3 creates the generated contract used by negative C ABI tests");

    writeFile(directory / "bad_pack.c", R"C(
#pragma pack(push, 1)
#include "Stage48_3HostApi.h"
#pragma pack(pop)
int main(void) { return 0; }
)C");
    tests.expect(!runCommand(
            clangCCommand(directory, directory / "bad_pack.c", directory / "bad_pack.obj"),
            directory / "bad_pack.log"),
        "generated POD assertions reject a C host that changes ABI packing");
    const std::string packLog = readFile(directory / "bad_pack.log");
    tests.expect(packLog.find("static assertion failed") != std::string::npos ||
            packLog.find("Static_assert") != std::string::npos,
        "packing misuse fails specifically through the generated compile-time layout contract");

    writeFile(directory / "bad_opaque.c", R"C(
#include "Stage48_3HostApi.h"
int bad(NS_Counter* counter) { return counter->Value; }
)C");
    tests.expect(!runCommand(
            clangCCommand(directory, directory / "bad_opaque.c", directory / "bad_opaque.obj"),
            directory / "bad_opaque.log"),
        "C host cannot dereference a NativeSharp opaque class handle");
    const std::string opaqueLog = readFile(directory / "bad_opaque.log");
    tests.expect(opaqueLog.find("incomplete") != std::string::npos,
        "opaque-handle misuse is rejected because the generated C type is intentionally incomplete");

    writeFile(directory / "bad_callback.c", R"C(
#include "Stage48_3HostApi.h"
static int32_t Wrong(int32_t* value) { return value == 0 ? 0 : *value; }
void bad(void) { NS48_SetCallback(Wrong); }
)C");
    tests.expect(!runCommand(
            clangCCommand(directory, directory / "bad_callback.c", directory / "bad_callback.obj"),
            directory / "bad_callback.log"),
        "strict C11 host build rejects a callback whose function-pointer signature differs from the generated typedef");
    const std::string callbackLog = readFile(directory / "bad_callback.log");
    tests.expect(callbackLog.find("incompatible function pointer") != std::string::npos ||
            callbackLog.find("incompatible-pointer-types") != std::string::npos,
        "callback misuse diagnostic exposes the exact C function-pointer mismatch");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage48_3_c_host_integration";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root, cleanupError);

    testGeneratedContract(tests, root);
    testSharedLibraryCHost(tests, root);
    testStaticLibraryCHost(tests, root);
    testCAbiMisuseGuards(tests, root);

    return tests.resultKeeping(root);
}
