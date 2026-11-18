// Regression suite: a `catch` body that leaves by an exception destroys the locals of the enclosing
// scope exactly once and in order, at -O0 and -O2, for frame objects (`A a;`) and heap objects
// (`A a = new A();`).
//
// Defect (docs/3.0/main-fixes/19-relanzamiento-desde-catch-y-winehprepare.md): at -O1 and above
// SROA turns the owner slot of a cleanup-owning local declared before the `try` into a phi on the
// try's catchswitch and a phi on the catch body's cleanuppad. LLVM's WinEHPrepare (LLVM 23.1)
// stores the catchswitch phi into the cleanup's spill slot and then replaces that phi with poison,
// so the store is dropped and the cleanup funclet reads a stale slot. The program skipped the
// destructor of a frame object and passed a garbage pointer to the destructor of a heap object
// (an access violation). The fix rewrites that phi entry through a stack slot right before code
// generation (src/LLVM/Internal/WinEhCatchSwitchPhiLegalization.cpp).
//
// Every program returns 0 when the counts and the order are right and a distinct small code
// otherwise, so the check also works where only the low byte of the exit code survives (Wine).
// On Windows the executables run natively; elsewhere they run under Wine when it is installed.
//
// Suite de regresion: un cuerpo `catch` que sale por una excepcion destruye los locales del ambito
// que lo encierra exactamente una vez y en orden, a -O0 y -O2, con objetos de marco y en heap.

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

#ifndef _WIN32
bool commandAvailable(std::string_view command)
{
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
}

std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}
#endif

// Returns false when the executable cannot be run on this host.
bool runExecutable(const std::filesystem::path& exePath, int& exitCode)
{
#ifdef _WIN32
    exitCode = _wsystem((L"\"" + exePath.wstring() + L"\"").c_str());
    return true;
#else
    const std::string wine = wineCommand();
    if (wine.empty()) return false;
    const int raw = std::system(("WINEDEBUG=-all " + wine + " " + exePath.string() + " >/dev/null 2>&1").c_str());
    if (raw < 0) return false;
#ifdef WIFEXITED
    exitCode = WIFEXITED(raw) ? WEXITSTATUS(raw) : 255;
#else
    exitCode = raw & 0xFF;
#endif
    return true;
#endif
}

void compileAndRun(
    TestContext& tests,
    const std::filesystem::path& directory,
    std::string_view projectName,
    std::string_view optimizationFlag)
{
    const std::string label = std::string(projectName) + " [" + std::string(optimizationFlag) + "]";
    const std::string projectText = (directory / (std::string(projectName) + ".nsproj")).string();
    const std::filesystem::path exePath = directory / ("out" + std::string(optimizationFlag) + ".exe");
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 6> args = {
        "--emit-project-oop-exe", projectText, optimizationFlag, "-o", exeText, "--no-artifact-cache"};

    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    tests.expect(result == ExitCode::Success, label + ": builds a Windows x64 executable (" + err.str() + ")");
    if (result != ExitCode::Success) return;

    int exitCode = -1;
    if (!runExecutable(exePath, exitCode))
    {
        tests.expect(true, label + ": no way to run a Windows executable on this host; execution skipped");
        return;
    }
    tests.expect(
        exitCode == 0,
        label + ": destructions happen exactly once and in order (exit code " + std::to_string(exitCode) +
            ", 0 expected; the program's Main names the failed check)");
}

std::string replaceAll(std::string text, std::string_view from, std::string_view to)
{
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
    return text;
}

constexpr std::string_view kFixture = R"NS(
namespace NativeSharp.Core { public class Exception { public int code; public Exception() {} } }
class Boom : NativeSharp.Core.Exception { public Boom() {} }
class Log { public static int drops; public static int order; public static int caught; }
class A { ~A() { Log.drops = Log.drops + 1; Log.order = Log.order * 10 + 1; } public int Read() { return 3; } }
class B { ~B() { Log.drops = Log.drops + 1; Log.order = Log.order * 10 + 2; } public int Read() { return 4; } }
class F { public static int Fail(int n) throws Boom { if (n > 0) { throw new Boom(); } return n; } }
)NS";

// `@A@` and `@B@` become the declarations of the two locals: `A a;` / `B b;` (frame objects) or
// `A a = new A();` / `B b = new B();` (heap objects). `@CATCH@` is the statement that leaves the
// catch body by an exception.
constexpr std::string_view kCatchExitProgram = R"NS(
class P
{
    static int Inner(int n) throws Boom
    {
        @A@
        try { @B@ return a.Read() + b.Read() + F.Fail(n); }
        catch (Boom e) { Log.caught = Log.caught + 1; @CATCH@ }
    }
    public static int Main()
    {
        int r = 0;
        try { r = Inner(0); } catch (Boom e) { return 1; }
        if (r != 7) { return 2; }
        if (Log.drops != 2 || Log.order != 21) { return 3; }
        Log.drops = 0;
        Log.order = 0;
        try { r = Inner(1); return 4; } catch (Boom e) { }
        if (Log.caught != 1) { return 5; }
        if (Log.drops != 2) { return 6; }
        if (Log.order != 21) { return 7; }
        return 0;
    }
}
)NS";

// The try/catch sits in a loop: the first failing iteration is handled, the second one leaves the
// catch body by a rethrow. B is destroyed on every iteration, A once, last.
constexpr std::string_view kLoopProgram = R"NS(
class P
{
    static int Loop(int first, int second) throws Boom
    {
        @A@
        int sum = 0;
        int i = 0;
        while (i < 4)
        {
            int fail = 0;
            if (i == first || i == second) { fail = 1; }
            try { @B@ sum = sum + b.Read() + F.Fail(fail); }
            catch (Boom e) { Log.caught = Log.caught + 1; if (i == second) { throw e; } }
            i = i + 1;
        }
        return sum + a.Read();
    }
    public static int Main()
    {
        int r = 0;
        try { r = Loop(1, 2); return 1; } catch (Boom e) { }
        if (Log.caught != 2) { return 2; }
        if (Log.drops != 4) { return 3; }
        if (Log.order != 2221) { return 4; }
        Log.caught = 0;
        Log.drops = 0;
        Log.order = 0;
        try { r = Loop(1, 9); } catch (Boom e) { return 5; }
        if (r != 15 || Log.caught != 1) { return 6; }
        if (Log.drops != 5 || Log.order != 22221) { return 7; }
        return 0;
    }
}
)NS";

// Control: the catch body ends normally. The locals are still destroyed once each, in order.
constexpr std::string_view kHandledProgram = R"NS(
class P
{
    static int Inner(int n)
    {
        @A@
        try { @B@ return a.Read() + b.Read() + F.Fail(n); }
        catch (Boom e) { Log.caught = Log.caught + 1; }
        return 50 + a.Read();
    }
    public static int Main()
    {
        if (Inner(1) != 53) { return 1; }
        if (Log.caught != 1 || Log.drops != 2 || Log.order != 21) { return 2; }
        return 0;
    }
}
)NS";

void runVariants(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view program,
    std::string_view catchExit)
{
    struct Variant
    {
        std::string_view suffix;
        std::string_view a;
        std::string_view b;
    };
    constexpr std::array<Variant, 2> variants = {{
        {"Frame", "A a;", "B b;"},
        {"Heap", "A a = new A();", "B b = new B();"},
    }};
    for (const Variant& variant : variants)
    {
        const std::string projectName = std::string(name) + std::string(variant.suffix);
        std::string source = std::string(kFixture) + std::string(program);
        source = replaceAll(source, "@A@", variant.a);
        source = replaceAll(source, "@B@", variant.b);
        source = replaceAll(source, "@CATCH@", catchExit);
        const std::filesystem::path directory = root / projectName;
        writeProject(directory, projectName);
        writeFile(directory / "Main.ns", source);
        compileAndRun(tests, directory, projectName, "-O0");
        compileAndRun(tests, directory, projectName, "-O2");
    }
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_wineh_catch_exit_cleanup_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    runVariants(tests, root, "Rethrow", kCatchExitProgram, "throw e;");
    runVariants(tests, root, "NewThrow", kCatchExitProgram, "throw new Boom();");
    runVariants(tests, root, "ThrowingCall", kCatchExitProgram, "F.Fail(1); return 0;");
    runVariants(tests, root, "LoopRethrow", kLoopProgram, "");
    runVariants(tests, root, "Handled", kHandledProgram, "");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
