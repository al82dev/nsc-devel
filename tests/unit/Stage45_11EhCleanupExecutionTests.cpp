// Stage 45.11 (fix) -- cleanup EXECUTION suite: every scope-exit obligation the language makes on
// an exceptional path is verified by RUNNING a real Windows x64 PE, at -O0 and -O2.
//
// Why this file exists, separately from Stage45_10EhRaiiReuseAfterCatchTests.cpp:
//
// An audit of this project's own test suite (2026-08-29) found that only 27 of 112 unit-test files
// execute anything at all -- the rest assert on the TEXT of the emitted LLVM IR. That is how a P0
// bug survived indefinitely: Stage45TypedCatchRttiTests.cpp asserted "LLVM catchswitch owns exactly
// three catchpad handlers" and was green for a structure that could not work, because
// `__C_specific_handler` honours exactly one handler per catchswitch and every clause past the
// first was dead code. Nothing ever ran a program with two catch clauses to notice.
//
// The EH/cleanup assertions in Stage45LlvmCleanupFuncletTests.cpp, Stage45LlvmWin64EhBasicTests.cpp,
// Stage49RaiiMoveSemanticsTests.cpp and Stage50_8OwnershipIrLoweringTests.cpp are all of that same
// shape-only family: they pin that a `cleanuppad`/`cleanupret`/funclet bundle APPEARS in the IR,
// which says nothing about whether the cleanup body ever executes at runtime. This file supplies
// the missing half for the claims that matter, by observation rather than by inspection: each
// scenario counts, in a static field the program itself returns, how many times a destructor /
// `Dispose()` / `finally` body actually ran.
//
// Bugs this suite was written against, all found by real execution and all since fixed:
//
//  * A `finally` (or `using`/`owned`-parameter cleanup) was SILENTLY SKIPPED at -O2 whenever the
//    last instruction of its protected range was a never-returning call -- a `throw`. The Windows
//    scope table records a region's end as "last protected instruction + 1", so a catch handler
//    laid out immediately after the range starts at an address the personality still considers
//    INSIDE it; it then concludes the region is not being left and skips its termination handler.
//    Confirmed to be an LLVM-level hazard rather than a defect unique to this compiler: the same C
//    SEH nesting compiled by CLANG, with a `__declspec(noreturn)` call as the whole try body,
//    reproduces it identically under the same Wine (`cleanups=0`). The fix keeps a real call after
//    every throw and marks `__nativesharp_trap` `nounwind`, so the trap stays a plain call whose
//    bytes fall OUTSIDE the protected range and push the handler past its end -- see
//    ProjectOopLlvmLowerer's trap declaration and lowerExceptionalTransfer for the mechanism.
//  * Premature destruction of cleanup-owning locals whose scope encloses a catching try (B-006/
//    B-007), and dead catch clauses past the first -- both covered by the sibling suite.
//
// Every scenario returns a COUNT, not a flag: a count is the only observation that distinguishes
// "ran once" from "leaked" (0) and from "ran twice" (2), and those two failure modes have opposite
// causes. Asserting merely "did not crash" is what let both of them hide.

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

void writeProject(const std::filesystem::path& directory, std::string_view name)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

bool commandAvailable(std::string_view command)
{
#ifdef _WIN32
    return std::system(("where " + std::string(command) + " >nul 2>nul").c_str()) == 0;
#else
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
#endif
}

std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}

bool runUnderWine(
    const std::string& wine,
    const std::filesystem::path& exePath,
    const std::filesystem::path& logPath,
    int& exitCode,
    bool& crashed)
{
    const std::string command = "WINEDEBUG=-all " + wine + " " + exePath.string() +
        " >" + logPath.string() + " 2>&1";
    const int raw = std::system(command.c_str());
    if (raw < 0) return false;
#ifdef WIFEXITED
    exitCode = WIFEXITED(raw) ? WEXITSTATUS(raw) : (raw & 0xFF);
#else
    exitCode = raw & 0xFF;
#endif
    std::ifstream logStream(logPath, std::ios::binary);
    std::ostringstream logText;
    logText << logStream.rdbuf();
    const std::string log = logText.str();
    crashed = log.find("page fault") != std::string::npos ||
        log.find("Unhandled exception") != std::string::npos ||
        exitCode == 135;
    return true;
}

void compileAndRun(
    TestContext& tests,
    const std::filesystem::path& directory,
    const std::filesystem::path& exePath,
    std::string_view optimizationFlag,
    std::string_view label,
    int expectedExitCode)
{
    const std::string projectText = (directory / (directory.filename().string() + ".nsproj")).string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, optimizationFlag, "-o", exeText};

    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    tests.expect(result == ExitCode::Success, std::string(label) + ": compiles to a Windows x64 PE");
    tests.expect(err.str().empty(), std::string(label) + ": no driver diagnostics (" + err.str() + ")");
    if (result != ExitCode::Success) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string(label) + ": Wine not available -- execution check skipped");
        return;
    }
    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(wine, exePath, exePath.string() + ".log", exitCode, crashed);
    tests.expect(ran, std::string(label) + ": Wine could be invoked");
    tests.expect(!crashed, std::string(label) + ": does not crash under real execution");
    tests.expect(
        exitCode == expectedExitCode,
        std::string(label) + ": observed count " + std::to_string(exitCode) + ", expected " +
            std::to_string(expectedExitCode));
}

// -O0 and -O2 both, always: every bug this file guards against was optimization-level dependent,
// and in both directions -- the layout hazard only appeared at -O2, while B-001's crash only
// appeared at -O0. Checking one level would have missed one of them.
void run(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view projectName,
    std::string_view source,
    int expected)
{
    const auto directory = root / std::string(projectName);
    writeProject(directory, projectName);
    writeFile(directory / "Main.ns", source);
    compileAndRun(
        tests, directory, directory / "o0.exe", "-O0", std::string(projectName) + " [-O0]", expected);
    compileAndRun(
        tests, directory, directory / "o2.exe", "-O2", std::string(projectName) + " [-O2]", expected);
}

// Shared prelude. `Res` reports BOTH how many times it was destroyed and how many times it was
// disposed, because `raii`/`owned` cleanup and `using` cleanup are different obligations and a test
// that conflates them cannot tell which one regressed. Local NativeSharp.Core stub + top-level
// classes, the same self-contained convention every other Stage 45 EH test file uses.
constexpr std::string_view kPrelude = R"NS(
namespace NativeSharp.Core
{
    public class Exception { public string Message; }
    public interface IDisposable { void Dispose(); }
}
using NativeSharp.Core;

class Boom : Exception { public int Code; }
class Unrelated : Exception { public int Code; }

class Res : IDisposable
{
    private int value;
    private static int destructions;
    private static int disposals;
    public Res(int initial) { value = initial; }
    ~Res() { destructions = destructions + 1; }
    public void Dispose() { disposals = disposals + 1; }
    public void Fail() { Boom problem = new Boom(); problem.Code = 7; throw problem; }
    public int Value() { return value; }
    public static int Destructions() { return destructions; }
    public static int Disposals() { return disposals; }
}
)NS";

// ---------------------------------------------------------------------------------------
// `finally` reached by an exception that ESCAPES the method, where the try body's last act is a
// throw. This is the exact shape the -O2 layout hazard silently broke: the finally simply never
// ran, no crash, no diagnostic. Counting is the only way to see it.
// ---------------------------------------------------------------------------------------
void testFinallyRunsWhenExceptionEscapes(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        private static int marks;
        public static void Inner()
        {
            try { Boom problem = new Boom(); throw problem; }
            finally { marks = marks + 1; }
        }
        public static int Main()
        {
            try { Inner(); } catch (Boom problem) { }
            return marks;
        }
    }
)NS";
    run(tests, root, "FinallyRunsWhenExceptionEscapes", source, 1);
}

// ---------------------------------------------------------------------------------------
// Same, but the throw is inside a HELPER method rather than written inline. At -O2 the helper is
// inlined, which is what turned a cross-frame throw into a same-frame one and re-created the
// hazard after the inline-free case had already been fixed. Kept as its own scenario because the
// two fixes it needed were different (a trap after the throw, and marking that trap `nounwind` so
// inlining could not turn it into an invoke that extends the protected range).
// ---------------------------------------------------------------------------------------
void testFinallyRunsWhenThrowComesFromInlinedHelper(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        private static int marks;
        public static void Thrower() { Boom problem = new Boom(); throw problem; }
        public static void Inner()
        {
            try { Thrower(); }
            finally { marks = marks + 1; }
        }
        public static int Main()
        {
            try { Inner(); } catch (Boom problem) { }
            return marks;
        }
    }
)NS";
    run(tests, root, "FinallyRunsFromInlinedHelperThrow", source, 1);
}

// ---------------------------------------------------------------------------------------
// Two nested `finally` blocks, both exited by the same escaping exception: both must run, in
// innermost-first order. Encoded as 1 + 10 so a result of 10 (only the outer ran) and 1 (only the
// inner) are distinguishable from each other and from 0.
// ---------------------------------------------------------------------------------------
void testNestedFinallyBothRunWhenExceptionEscapes(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        private static int marks;
        public static void Inner()
        {
            try
            {
                try { Boom problem = new Boom(); throw problem; }
                finally { marks = marks + 1; }
            }
            finally { marks = marks + 10; }
        }
        public static int Main()
        {
            try { Inner(); } catch (Boom problem) { }
            return marks;
        }
    }
)NS";
    run(tests, root, "NestedFinallyBothRunOnEscape", source, 11);
}

// ---------------------------------------------------------------------------------------
// A `finally` nested inside a catching try, plus the catching try's own `finally`. 1 + 10 + 100
// keeps all three obligations independently observable in one exit code.
// ---------------------------------------------------------------------------------------
void testInnerFinallyOuterCatchAndOuterFinally(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        private static int marks;
        public static int Main()
        {
            try
            {
                try { Boom problem = new Boom(); throw problem; }
                finally { marks = marks + 1; }
            }
            catch (Boom problem) { marks = marks + 10; }
            finally { marks = marks + 100; }
            return marks;
        }
    }
)NS";
    run(tests, root, "InnerFinallyOuterCatchOuterFinally", source, 111);
}

// ---------------------------------------------------------------------------------------
// A `finally` written INSIDE a catch handler body. With the catch clause bodies now lowered as
// ordinary parent code rather than funclet interiors, this exercises a try/finally that is nested
// in a catch without any funclet nesting -- a path that did not exist before that change.
// ---------------------------------------------------------------------------------------
void testFinallyInsideCatchHandlerBody(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        private static int marks;
        public static int Main()
        {
            try { Boom problem = new Boom(); throw problem; }
            catch (Boom problem)
            {
                try { marks = marks + 3; }
                finally { marks = marks + 30; }
            }
            return marks;
        }
    }
)NS";
    run(tests, root, "FinallyInsideCatchHandlerBody", source, 33);
}

// ---------------------------------------------------------------------------------------
// `using` must Dispose exactly once when its body throws. Counts DISPOSALS, not destructions:
// Stage45LlvmCleanupFuncletTests asserts that the using cleanup produces a `cleanuppad` in the IR
// and that IDisposable dispatch survives, but never observes a single Dispose() actually running.
// ---------------------------------------------------------------------------------------
void testUsingDisposesExactlyOnceOnThrow(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Inner()
        {
            Res resource = new Res(1);
            using (Res scoped = resource) { scoped.Fail(); }
        }
        public static int Main()
        {
            try { Inner(); } catch (Boom problem) { }
            return Res.Disposals();
        }
    }
)NS";
    run(tests, root, "UsingDisposesOnceOnThrow", source, 1);
}

// ---------------------------------------------------------------------------------------
// `using` whose scope ENCLOSES a try that catches: Dispose must still happen exactly once, and not
// early. Same shape that destroyed `raii` objects prematurely (B-006), on the `using` cleanup path.
// ---------------------------------------------------------------------------------------
void testUsingEnclosingCatchingTryDisposesOnce(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Inner()
        {
            Res resource = new Res(1);
            using (Res scoped = resource)
            {
                try { scoped.Fail(); } catch (Boom problem) { }
            }
        }
        public static int Main()
        {
            Inner();
            return Res.Disposals();
        }
    }
)NS";
    run(tests, root, "UsingEnclosingCatchDisposesOnce", source, 1);
}

// ---------------------------------------------------------------------------------------
// An `owned` parameter destroyed exactly once when the method it was moved into throws. This is
// the second cleanup-guard site (emitOwnedParameterDestroy); Stage50_8OwnershipIrLoweringTests
// asserts only that it "reuses the existing Win64 exceptional cleanup machinery" in the IR text.
// ---------------------------------------------------------------------------------------
void testOwnedParameterDestroyedOnceOnThrow(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Consume(owned Res value) { value.Fail(); }
        public static int Main()
        {
            raii Res resource = new Res(1);
            try { Consume(move(resource)); } catch (Boom problem) { }
            return Res.Destructions();
        }
    }
)NS";
    run(tests, root, "OwnedParameterDestroyedOnceOnThrow", source, 1);
}

// ---------------------------------------------------------------------------------------
// An `owned` parameter whose method catches the exception itself: the parameter must be destroyed
// exactly once, on the normal exit, and NOT early during the unwind to that catch.
// ---------------------------------------------------------------------------------------
void testOwnedParameterEnclosingCatchDestroyedOnce(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Consume(owned Res value)
        {
            try { value.Fail(); } catch (Boom problem) { }
        }
        public static int Main()
        {
            raii Res resource = new Res(1);
            Consume(move(resource));
            return Res.Destructions();
        }
    }
)NS";
    run(tests, root, "OwnedParameterEnclosingCatchOnce", source, 1);
}

// ---------------------------------------------------------------------------------------
// `rethrow` from inside a catch must still run the enclosing cleanup exactly once on the way out.
// `rethrow` finds its exception object differently since catch bodies stopped being funclets, so
// this is the execution check for that change.
// ---------------------------------------------------------------------------------------
void testRethrowRunsEnclosingCleanupOnce(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Inner()
        {
            raii Res guard = new Res(5);
            try { guard.Fail(); }
            catch (Boom problem) { throw; }
        }
        public static int Main()
        {
            try { Inner(); } catch (Boom problem) { }
            return Res.Destructions();
        }
    }
)NS";
    run(tests, root, "RethrowRunsEnclosingCleanupOnce", source, 1);
}

// ---------------------------------------------------------------------------------------
// A throw out of a CONSTRUCTOR, with a `raii` local already live in the calling scope: the live
// local must be destroyed exactly once, and the partially-constructed object must not be.
// ---------------------------------------------------------------------------------------
void testThrowFromConstructorDestroysLiveRaiiOnce(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Failing { public Failing() { Boom problem = new Boom(); throw problem; } }

    class Program
    {
        public static void Inner()
        {
            raii Res guard = new Res(1);
            Failing bad = new Failing();
        }
        public static int Main()
        {
            try { Inner(); } catch (Boom problem) { }
            return Res.Destructions();
        }
    }
)NS";
    run(tests, root, "ThrowFromConstructorDestroysOnce", source, 1);
}

// ---------------------------------------------------------------------------------------
// A catching try inside a LOOP, over a `raii` object used after the loop. Three iterations each
// throw and catch; the object must survive all of them and still be usable afterwards. Guards the
// premature-destruction fix against a shape where the same cleanup region is entered repeatedly.
// ---------------------------------------------------------------------------------------
void testRaiiSurvivesRepeatedCatchInLoop(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Account
    {
        private int balance;
        public Account(int initial) { balance = initial; }
        public void Withdraw(int amount)
        {
            if (amount > balance) { Boom problem = new Boom(); throw problem; }
            balance = balance - amount;
        }
        public int Balance() { return balance; }
    }

    class Program
    {
        public static int Main()
        {
            raii Account account = new Account(100);
            int index = 0;
            while (index < 3)
            {
                try { account.Withdraw(500); } catch (Boom problem) { }
                index = index + 1;
            }
            account.Withdraw(30);
            return account.Balance();
        }
    }
)NS";
    // 100 untouched by the three failed withdrawals, then 100 - 30 = 70 -- reachable only if the
    // account is still alive after three separate handled catches.
    run(tests, root, "RaiiSurvivesRepeatedCatchInLoop", source, 70);
}
}

int main()
{
    TestContext tests;
    const auto root =
        std::filesystem::temp_directory_path() / "nativesharp_stage45_11_eh_cleanup_execution_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testFinallyRunsWhenExceptionEscapes(tests, root);
    testFinallyRunsWhenThrowComesFromInlinedHelper(tests, root);
    testNestedFinallyBothRunWhenExceptionEscapes(tests, root);
    testInnerFinallyOuterCatchAndOuterFinally(tests, root);
    testFinallyInsideCatchHandlerBody(tests, root);
    testUsingDisposesExactlyOnceOnThrow(tests, root);
    testUsingEnclosingCatchingTryDisposesOnce(tests, root);
    testOwnedParameterDestroyedOnceOnThrow(tests, root);
    testOwnedParameterEnclosingCatchDestroyedOnce(tests, root);
    testRethrowRunsEnclosingCleanupOnce(tests, root);
    testThrowFromConstructorDestroysLiveRaiiOnce(tests, root);
    testRaiiSurvivesRepeatedCatchInLoop(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
