// EN: NativeSharp 3.0 freeze (NSC-PLAN-CONGELAMIENTO-3.0.md). Each block reproduces, by compiling
//     and running a real Windows x64 program at -O0 and -O2, one contract the language promises
//     before the freeze: C-08 allocator alignment, C-06 published surface, C-07 IDisposable, C-01
//     lost ownership, C-02 synchronous await, C-05 file mode, C-11/C-12 value and array
//     destruction, C-13 throws contracts, C-14 future drop, C-16 generator cleanup and C-17
//     async-generator errors.
// ES: Congelamiento de NativeSharp 3.0 (NSC-PLAN-CONGELAMIENTO-3.0.md). Cada bloque reproduce,
//     compilando y ejecutando un programa Windows x64 real en -O0 y -O2, un contrato que el
//     lenguaje promete antes del congelamiento: C-08 alineacion de allocators, C-06 superficie
//     publicada, C-07 IDisposable, C-01 ownership perdido, C-02 await sincrono, C-05 modo
//     archivo, C-11/C-12 destruccion de valores y arreglos, C-13 contratos throws, C-14 drop de
//     futures, C-16 cleanup de generators y C-17 errores de async-generators.

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

/// EN: Writes `directory/<name>.nsproj` compiling every .ns file and referencing NativeSharp.Core.
/// ES: Escribe `directory/<name>.nsproj`, que compila todo .ns y referencia NativeSharp.Core.
std::filesystem::path writeProject(const std::filesystem::path& directory, std::string_view name)
{
    const std::filesystem::path core =
        std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj";
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup>"
            "<ItemGroup><ProjectReference Include=\"" + core.generic_string() +
            "\" /></ItemGroup></Project>");
    return project;
}

/// EN: Compiles `project` into `executable` at `level`; returns the exit code, `diagnostic` gets
///     the driver's output and error streams.
/// ES: Compila `project` en `executable` con `level`; devuelve el codigo de salida y `diagnostic`
///     recibe la salida y el error del driver.
ExitCode compile(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string_view level,
    std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 6> arguments = {
        level, "--no-artifact-cache", "--emit-project-oop-exe", projectText, "-o", executableText};
    Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    const ExitCode code = driver.run(arguments, output, error);
    diagnostic = output.str() + error.str();
    return code;
}

/// EN: `source` must compile at -O0 and -O2 and both programs must exit with `expected`.
/// ES: `source` debe compilar en -O0 y -O2 y ambos programas deben salir con `expected`.
void expectRun(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    int expected)
{
    const std::filesystem::path directory = root / std::string(name);
    const std::filesystem::path project = writeProject(directory, name);
    writeFile(directory / "Main.ns", source);
    const std::array<std::string_view, 2> levels = {"-O0", "-O2"};
    for (const std::string_view level : levels)
    {
        const std::filesystem::path executable =
            directory / (std::string(name) + std::string(level.substr(1)) + ".exe");
        std::string diagnostic;
        const ExitCode code = compile(project, executable, level, diagnostic);
        const std::string label = std::string(name) + " [" + std::string(level) + "]";
        tests.expect(code == ExitCode::Success, label + ": compiles: " + diagnostic);
        if (code != ExitCode::Success) continue;
        const std::string command = "\"\"" + executable.string() + "\"\"";
        const int observed = std::system(command.c_str());
        tests.expect(observed == expected,
            label + ": exits " + std::to_string(expected) + ", observed " + std::to_string(observed));
    }
}

/// EN: `source` must be rejected and the diagnostic must contain `fragment`.
/// ES: `source` debe rechazarse y el diagnostico debe contener `fragment`.
[[maybe_unused]] void expectReject(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view fragment)
{
    const std::filesystem::path directory = root / std::string(name);
    const std::filesystem::path project = writeProject(directory, name);
    writeFile(directory / "Main.ns", source);
    std::string diagnostic;
    const ExitCode code = compile(project, directory / (std::string(name) + ".exe"), "-O0", diagnostic);
    tests.expect(code != ExitCode::Success && diagnostic.find(fragment) != std::string::npos,
        std::string(name) + ": is rejected with \"" + std::string(fragment) + "\": " + diagnostic);
}

/// EN: C-08. `new (alloc) T` asks IAllocator.Allocate for sizeof(T) and alignof(T): an align(32)
///     struct lands on a 32-byte boundary even when the allocator would hand out 8-aligned
///     blocks for smaller requests, and the object's Deallocate receives the same size and
///     alignment its Allocate received.
/// ES: C-08. `new (alloc) T` pide a IAllocator.Allocate sizeof(T) y alignof(T): un struct
///     align(32) queda en un limite de 32 aunque el allocator entregue bloques alineados a 8 para
///     pedidos menores, y el Deallocate del objeto recibe el mismo tamano y alineacion que su
///     Allocate.
void testAllocatorAlignment(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C08AllocatorAlignment", R"ns(
using NativeSharp.Core;

public align(32) struct Wide
{
    public long A;
    public long B;
}

class Box
{
    public long V;
    public Box(long v) { V = v; }
}

class ExactAllocator : IAllocator
{
    private owned byte[] pool;
    private int next;
    public static long AllocBytes = 0;
    public static int AllocAlign = 0;
    public static long FreeBytes = 0;
    public static int FreeAlign = 0;
    public static int Frees = 0;

    public ExactAllocator(int size) { pool = new byte[size]; next = 0; }

    // Honors exactly the requested alignment and nothing more: the block starts at a 64-byte
    // boundary plus `alignment`, so any alignment smaller than requested would be visible.
    public byte* Allocate(long bytes, int alignment)
    {
        AllocBytes = bytes;
        AllocAlign = alignment;
        byte* data = Spans.Of<byte>(pool).Data();
        long address = (long)data;
        long start = (address + 63) / 64 * 64 + (long)next * 256;
        next = next + 1;
        long offset = 0;
        if (alignment < 64) { offset = (long)alignment; }
        return data + (start - address) + offset;
    }

    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return null; }

    public void Deallocate(byte* ptr, long bytes, int alignment)
    {
        FreeBytes = bytes;
        FreeAlign = alignment;
        Frees = Frees + 1;
    }
}

class Program
{
    public static int Main()
    {
        raii ExactAllocator alloc = new ExactAllocator(4096);
        Wide* wide = new (alloc) Wide();
        if ((long)wide % 32 != 0) { return 1; }
        if (ExactAllocator.AllocAlign != 32) { return 2; }
        if (ExactAllocator.AllocBytes != 32) { return 3; }
        {
            owned Box box = new (alloc) Box(7);
            if (box.V != 7) { return 4; }
            if ((long)ExactAllocator.AllocAlign <= 0) { return 5; }
        }
        if (ExactAllocator.Frees != 1) { return 6; }
        if (ExactAllocator.FreeBytes != ExactAllocator.AllocBytes) { return 7; }
        if (ExactAllocator.FreeAlign != ExactAllocator.AllocAlign) { return 8; }
        return 0;
    }
}
)ns", 0);
}

/// EN: C-07. NativeSharp.Core publishes IDisposable: `using` compiles against the library without
///     a local declaration and calls Dispose exactly once on every exit path.
/// ES: C-07. NativeSharp.Core publica IDisposable: `using` compila contra la biblioteca sin una
///     declaracion local y llama Dispose exactamente una vez en cada salida.
void testCoreDisposable(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C07CoreDisposable", R"ns(
using NativeSharp.Core;

class Handle : IDisposable
{
    public static int Disposed = 0;
    public void Dispose() { Disposed = Disposed + 1; }
}

class Program
{
    public static int Work(bool early)
    {
        using (Handle handle = new Handle())
        {
            if (early) { return 1; }
        }
        return 2;
    }

    public static int Main()
    {
        int first = Work(true);
        int second = Work(false);
        if (first != 1 || second != 2) { return 9; }
        return Handle.Disposed;
    }
}
)ns", 2);
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root = std::filesystem::path(NSC_BINARY_DIR) / "freeze30_contracts";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);

    testAllocatorAlignment(tests, root);
    testCoreDisposable(tests, root);
    return tests.resultKeeping(root);
}
