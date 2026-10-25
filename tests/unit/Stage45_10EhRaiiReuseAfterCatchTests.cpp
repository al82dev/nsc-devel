// Stage 45.10 (fix) -- B-001 regression suite: "reusing a RAII object in a sibling try/anything
// after a REAL, handled catch" no longer crashes.
//
// Bug report: BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md (found writing
// examples/tutorial_19_excepciones, NOT a synthetic test -- see that file for the original
// minimal repro). External audit: claude/NativeSharp_Gaps_y_Prioridades_Siguiente_Version.md,
// item B-001 ("BUG CONFIRMADO P0 -- EH + RAII tras un catch"). Full root-cause writeup:
// claude/30_bug_b001_eh_raii_mem2reg_fix.md.
//
// Root cause (confirmed via real Wine PE execution, winedbg, and a Wine +seh trace -- NOT
// guessed): LLVM's "default<O0>" pass pipeline never runs mem2reg, so every local in a -O0
// build stays a raw, un-promoted `alloca`. For an ORDINARY function that is harmless. For a
// function that declares a Windows SEH personality (any real try/catch or try/finally -- see
// LlvmOptimizer.cpp's legalizeSehFuncletFunctionsForO0), LLVM's WinEHPrepare/SelectionDAG
// lowering of `catchswitch`/`catchpad`/`cleanuppad` at CodeGenOpt::None miscompiles the function
// the moment a *later* exception-protected call executes after a real catch has already run and
// returned via `catchret` -- confirmed via winedbg to be a genuine STATUS_ACCESS_VIOLATION
// (0xC0000005) on that LATER call, not on anything related to the throw/catch itself. Confirmed
// via a five-repro bisection that NEITHER a second try block, NOR a `finally`, NOR a second
// RAII object is required to trigger it: one plain method call after a handled catch, anywhere
// later in the same function, is already enough, as long as that call also needs unwind
// protection (which any call inside a `raii` local's lexical scope always does). Confirmed via
// direct opt-level bisection that -O1 and -O2 (which already run mem2reg as part of their own
// default pipeline) were NEVER affected -- only -O0. Fix: LlvmOptimizer::optimize now runs
// `mem2reg` on exactly the functions that declare a Windows SEH personality, before running the
// "default<O0>" pipeline on the rest of the module, matching what -O1/-O2 already do for those
// functions specifically instead of promoting every -O0 local project-wide (see that function's
// own header comment for why the narrower scope was chosen over a blanket fix -- both were
// verified against the full suite, only the narrow one was kept).
//
// Every scenario below is checked via REAL execution (compile to a genuine Windows x64 PE
// through the actual `nsc` Driver, then run it under Wine when available -- never just "compiles
// and LLVM-verifies", exactly the gap that let this bug ship silently in the first place), at
// BOTH -O0 (where the bug lived) and -O2 (which was always clean and must STAY clean), per the
// project's own "test de regresión obligatorio" checklist for this bug.
//
// IMPORTANT -- building this suite exposed a SECOND, deeper bug that B-001's mem2reg fix did not
// resolve, and that this suite now also guards (bugs B-006/B-007, both since FIXED; see
// BUGS_ENCONTRADOS/bug_eh_raii_doble_destruccion_tras_catch_o0.md and
// BUGS_ENCONTRADOS/bug_eh_try_anidado_dentro_de_catch_o0.md for the full writeups, and
// ProjectOopLlvmLowerer.cpp's catchswitch comment for the fix):
//
// Any cleanup-owning local whose scope ENCLOSES a try whose catch actually catches was destroyed
// AND FREED during the unwind to that catch, while still in scope -- so every use of it after the
// catch was a use-after-free, and its destructor ran twice. It looked at first like two separate
// -O0-only bugs (a double destructor count, and a nested try-inside-catch whose inner catch
// "failed to catch"); both turned out to be the same premature free, and both turned out to occur
// at -O2 as well, where optimization merely hid the symptom in the original test shapes. The
// cause was NOT this compiler emitting bad IR: `__C_specific_handler` genuinely runs an enclosing
// cleanup while unwinding to a catch in the same frame -- proven by compiling the analogous C SEH
// construct with clang and running it under this same Wine, where clang's own binary double-runs
// its __finally identically. The fix therefore stops relying on that unwind edge entirely.
//
// Chasing that fix also uncovered a THIRD, unrelated and pre-existing P0 bug: a try's SECOND and
// later `catch` clauses never matched at all, because `__C_specific_handler` honours exactly one
// handler per catchswitch and LLVM's SEH state numbering emits a scope-table entry for the first
// one only. Every clause past the first was dead code, and a program whose matching clause was not
// the first let the exception escape to the OS. Nothing in the project's tests covered it, because
// every existing EH test happens to use a single clause.
//
// All three are fixed by one restructure (see lowerTryRegion's catchswitch comment): the
// catchswitch unwinds to caller and owns ONE catchpad, screened by the generic "is this one of
// ours" filter, that catchrets straight out to ordinary parent code. That parent code re-tests the
// caught object against each clause's RTTI type in source order -- so every clause works -- and,
// when no clause matches, runs the enclosing cleanups explicitly and re-raises.
//
// Every scenario in this file is asserted at BOTH optimization levels; nothing here is a
// "documented known issue" any more. Scenarios 7-13 exist because the original six could not have
// caught any of this: they all used a fixture with no destructor, so they passed while silently
// operating on freed memory, and they all used a single catch clause.

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

// Same discovery order as every other real-execution test in this suite (e.g.
// Stage52_4OwnedCleanupRuntimeTests.cpp) -- try the sandbox's fixed Wine location first, then
// fall back to PATH.
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
    // 135 = 128+7, this bug's own signature exit code observed via Wine's crash-to-exit-code
    // translation for an unhandled STATUS_ACCESS_VIOLATION -- kept as an explicit extra check
    // (not just "crashed==false") so a regression that happens to print nothing useful to the
    // log still gets caught by the exit-code shape alone.
    crashed = log.find("page fault") != std::string::npos ||
        log.find("Unhandled exception") != std::string::npos ||
        exitCode == 135;
    return true;
}

// Compiles `directory`'s project to a real Windows x64 PE executable at the given optimization
// level through the actual `nsc` Driver (the exact code path `nsc --emit-project-oop-exe ...
// -O0|-O2 -o out.exe` runs), then -- when Wine is available -- executes it and checks both "did
// not crash" and "returned the exact expected value". `label` is folded into every assertion so
// a failure names its scenario AND its optimization level without needing to scroll up.
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
    tests.expect(result == ExitCode::Success, std::string(label) + ": nsc emits a real Windows x64 PE executable");
    tests.expect(err.str().empty(), std::string(label) + ": no driver diagnostics (" + err.str() + ")");
    if (result != ExitCode::Success) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string(label) + ": Wine not available -- real-execution check skipped");
        return;
    }
    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(wine, exePath, exePath.string() + ".log", exitCode, crashed);
    tests.expect(ran, std::string(label) + ": Wine could be invoked");
    tests.expect(!crashed, std::string(label) + ": does not crash under real execution (bug B-001)");
    tests.expect(
        exitCode == expectedExitCode,
        std::string(label) + ": returns the exact expected exit code (" +
            std::to_string(expectedExitCode) + ", got " + std::to_string(exitCode) + ")");
}

// Runs the same source through both -O0 (where B-001 lived) and -O2 (which was always clean and
// must stay clean) -- every scenario in this file goes through this single entry point so the
// O0/O2 matrix required by the bug's own regression checklist is never accidentally skipped for
// a new scenario.
void compileAndRunBothOptLevels(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view projectName,
    std::string_view source,
    int expectedExitCode)
{
    const auto directory = root / std::string(projectName);
    writeProject(directory, projectName);
    writeFile(directory / "Main.ns", source);

    compileAndRun(
        tests, directory, directory / "out_o0.exe", "-O0",
        std::string(projectName) + " [-O0]", expectedExitCode);
    compileAndRun(
        tests, directory, directory / "out_o2.exe", "-O2",
        std::string(projectName) + " [-O2]", expectedExitCode);
}

// Shared fixture for the exactly-once-destruction scenarios: an account class that COUNTS its own
// destructions in a static, so a test can observe how many times its cleanup actually ran. The
// plain `Account` fixture above deliberately has no destructor, which is exactly why the original
// B-001 suite could not see bug B-006 at all.
constexpr std::string_view kCountingFixture = R"NS(
namespace NativeSharp.Core
{
    public class Exception { public string Message; }
}
using NativeSharp.Core;

class InsufficientFundsException : Exception
{
    public int Requested;
}

class CountingAccount
{
    private int balance;
    private static int destructionCount;
    public CountingAccount(int initialBalance) { balance = initialBalance; }
    ~CountingAccount() { destructionCount = destructionCount + 1; }
    public void Withdraw(int amount)
    {
        if (amount > balance)
        {
            raii InsufficientFundsException problem = new InsufficientFundsException();
            problem.Requested = amount;
            throw move(problem);
        }
        balance = balance - amount;
    }
    public int Balance() { return balance; }
    public static int DestructionCount() { return destructionCount; }
}
)NS";

// Same self-contained "define a minimal NativeSharp.Core.Exception stub locally" convention
// every other Stage 45 EH test file in this suite already uses (see e.g.
// Stage45LlvmWin64EhBasicTests.cpp) rather than depending on a real NativeSharp.Core project
// reference these fixtures never declare.
constexpr std::string_view kAccountFixture = R"NS(
namespace NativeSharp.Core
{
    public class Exception { public string Message; }
}
using NativeSharp.Core;

class InsufficientFundsException : Exception
{
    public int Requested;
}

class Account
{
    private int balance;
    public Account(int initialBalance) { balance = initialBalance; }
    public void Withdraw(int amount)
    {
        if (amount > balance)
        {
            raii InsufficientFundsException problem = new InsufficientFundsException();
            problem.Requested = amount;
            throw move(problem);
        }
        balance = balance - amount;
    }
    public int Balance() { return balance; }
}
)NS";

// ---------------------------------------------------------------------------------------
// Scenario 1: the bug report's own minimal repro, verbatim -- catch a real exception, then a
// SECOND try/finally reusing the SAME raii object. This is the exact shape that used to SIGSEGV.
// ---------------------------------------------------------------------------------------
void testCatchThenSecondTryFinallySameRaii(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kAccountFixture) + R"NS(
    class Program
    {
        public static int Main()
        {
            raii Account account = new Account(100);
            int caught = 0;
            try
            {
                account.Withdraw(500);
            }
            catch (InsufficientFundsException problem)
            {
                caught = problem.Requested;
            }
            try
            {
                account.Withdraw(30);
            }
            finally
            {
            }
            return caught % 256;
        }
    }
)NS";
    // caught = 500, 500 % 256 = 244.
    compileAndRunBothOptLevels(tests, root, "CatchThenSecondTryFinally", source, 244);
}

// ---------------------------------------------------------------------------------------
// Scenario 2: catch + a second try that has its OWN catch clause (not finally) on the same
// raii object -- the bug report only pinned down try/finally; this closes the try/catch variant.
// ---------------------------------------------------------------------------------------
void testCatchThenSecondTryCatchSameRaii(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kAccountFixture) + R"NS(
    class Program
    {
        public static int Main()
        {
            raii Account account = new Account(100);
            int firstCaught = 0;
            int secondCaught = 0;
            try
            {
                account.Withdraw(500);
            }
            catch (InsufficientFundsException problem)
            {
                firstCaught = problem.Requested;
            }
            try
            {
                account.Withdraw(999);
            }
            catch (InsufficientFundsException problem)
            {
                secondCaught = problem.Requested;
            }
            return (firstCaught + secondCaught) % 256;
        }
    }
)NS";
    // firstCaught = 500, secondCaught = 999 (balance still 100, unchanged by the first throw).
    // (500 + 999) % 256 = 1499 % 256 = 219.
    compileAndRunBothOptLevels(tests, root, "CatchThenSecondTryCatch", source, 219);
}

// ---------------------------------------------------------------------------------------
// Scenario 3: the MINIMAL trigger found during diagnosis -- catch, then a plain method call on
// the same raii object with NO enclosing try at all. No second try/catch/finally is needed for
// the bug to fire, so this is the smallest possible regression guard for it.
// ---------------------------------------------------------------------------------------
void testCatchThenPlainCallNoSecondTry(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kAccountFixture) + R"NS(
    class Program
    {
        public static int Main()
        {
            raii Account account = new Account(100);
            int caught = 0;
            try
            {
                account.Withdraw(500);
            }
            catch (InsufficientFundsException problem)
            {
                caught = problem.Requested;
            }
            account.Withdraw(30);
            return account.Balance() + (caught - caught);
        }
    }
)NS";
    // balance stays 100 (first Withdraw threw before decrementing), then Withdraw(30) succeeds:
    // 100 - 30 = 70.
    compileAndRunBothOptLevels(tests, root, "CatchThenPlainCall", source, 70);
}

// ---------------------------------------------------------------------------------------
// Scenario 4 (bug B-007, FIXED -- see
// BUGS_ENCONTRADOS/bug_eh_try_anidado_dentro_de_catch_o0.md): a try NESTED inside another try's
// catch handler, both touching the same raii object. Beyond B-001's own documented scope ("dos try
// hermanos, no anidados", see BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md), and it
// used to fail with the INNER catch apparently not catching its own exception, which reached the
// OS unhandled (0xe04e5301). That symptom was misleading: the inner catch worked fine all along --
// the raii account had already been FREED during the unwind to the OUTER catch (bug B-006), so the
// later Withdraw() read a garbage balance from reused heap memory and threw a THIRD exception that
// genuinely had no handler. Fixing the premature free fixed this too; kept as its own scenario
// because "nested try inside a catch" exercises funclet parentage no other scenario here does.
// ---------------------------------------------------------------------------------------
void testNestedTryInsideCatchSameRaii(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kAccountFixture) + R"NS(
    class Program
    {
        public static int Main()
        {
            raii Account account = new Account(100);
            int outerCaught = 0;
            int innerCaught = 0;
            try
            {
                account.Withdraw(500);
            }
            catch (InsufficientFundsException problem)
            {
                outerCaught = problem.Requested;
                try
                {
                    account.Withdraw(777);
                }
                catch (InsufficientFundsException innerProblem)
                {
                    innerCaught = innerProblem.Requested;
                }
            }
            account.Withdraw(10);
            return (outerCaught + innerCaught + account.Balance()) % 256;
        }
    }
)NS";
    // outerCaught=500, innerCaught=777, balance after Withdraw(10) succeeds = 100-10=90.
    // (500+777+90) % 256 = 1367 % 256 = 87 -- the value -O1/-O2 already return correctly, and
    // what -O0 should return once bug_eh_try_anidado_dentro_de_catch_o0.md's root cause is fixed.
    compileAndRunBothOptLevels(tests, root, "NestedTryInsideCatch", source, 87);
}

// ---------------------------------------------------------------------------------------
// Scenario 5: TWO independent raii objects, both used after a catch handled on the first one --
// confirms the fix does not depend on there being exactly one raii local in scope, and that an
// unrelated second raii object's own cleanup chain is unaffected.
// ---------------------------------------------------------------------------------------
void testMultipleRaiiObjectsAfterCatch(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kAccountFixture) + R"NS(
    class Program
    {
        public static int Main()
        {
            raii Account first = new Account(100);
            raii Account second = new Account(50);
            int caught = 0;
            try
            {
                first.Withdraw(500);
            }
            catch (InsufficientFundsException problem)
            {
                caught = problem.Requested;
            }
            first.Withdraw(20);
            second.Withdraw(15);
            return (first.Balance() + second.Balance() + (caught - caught)) % 256;
        }
    }
)NS";
    // first: 100-20=80, second: 50-15=35, total=115.
    //
    // CAVEAT (see BUGS_ENCONTRADOS/bug_eh_raii_doble_destruccion_tras_catch_o0.md): `Account`
    // above declares no destructor, so this assertion can only observe "does not crash" and
    // "computes the right balance" -- it CANNOT detect whether `first`'s raii cleanup silently
    // ran twice at -O0. Confirmed separately (with a destructor-counting class in that same
    // shape) that it does. This scenario staying green is expected and correct for what it
    // actually checks; it is not evidence the double-destruction bug is absent here.
    compileAndRunBothOptLevels(tests, root, "MultipleRaiiObjectsAfterCatch", source, 115);
}

// ---------------------------------------------------------------------------------------
// Scenario 6 (bug B-006, FIXED -- see
// BUGS_ENCONTRADOS/bug_eh_raii_doble_destruccion_tras_catch_o0.md): destructor counter -- confirms
// the raii object reused after a handled catch is destroyed EXACTLY ONCE. This is the scenario
// that exposed B-006 in the first place: every other scenario in this file uses the `Account`
// fixture, which has NO destructor, so none of them could observe how many times cleanup actually
// ran -- they passed while silently operating on freed memory that merely happened to still hold
// its old contents. A count of 0 would mean the cleanup never ran (a leak); anything above 1 means
// it ran more than once (the double-free B-006 was).
// ---------------------------------------------------------------------------------------
void testDestructorRunsExactlyOnceAfterCatchReuse(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
namespace NativeSharp.Core
{
    public class Exception { public string Message; }
}
using NativeSharp.Core;

class InsufficientFundsException : Exception
{
    public int Requested;
}

class CountingAccount
{
    private int balance;
    private static int destructionCount;
    public CountingAccount(int initialBalance) { balance = initialBalance; }
    ~CountingAccount() { destructionCount = destructionCount + 1; }
    public void Withdraw(int amount)
    {
        if (amount > balance)
        {
            raii InsufficientFundsException problem = new InsufficientFundsException();
            problem.Requested = amount;
            throw move(problem);
        }
        balance = balance - amount;
    }
    public static int DestructionCount() { return destructionCount; }
}

class Program
{
    public static int RunScope()
    {
        raii CountingAccount account = new CountingAccount(100);
        int caught = 0;
        try
        {
            account.Withdraw(500);
        }
        catch (InsufficientFundsException problem)
        {
            caught = problem.Requested;
        }
        account.Withdraw(30);
        return caught - caught;
    }

    public static int Main()
    {
        RunScope();
        // Exactly one CountingAccount was ever constructed (inside RunScope), so exactly one
        // destruction is correct; 0 would mean the post-catch cleanup chain never ran (a
        // leak), anything above 1 would mean it ran more than once (a double-free).
        return CountingAccount.DestructionCount();
    }
}
)NS";
    compileAndRunBothOptLevels(tests, root, "DestructorCounterAfterCatchReuse", source, 1);
}

// ---------------------------------------------------------------------------------------
// Scenario 7 (bug B-006's REAL severity): the object reused after a handled catch must still be
// ALIVE and INTACT, not merely "destroyed the right number of times".
//
// This is the check whose absence let B-006 hide for so long. Before the fix the raii account was
// freed during the unwind to the catch, yet scenarios 1-5 still passed -- freed heap memory
// happens to keep its old contents until something else claims it, so `Balance()` kept returning
// the right number over a dangling pointer. Allocating inside the catch handler is what makes the
// difference observable: the allocator hands that memory out again, the old contents are
// overwritten, and any surviving use-after-free turns into a wrong balance (or a throw from a
// garbage comparison) instead of a silent pass.
// ---------------------------------------------------------------------------------------
void testRaiiObjectStillUsableAfterCatchWithHeapChurn(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kAccountFixture) + R"NS(
    class Program
    {
        public static int Main()
        {
            raii Account account = new Account(100);
            try
            {
                account.Withdraw(500);
            }
            catch (InsufficientFundsException problem)
            {
                int index = 0;
                while (index < 40)
                {
                    raii InsufficientFundsException filler = new InsufficientFundsException();
                    filler.Requested = 987654321;
                    index = index + 1;
                }
            }
            account.Withdraw(30);
            return account.Balance();
        }
    }
)NS";
    // balance stays 100 through the failed Withdraw(500), then 100 - 30 = 70 -- but only if
    // `account` still points at live, uncorrupted memory after 40 intervening allocations.
    compileAndRunBothOptLevels(tests, root, "RaiiUsableAfterCatchWithHeapChurn", source, 70);
}

// ---------------------------------------------------------------------------------------
// Scenario 8 (bug B-006): exactly-once destruction WITHOUT any reuse after the catch.
//
// Pins down that "reuse after the catch" was never the trigger -- merely having a cleanup-owning
// local whose scope encloses a try that catches was enough to destroy it during the unwind. This
// is the smallest shape that reproduced B-006, and it is strictly smaller than B-001's own repro.
// ---------------------------------------------------------------------------------------
void testDestructorRunsExactlyOnceWithNoReuseAfterCatch(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kCountingFixture) + R"NS(
    class Program
    {
        public static int RunScope()
        {
            raii CountingAccount account = new CountingAccount(100);
            try
            {
                account.Withdraw(500);
            }
            catch (InsufficientFundsException problem)
            {
            }
            return 0;
        }

        public static int Main()
        {
            RunScope();
            return CountingAccount.DestructionCount();
        }
    }
)NS";
    compileAndRunBothOptLevels(tests, root, "DestructorOnceNoReuseAfterCatch", source, 1);
}

// ---------------------------------------------------------------------------------------
// Scenario 9 (bug B-006, opposite direction): a raii local declared INSIDE the try really IS
// exited by the throw, so its cleanup MUST run during the unwind -- exactly once.
//
// Guards the fix from over-correcting. The B-006 fix stops an ENCLOSING cleanup from running on
// the way to a catch; it must not also suppress a cleanup that the throw genuinely leaves. A
// result of 0 here would mean the fix turned a double-free into a leak.
// ---------------------------------------------------------------------------------------
void testRaiiDeclaredInsideTryStillDestroyedOnThrow(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kCountingFixture) + R"NS(
    class Program
    {
        public static int RunScope()
        {
            try
            {
                raii CountingAccount account = new CountingAccount(100);
                account.Withdraw(500);
            }
            catch (InsufficientFundsException problem)
            {
            }
            return 0;
        }

        public static int Main()
        {
            RunScope();
            return CountingAccount.DestructionCount();
        }
    }
)NS";
    compileAndRunBothOptLevels(tests, root, "RaiiInsideTryDestroyedOnThrow", source, 1);
}

// ---------------------------------------------------------------------------------------
// Scenario 10 (bug B-006 fix, leak guard): an exception that is NOT caught here must still run the
// enclosing cleanup on its way out.
//
// The B-006 fix works by taking the enclosing cleanups OFF the catchswitch's unwind edge and
// giving that job to a synthetic always-match catch-all clause, which the personality reaches only
// once every user filter has declined. If that catch-all were wrong or missing, this is where it
// would show: the inner method's typed catch does not match the thrown type, so the exception must
// travel out through the raii scope, destroying the account exactly once on the way.
// ---------------------------------------------------------------------------------------
void testUnmatchedCatchStillRunsEnclosingCleanup(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kCountingFixture) + R"NS(
    class OtherException : Exception
    {
        public int Code;
    }

    class Program
    {
        public static int RunScope()
        {
            raii CountingAccount account = new CountingAccount(100);
            try
            {
                account.Withdraw(500);
            }
            catch (OtherException other)
            {
            }
            return 0;
        }

        public static int Main()
        {
            try
            {
                RunScope();
            }
            catch (InsufficientFundsException problem)
            {
            }
            return CountingAccount.DestructionCount();
        }
    }
)NS";
    // RunScope's own catch does not match InsufficientFundsException, so the exception leaves
    // RunScope -- and `account` must be destroyed exactly once on the way out, not leaked.
    compileAndRunBothOptLevels(tests, root, "UnmatchedCatchRunsEnclosingCleanup", source, 1);
}

// ---------------------------------------------------------------------------------------
// Scenario 11: a try with MORE THAN ONE catch clause, where the matching clause is not the first.
//
// Found while fixing B-006/B-007, and entirely independent of them: `__C_specific_handler` honours
// exactly ONE handler per catchswitch, and LLVM's SEH state numbering emits a scope-table entry for
// the first one only. Every clause after the first was therefore dead code, and a program whose
// matching clause was not the first simply let the exception escape to the OS. That is a P0
// language-correctness bug that nothing in this suite -- or anywhere else in the project's tests --
// covered, because every existing EH test happens to use a single catch clause.
//
// It is guarded here because the fix is the same restructure: one catchpad that catchrets to
// ordinary parent code, which re-tests the caught object against each clause's RTTI type in source
// order. See lowerTryRegion's catchswitch comment.
// ---------------------------------------------------------------------------------------
void testSecondCatchClauseMatches(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kAccountFixture) + R"NS(
    class UnrelatedException : Exception
    {
        public int Code;
    }

    class Program
    {
        public static int Main()
        {
            raii Account account = new Account(100);
            int hit = 0;
            try
            {
                account.Withdraw(500);
            }
            catch (UnrelatedException unrelated)
            {
                hit = 1;
            }
            catch (InsufficientFundsException problem)
            {
                hit = 2;
            }
            return hit;
        }
    }
)NS";
    // The thrown type matches the SECOND clause only: 1 would mean clause order is being ignored,
    // and a crash/escape (the pre-fix behaviour) would mean clauses past the first are dead.
    compileAndRunBothOptLevels(tests, root, "SecondCatchClauseMatches", source, 2);
}

// ---------------------------------------------------------------------------------------
// Scenario 12: a catch-all clause placed after typed clauses still catches what they decline, and
// a `raii` object enclosing the try survives it intact.
//
// Complements scenario 11 from the other side: clause selection must fall through every typed test
// to the terminal catch-all, and doing so must not disturb the enclosing cleanup.
// ---------------------------------------------------------------------------------------
void testCatchAllAfterTypedClauseWithEnclosingRaii(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kAccountFixture) + R"NS(
    class UnrelatedException : Exception
    {
        public int Code;
    }

    class Program
    {
        public static int Main()
        {
            raii Account account = new Account(100);
            int hit = 0;
            try
            {
                account.Withdraw(500);
            }
            catch (UnrelatedException unrelated)
            {
                hit = 1;
            }
            catch
            {
                hit = 2;
            }
            account.Withdraw(30);
            return hit + account.Balance();
        }
    }
)NS";
    // hit = 2 via the catch-all, balance = 100 - 30 = 70, total 72 -- and reaching 72 at all means
    // `account` was still alive after the catch.
    compileAndRunBothOptLevels(tests, root, "CatchAllAfterTypedClause", source, 72);
}

// ---------------------------------------------------------------------------------------
// Scenario 13: `finally` runs on BOTH the matched-catch and the unmatched-catch paths.
//
// The B-006/B-007 fix changed how a try/catch/finally reaches its finally on the exceptional path
// (it is no longer the catchswitch's unwind destination -- see lowerTryRegion's comment), so the
// guarantee is re-pinned here by real execution rather than by IR shape. Stage45LlvmWin64EhBasicTests
// used to assert the old IR shape and now points here for the behaviour.
// ---------------------------------------------------------------------------------------
void testFinallyRunsOnBothMatchedAndUnmatchedCatchPaths(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
namespace NativeSharp.Core
{
    public class Exception { public string Message; }
}
using NativeSharp.Core;

class InsufficientFundsException : Exception
{
    public int Requested;
}

class UnrelatedException : Exception
{
    public int Code;
}

class Program
{
    private static int finallyRuns;

    public static void Thrower()
    {
        raii InsufficientFundsException problem = new InsufficientFundsException();
        problem.Requested = 1;
        throw move(problem);
    }

    // The catch MATCHES: the finally must still run on the way out of the try.
    public static void Matched()
    {
        try { Thrower(); }
        catch (InsufficientFundsException problem) { }
        finally { finallyRuns = finallyRuns + 1; }
    }

    // The catch does NOT match: the exception leaves this method, and the finally must run
    // before it does -- this is the path the fix routes through the explicit unmatched block.
    public static void Unmatched()
    {
        try { Thrower(); }
        catch (UnrelatedException unrelated) { }
        finally { finallyRuns = finallyRuns + 10; }
    }

    public static int Main()
    {
        Matched();
        try { Unmatched(); }
        catch (InsufficientFundsException problem) { }
        return finallyRuns;
    }
}
)NS";
    // 1 (matched path) + 10 (unmatched path) = 11. A result of 1 would mean the finally is skipped
    // whenever the exception escapes; 10 would mean it is skipped whenever the catch matches.
    compileAndRunBothOptLevels(tests, root, "FinallyRunsOnBothCatchPaths", source, 11);
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage45_10_eh_raii_reuse_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testCatchThenSecondTryFinallySameRaii(tests, root);
    testCatchThenSecondTryCatchSameRaii(tests, root);
    testCatchThenPlainCallNoSecondTry(tests, root);
    testNestedTryInsideCatchSameRaii(tests, root);
    testMultipleRaiiObjectsAfterCatch(tests, root);
    testDestructorRunsExactlyOnceAfterCatchReuse(tests, root);
    testRaiiObjectStillUsableAfterCatchWithHeapChurn(tests, root);
    testDestructorRunsExactlyOnceWithNoReuseAfterCatch(tests, root);
    testRaiiDeclaredInsideTryStillDestroyedOnThrow(tests, root);
    testUnmatchedCatchStillRunsEnclosingCleanup(tests, root);
    testSecondCatchClauseMatches(tests, root);
    testCatchAllAfterTypedClauseWithEnclosingRaii(tests, root);
    testFinallyRunsOnBothMatchedAndUnmatchedCatchPaths(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
