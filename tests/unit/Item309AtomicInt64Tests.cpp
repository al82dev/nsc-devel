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
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item309_atomic_int64_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    writeFile(root / "AtomicInt64.nsproj",
        "<Project><PropertyGroup><Name>AtomicInt64Item309</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Threading;

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

    public class AtomicInt64
    {
        private long value_;

        public AtomicInt64(long initialValue) { value_ = initialValue; }
        public long Load() { return 0; }
        public long Load(MemoryOrder order) { return 0; }
        public void Store(long value) { }
        public void Store(long value, MemoryOrder order) { }
        public long Increment() { return 0; }
        public long Increment(MemoryOrder order) { return 0; }
        public long Decrement() { return 0; }
        public long Decrement(MemoryOrder order) { return 0; }
        public long Add(long value) { return 0; }
        public long Add(long value, MemoryOrder order) { return 0; }
        public long Exchange(long value) { return 0; }
        public long Exchange(long value, MemoryOrder order) { return 0; }
        public long CompareExchange(long value, long comparand) { return 0; }
        public long CompareExchange(
            long value,
            long comparand,
            MemoryOrder successOrder,
            MemoryOrder failureOrder) { return 0; }
    }
}

public class Program
{
    public static int Main()
    {
        AtomicInt64 value = new AtomicInt64(9223372036854775807l);
        if (value.Increment() != -9223372036854775808l) { return 1; }
        if (value.Load() != -9223372036854775808l) { return 2; }
        if (value.Decrement(MemoryOrder.SequentiallyConsistent) != 9223372036854775807l) { return 3; }
        if (value.Add(2l, MemoryOrder.Relaxed) != -9223372036854775807l) { return 4; }
        if (value.Exchange(-7l, MemoryOrder.AcquireRelease) != -9223372036854775807l) { return 5; }
        if (value.CompareExchange(
                42l, -7l, MemoryOrder.SequentiallyConsistent, MemoryOrder.Acquire) != -7l) { return 6; }
        if (value.CompareExchange(
                9l, -7l, MemoryOrder.Acquire, MemoryOrder.Relaxed) != 42l) { return 7; }
        value.Store(-9223372036854775808l, MemoryOrder.Release);
        if (value.Load(MemoryOrder.Acquire) != -9223372036854775808l) { return 8; }
        if (value.Increment(MemoryOrder.Acquire) != -9223372036854775807l) { return 9; }
        value.Store(11l);
        if (value.Add(-12l) != -1l) { return 10; }
        if (value.Exchange(5l) != -1l) { return 11; }
        if (value.CompareExchange(6l, 5l) != 5l) { return 12; }
        return value.Load(MemoryOrder.SequentiallyConsistent) == 6l ? 0 : 13;
    }
}
)NS");

    const std::string project = (root / "AtomicInt64.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 3> dumpArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    const bool dumped = runDriver(dumpArguments, output, error) == ExitCode::Success;
    tests.expect(dumped, "AtomicInt64 boundary fixture emits O0 LLVM (" + error + ")");
    if (dumped)
    {
        writeFile(root / "AtomicInt64.ll", output);
        tests.expect(output.find("load atomic i64") != std::string::npos &&
                output.find("store atomic i64") != std::string::npos,
            "ordered load/store use native i64 atomic instructions");
        tests.expect(count(output, "atomicrmw add ptr") >= 3U,
            "increment, decrement, and add use native atomicrmw add");
        tests.expect(output.find("atomicrmw xchg ptr") != std::string::npos,
            "exchange uses a native i64 atomicrmw xchg");
        tests.expect(output.find("cmpxchg ptr") != std::string::npos,
            "compare-exchange uses native i64 cmpxchg");
        tests.expect(count(output, "align 8") >= 7U,
            "64-bit atomic instructions carry natural eight-byte alignment");
        tests.expect(output.find("__atomic_") == std::string::npos,
            "Windows x64 AtomicInt64 requires no external libatomic helper");
        tests.expect(output.find("i32 9223372036854775807") == std::string::npos,
            "64-bit boundary constants are never truncated to i32");
    }

    const std::string executable = (root / "AtomicInt64.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "AtomicInt64 boundary fixture links at O0 (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "AtomicInt64 preserves full-width values and return contracts at runtime");
    }

    return tests.resultKeeping(root);
}
