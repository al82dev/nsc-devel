#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode code = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return code;
}

std::size_t count(std::string_view text, std::string_view needle)
{
    std::size_t result = 0;
    std::size_t offset = 0;
    while ((offset = text.find(needle, offset)) != std::string_view::npos)
    {
        ++result;
        offset += needle.size();
    }
    return result;
}

constexpr std::string_view interlockedDeclaration = R"NS(
namespace NativeSharp.Core.Threading
{
    public enum MemoryOrder
    {
        Relaxed = 0,
        Acquire = 1,
        Release = 2,
        AcquireRelease = 3,
        SequentiallyConsistent = 4
    }

    public static class Interlocked
    {
        public static int Load(ref int location) { return 0; }
        public static int Load(ref int location, MemoryOrder order) { return 0; }
        public static void Store(ref int location, int value) { }
        public static void Store(ref int location, int value, MemoryOrder order) { }
        public static int Increment(ref int location) { return 0; }
        public static int Increment(ref int location, MemoryOrder order) { return 0; }
        public static int Decrement(ref int location) { return 0; }
        public static int Decrement(ref int location, MemoryOrder order) { return 0; }
        public static int Add(ref int location, int value) { return 0; }
        public static int Add(ref int location, int value, MemoryOrder order) { return 0; }
        public static int Exchange(ref int location, int value) { return 0; }
        public static int Exchange(ref int location, int value, MemoryOrder order) { return 0; }
        public static int CompareExchange(ref int location, int value, int comparand) { return 0; }
        public static int CompareExchange(
            ref int location, int value, int comparand,
            MemoryOrder successOrder, MemoryOrder failureOrder) { return 0; }

        public static long Load(ref long location) { return 0; }
        public static long Load(ref long location, MemoryOrder order) { return 0; }
        public static void Store(ref long location, long value) { }
        public static void Store(ref long location, long value, MemoryOrder order) { }
        public static long Increment(ref long location) { return 0; }
        public static long Increment(ref long location, MemoryOrder order) { return 0; }
        public static long Decrement(ref long location) { return 0; }
        public static long Decrement(ref long location, MemoryOrder order) { return 0; }
        public static long Add(ref long location, long value) { return 0; }
        public static long Add(ref long location, long value, MemoryOrder order) { return 0; }
        public static long Exchange(ref long location, long value) { return 0; }
        public static long Exchange(ref long location, long value, MemoryOrder order) { return 0; }
        public static long CompareExchange(ref long location, long value, long comparand) { return 0; }
        public static long CompareExchange(
            ref long location, long value, long comparand,
            MemoryOrder successOrder, MemoryOrder failureOrder) { return 0; }

        public static void* Load(ref void* location) { return null; }
        public static void* Load(ref void* location, MemoryOrder order) { return null; }
        public static void Store(ref void* location, void* value) { }
        public static void Store(ref void* location, void* value, MemoryOrder order) { }
        public static void* Exchange(ref void* location, void* value) { return null; }
        public static void* Exchange(ref void* location, void* value, MemoryOrder order) { return null; }
        public static void* CompareExchange(ref void* location, void* value, void* comparand) { return null; }
        public static void* CompareExchange(
            ref void* location, void* value, void* comparand,
            MemoryOrder successOrder, MemoryOrder failureOrder) { return null; }
    }
}
)NS";

void writeProject(const std::filesystem::path& root, std::string_view program)
{
    writeFile(root / "Interlocked.nsproj",
        "<Project><PropertyGroup><Name>InterlockedItem311</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Interlocked.ns", interlockedDeclaration);
    writeFile(root / "Main.ns", program);
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item311_interlocked_api_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto validRoot = root / "valid";
    writeProject(validRoot, R"NS(
using NativeSharp.Core.Threading;

public class Program
{
    public static int Main()
    {
        int value = 10;
        if (Interlocked.Load(ref value) != 10) { return 1; }
        Interlocked.Store(ref value, 11, MemoryOrder.Release);
        if (Interlocked.Increment(ref value) != 12) { return 2; }
        if (Interlocked.Decrement(ref value, MemoryOrder.AcquireRelease) != 11) { return 3; }
        if (Interlocked.Add(ref value, 5, MemoryOrder.Relaxed) != 16) { return 4; }
        if (Interlocked.Exchange(ref value, 20) != 16) { return 5; }
        if (Interlocked.CompareExchange(
                ref value, 30, 20, MemoryOrder.SequentiallyConsistent, MemoryOrder.Acquire) != 20)
        {
            return 6;
        }
        if (Interlocked.CompareExchange(ref value, 40, 20) != 30 || value != 30) { return 7; }

        long wide = 100;
        Interlocked.Store(ref wide, 101);
        if (Interlocked.Increment(ref wide, MemoryOrder.AcquireRelease) != 102) { return 8; }
        if (Interlocked.Decrement(ref wide) != 101) { return 9; }
        if (Interlocked.Add(ref wide, 9, MemoryOrder.Relaxed) != 110) { return 10; }
        if (Interlocked.Exchange(ref wide, 200, MemoryOrder.AcquireRelease) != 110) { return 11; }
        if (Interlocked.CompareExchange(
                ref wide, 300, 200, MemoryOrder.SequentiallyConsistent, MemoryOrder.Relaxed) != 200)
        {
            return 12;
        }
        if (Interlocked.Load(ref wide, MemoryOrder.Acquire) != 300) { return 13; }

        int* slots = stackalloc int[3];
        void* first = slots;
        void* second = slots + 1;
        void* third = slots + 2;
        void* pointer = first;
        if (Interlocked.Load(ref pointer) != first) { return 14; }
        Interlocked.Store(ref pointer, second, MemoryOrder.Release);
        if (Interlocked.Exchange(ref pointer, third, MemoryOrder.AcquireRelease) != second) { return 15; }
        if (Interlocked.CompareExchange(
                ref pointer, first, third,
                MemoryOrder.SequentiallyConsistent, MemoryOrder.Acquire) != third)
        {
            return 16;
        }
        if (Interlocked.CompareExchange(ref pointer, second, third) != first) { return 17; }
        Interlocked.Store(ref pointer, null);
        return Interlocked.Load(ref pointer, MemoryOrder.Relaxed) == null ? 0 : 18;
    }
}
)NS");

    const std::string project = (validRoot / "Interlocked.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 3> dumpArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    const bool dumped = runDriver(dumpArguments, output, error) == ExitCode::Success;
    tests.expect(dumped, "Interlocked fixture emits O0 LLVM (" + error + ")");
    if (dumped)
    {
        writeFile(validRoot / "Interlocked.ll", output);
        tests.expect(output.find("__nativesharp_atomic_load_int32") != std::string::npos &&
                output.find("store atomic i32 ") != std::string::npos,
            "Interlocked ref int emits atomic i32 loads and stores");
        tests.expect(output.find("load atomic i64,") != std::string::npos &&
                output.find("__nativesharp_atomic_store_int64") != std::string::npos,
            "Interlocked ref long emits atomic i64 loads and stores");
        tests.expect(output.find("load atomic ptr") != std::string::npos &&
                output.find("store atomic ptr") != std::string::npos,
            "Interlocked ref void* remains pointer-typed");
        tests.expect(count(output, "atomicrmw add ptr") >= 4U,
            "ordered integer increment, decrement, and add share native atomic RMW lowering");
        tests.expect(count(output, "atomicrmw xchg ptr") >= 2U,
            "integer and pointer exchange use native atomic RMW lowering");
        tests.expect(count(output, "cmpxchg ptr") >= 4U,
            "integer and pointer compare-exchange use native cmpxchg lowering");
        tests.expect(output.find("__atomic_") == std::string::npos &&
                output.find("InterlockedExchangePointer") == std::string::npos,
            "Interlocked introduces no external atomic helper");
    }

    const std::string executable = (validRoot / "Interlocked.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "Interlocked fixture links at O0 (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "Interlocked preserves int, long, pointer, return-value, null, and CAS contracts");
    }

    const auto invalidRoot = root / "invalid";
    writeProject(invalidRoot, R"NS(
using NativeSharp.Core.Threading;

public class Program
{
    public static int Main()
    {
        int value = 0;
        MemoryOrder dynamicOrder = MemoryOrder.Relaxed;
        Interlocked.Load(ref value, MemoryOrder.Release);
        Interlocked.Store(ref value, 1, MemoryOrder.Acquire);
        Interlocked.Exchange(ref value, 2, dynamicOrder);
        Interlocked.CompareExchange(
            ref value, 3, 2, MemoryOrder.Relaxed, MemoryOrder.Acquire);
        Interlocked.Increment(value);
        return 0;
    }
}
)NS");
    const std::string invalidProject = (invalidRoot / "Interlocked.nsproj").string();
    const std::array<std::string_view, 3> invalidArguments = {
        "--dump-project-oop-hir", invalidProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(runDriver(invalidArguments, output, error) != ExitCode::Success,
        "Interlocked rejects invalid ref usage and memory orders");
    tests.expect(error.find("atomic Load order must be Relaxed, Acquire, or SequentiallyConsistent") !=
            std::string::npos,
        "Interlocked.Load rejects Release");
    tests.expect(error.find("atomic Store order must be Relaxed, Release, or SequentiallyConsistent") !=
            std::string::npos,
        "Interlocked.Store rejects Acquire");
    tests.expect(error.find("atomic memory order must be a compile-time MemoryOrder constant") !=
            std::string::npos,
        "Interlocked.Exchange rejects a runtime order");
    tests.expect(error.find(
            "atomic compare-exchange failure order cannot be stronger than success order") !=
            std::string::npos,
        "Interlocked.CompareExchange validates independent orders");
    tests.expect(error.find("no viable overload of 'Increment' accepts the argument types") != std::string::npos,
        "Interlocked rejects a location not explicitly passed by ref");

    return tests.resultKeeping(root);
}
