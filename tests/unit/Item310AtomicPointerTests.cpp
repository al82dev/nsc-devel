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

constexpr std::string_view atomicPointerDeclaration = R"NS(
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

    public class AtomicPointer
    {
        private void* value_;

        public AtomicPointer(void* initialValue) { value_ = initialValue; }
        public void* Load() { return null; }
        public void* Load(MemoryOrder order) { return null; }
        public void Store(void* value) { }
        public void Store(void* value, MemoryOrder order) { }
        public void* Exchange(void* value) { return null; }
        public void* Exchange(void* value, MemoryOrder order) { return null; }
        public void* CompareExchange(void* value, void* comparand) { return null; }
        public void* CompareExchange(
            void* value,
            void* comparand,
            MemoryOrder successOrder,
            MemoryOrder failureOrder) { return null; }
    }
}
)NS";

void writeProject(const std::filesystem::path& root, std::string_view program)
{
    writeFile(root / "AtomicPointer.nsproj",
        "<Project><PropertyGroup><Name>AtomicPointerItem310</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Atomics.ns", atomicPointerDeclaration);
    writeFile(root / "Main.ns", program);
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item310_atomic_pointer_tests";
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
        int* slots = stackalloc int[3];
        void* first = slots;
        void* second = slots + 1;
        void* third = slots + 2;
        raii AtomicPointer value = new AtomicPointer(first);

        if (value.Load() != first) { return 1; }
        value.Store(second, MemoryOrder.Release);
        if (value.Load(MemoryOrder.Acquire) != second) { return 2; }
        if (value.Exchange(third, MemoryOrder.AcquireRelease) != second) { return 3; }
        if (value.CompareExchange(
                first, third, MemoryOrder.SequentiallyConsistent, MemoryOrder.Acquire) != third)
        {
            return 4;
        }
        if (value.CompareExchange(
                second, third, MemoryOrder.Acquire, MemoryOrder.Relaxed) != first)
        {
            return 5;
        }
        if (value.Exchange(null) != first) { return 6; }
        if (value.Load(MemoryOrder.Relaxed) != null) { return 7; }
        value.Store(first);
        if (value.CompareExchange(second, first) != first) { return 8; }
        value.Store(third, MemoryOrder.SequentiallyConsistent);
        return value.Load(MemoryOrder.SequentiallyConsistent) == third ? 0 : 9;
    }
}
)NS");

    const std::string project = (validRoot / "AtomicPointer.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 3> dumpArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    const bool dumped = runDriver(dumpArguments, output, error) == ExitCode::Success;
    tests.expect(dumped, "AtomicPointer fixture emits O0 LLVM (" + error + ")");
    if (dumped)
    {
        writeFile(validRoot / "AtomicPointer.ll", output);
        tests.expect(output.find("load atomic ptr") != std::string::npos,
            "AtomicPointer.Load emits a pointer-width atomic load");
        tests.expect(output.find("store atomic ptr") != std::string::npos,
            "AtomicPointer.Store emits a pointer-width atomic store");
        tests.expect(count(output, "atomicrmw xchg ptr") >= 2U,
            "legacy and ordered AtomicPointer.Exchange emit pointer xchg");
        tests.expect(count(output, "cmpxchg ptr") >= 2U,
            "legacy and ordered AtomicPointer.CompareExchange emit pointer cmpxchg");
        tests.expect(count(output, "align 8") >= 8U,
            "all pointer atomics carry natural Windows x64 alignment");
        tests.expect(output.find("__atomic_") == std::string::npos &&
                output.find("InterlockedExchangePointer") == std::string::npos,
            "AtomicPointer requires no external atomic helper or Win32 import");
    }

    const std::string executable = (validRoot / "AtomicPointer.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "AtomicPointer fixture links at O0 (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "AtomicPointer preserves pointer identity, null, exchange, and CAS contracts");
    }

    const auto invalidRoot = root / "invalid";
    writeProject(invalidRoot, R"NS(
using NativeSharp.Core.Threading;

public class Program
{
    public static int Main()
    {
        raii AtomicPointer value = new AtomicPointer(null);
        MemoryOrder dynamicOrder = MemoryOrder.Relaxed;
        value.Load(MemoryOrder.Release);
        value.Store(null, MemoryOrder.Acquire);
        value.Exchange(null, dynamicOrder);
        value.CompareExchange(
            null, null, MemoryOrder.Relaxed, MemoryOrder.Acquire);
        return 0;
    }
}
)NS");
    const std::string invalidProject = (invalidRoot / "AtomicPointer.nsproj").string();
    const std::array<std::string_view, 3> invalidArguments = {
        "--dump-project-oop-hir", invalidProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(runDriver(invalidArguments, output, error) != ExitCode::Success,
        "AtomicPointer rejects illegal or runtime-selected memory orders");
    tests.expect(error.find(
            "atomic Load order must be Relaxed, Acquire, or SequentiallyConsistent") != std::string::npos,
        "AtomicPointer.Load rejects Release");
    tests.expect(error.find(
            "atomic Store order must be Relaxed, Release, or SequentiallyConsistent") != std::string::npos,
        "AtomicPointer.Store rejects Acquire");
    tests.expect(error.find(
            "atomic memory order must be a compile-time MemoryOrder constant") != std::string::npos,
        "AtomicPointer.Exchange rejects a runtime order");
    tests.expect(error.find(
            "atomic compare-exchange failure order cannot be stronger than success order") != std::string::npos,
        "AtomicPointer.CompareExchange enforces success/failure ordering");

    return tests.resultKeeping(root);
}
