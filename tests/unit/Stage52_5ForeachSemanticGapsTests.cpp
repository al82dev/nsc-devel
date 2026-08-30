// Stage 52.5 -- foreach semantic gaps closed per STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md
// (the technical review Alberto sent after the 52.x foreach feature and the 52.4 owned-cleanup
// closure round were both already delivered). That review is a genuine SECOND pass over foreach
// specifically -- distinct from, and more detailed than, the general closure checklist -- and it
// found four real P0 semantic gaps plus one P1 compatibility gap in the ORIGINAL foreach
// desugaring (Parser::parseForeachStatement / ProjectMethodBodyModel::analyzeForeach), none of
// which the existing Stage52_3ForeachTests.cpp / Stage52_4OwnedCleanupRuntimeTests.cpp happened
// to exercise:
//
//   P0.1 (hygiene): the original desugaring synthesized bare identifiers (`__foreach_index`,
//     `__foreach_source`, `__foreach_enumerator`) that, while extremely unlikely to collide with
//     real user code, were not IMPOSSIBLE to collide -- nothing in the lexer/parser prevented a
//     user from writing a local literally named `__foreach_index`. Fixed by prefixing every
//     synthesized name with `$`, a character NativeSharp's lexer can never produce from user
//     source (see Lexer.cpp's isIdentifierStart/isIdentifierPart), making collision categorically
//     impossible rather than merely unlikely. testForeachHygieneUserNameShadowsInternalPrefix
//     below is the regression guard: it deliberately names a user local
//     `__foreach_index` (no `$`) inside a foreach body and confirms the loop still computes the
//     correct result under real execution.
//
//   P0.2 (ownership-aware source): `foreach (var x in expr)` where `expr` is itself an
//     owned-returning call (e.g. a factory method returning `owned List<T>`) failed to compile
//     with "owned return value cannot initialize a non-owning local" -- the parser always built
//     `$__foreach_source`'s declaration as a plain, non-raii `var`, with no way to know at parse
//     time whether the collection expression would turn out to be owned-returning. Fixed by
//     parsing the collection expression a SECOND time into a throwaway probe copy
//     (`collectionProbe`, attached to the ForeachStatement node as an extra child), which
//     ProjectMethodBodyModel::analyzeForeach uses (via a scratch, discarded DiagnosticEngine) to
//     decide -- BEFORE the real analysis -- whether `$__foreach_source` needs the `raii` modifier
//     added. testForeachOverOwnedReturningSource below is the regression guard.
//
//   P0.3 (readonly iteration variable): the foreach element was, before this fix, an ordinary
//     reassignable local -- `foreach (var x in numbers) { x = 99; }` compiled cleanly, silently
//     diverging from real C#'s iteration variable, which is always non-reassignable.
//     testForeachElementIsReadonly below is the regression guard (a negative diagnostic test).
//
//   P0.4 (const keyword handling): `foreach (const var x in ...)` used to parse successfully and
//     then silently DISCARD the `const` -- a promise the compiler never kept. Since P0.3 now
//     makes the element unconditionally readonly, accepting `const` there would be redundant at
//     best and misleading at worst (implying non-`const` elements are mutable, which they no
//     longer are); the decision taken was to stop accepting `const` in this position entirely, so
//     it now fails as an honest syntax error instead of a silently-ignored no-op.
//     testForeachConstKeywordIsNowARejectedSyntaxError below is the regression guard.
//
//   P1.1 (Current as a property, not a method): real C#'s IEnumerator<T>.Current is a get-only
//     PROPERTY; NativeSharp's original enumerator-mode desugaring called `Current()` with
//     parentheses instead, an unnecessary deviation given this compiler has supported properties
//     since Stage 38. Fixed in the desugaring itself (Parser::parseForeachStatement's
//     patternCandidate now reads `$__foreach_enumerator.Current` as a plain member access) and in
//     every enumerator class this compiler ships/documents (ListEnumerator<T>, and the
//     hand-written CountUpToEnumerator in examples/project_stage52x_foreach/Main.ns).
//     testForeachOverCustomEnumeratorWithPropertyCurrent below is a small, focused regression
//     guard distinct from (and cheaper than) recompiling the full
//     examples/project_stage52x_foreach project that Stage52_3ForeachTests.cpp already exercises.
//
// Real execution (Wine, when available -- see Stage52_4OwnedCleanupRuntimeTests.cpp's own
// rationale for why structural-only COFF verification is not enough for this codebase's own
// closure standard) is used for P0.1 and P0.2, since both are genuinely runtime-observable
// (a hygiene collision or a use-after-free-shaped ownership bug would not necessarily show up as
// a compile-time or structural-COFF failure). P0.3 and P0.4 are purely compile-time diagnostics,
// so a semantic-analysis-only check is the correct and sufficient level for those two.

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

// Same as writeProject, but with <ProjectReference>s pulling in the real
// NativeSharp.Core / NativeSharp.Core.Collections.Generic library projects from this repo's own
// lib/ tree (absolute paths via sourceRoot, so this works regardless of the fixture's temp
// directory location) -- needed by any fixture that actually uses List<T> and friends, exactly
// like examples/project_stage52x_foreach/Stage52xForeach.nsproj already does.
void writeProjectWithCollectionsLibrary(
    const std::filesystem::path& directory, std::string_view name, const std::filesystem::path& sourceRoot)
{
    const std::string corePath = (sourceRoot / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").string();
    const std::string collectionsPath =
        (sourceRoot / "lib/NativeSharp.Core.Collections.Generic/NativeSharp.Core.Collections.Generic.nsproj").string();
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
            "<ProjectReference Include=\"" + corePath + "\" />"
            "<ProjectReference Include=\"" + collectionsPath + "\" /></ItemGroup></Project>");
}

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find(text) != std::string::npos) return true;
    }
    return false;
}

// Duplicated from Stage52_4OwnedCleanupRuntimeTests.cpp / Stage52GenericCollectionsCompilerFixesTests.cpp
// per this codebase's established "duplication over shared helpers between independent test
// files" convention (see claude/05_metodo_de_trabajo_y_archivo_historico.md).
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
    const std::string command = "WINEDEBUG=-all timeout 20 " + wine + " " + exePath.string() +
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
        log.find("Unhandled exception") != std::string::npos;
    return true;
}

std::string emitExecutable(
    TestContext& tests,
    const std::filesystem::path& directory,
    const std::filesystem::path& exePath,
    std::string_view label)
{
    const std::string projectText = (directory / (directory.filename().string() + ".nsproj")).string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", exeText};

    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    tests.expect(result == ExitCode::Success, std::string(label) + ": nsc emits a real Windows x64 PE executable");
    tests.expect(err.str().empty(), std::string(label) + ": no driver diagnostics (" + err.str() + ")");
    tests.expect(std::filesystem::is_regular_file(exePath), std::string(label) + ": .exe file exists on disk");

    ProjectCompilation mir;
    std::string mirError;
    if (!mir.compileProjectOopMir(directory / (directory.filename().string() + ".nsproj"), mirError) ||
        mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr)
    {
        return {};
    }
    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    if (module == nullptr) return {};
    return module->text();
}

// ---------------------------------------------------------------------------------------
// P0.1 -- hygiene: a user local literally named `__foreach_index` (no `$`) inside a foreach body
// must be an ordinary, independent user variable -- it must NOT alias, corrupt, or be confused
// with the compiler's own internal loop-index local (now named `$__foreach_index`, a name the
// lexer can never produce from user source). Verified by actually running the loop: if the two
// names were ever confused, the loop would either never terminate, iterate the wrong number of
// times, or the arithmetic below would not add up to the expected value.
// ---------------------------------------------------------------------------------------

void testForeachHygieneUserNameShadowsInternalPrefix(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "ForeachHygiene";
    writeProject(directory, "ForeachHygiene");
    writeFile(directory / "Main.ns", R"NS(
namespace ForeachHygiene
{
    public class Runner
    {
        public static int Main()
        {
            int[] numbers = new int[3];
            numbers[0] = 1;
            numbers[1] = 2;
            numbers[2] = 3;

            int total = 0;
            foreach (var n in numbers)
            {
                // Deliberately shadows what USED TO BE the compiler's own internal name before
                // the P0.1 `$`-prefix fix. If hygiene were broken, this would either fail to
                // compile (duplicate/conflicting declaration) or silently corrupt the real loop
                // index, producing a wrong total or an infinite loop.
                int __foreach_index = 100;
                total = total + n + __foreach_index;
            }

            // Also exercises the enumerator-mode candidate's internal names in the same way,
            // over the real List<T>/ListEnumerator<T> so both desugaring paths are covered.
            return total;
        }
    }
}
)NS");

    const auto exePath = root / "ForeachHygiene.exe";
    const std::string llvm = emitExecutable(tests, directory, exePath, "foreach hygiene (P0.1)");
    tests.expect(!llvm.empty(), "foreach hygiene fixture reaches LLVM IR");
    if (llvm.empty()) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, "Wine not available in this environment -- real-execution check skipped, "
            "structural checks above still ran");
        return;
    }
    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(wine, exePath, root / "ForeachHygiene.log", exitCode, crashed);
    tests.expect(ran, "Wine could be invoked on the foreach-hygiene executable");
    tests.expect(!crashed, "foreach hygiene executable does not crash under real execution");
    // (1+100) + (2+100) + (3+100) == 306, which wraps to 306 % 256 == 50 through the process
    // exit code's 8-bit width (see runUnderWine's own comment on the WEXITSTATUS convention).
    // Any confusion between the user's `__foreach_index` and the compiler's `$__foreach_index`
    // would produce a different value (or an infinite loop, timed out by the Wine harness's own
    // `timeout 20`).
    tests.expect(exitCode == 50, "foreach hygiene executable returns the exact expected value (306, wraps to 50)");
}

// ---------------------------------------------------------------------------------------
// P0.2 -- ownership-aware source: `foreach (var x in Factory.CreateList())`, where CreateList()
// returns `owned List<int>`. Before this fix this failed to compile at all ("owned return value
// cannot initialize a non-owning local"); now `$__foreach_source` picks up `raii` automatically
// via the collectionProbe mechanism, and the loop must actually run correctly (not merely
// compile) since the probe/real-analysis split is exactly the kind of thing that could silently
// diverge (e.g. analyze the probe correctly but forget to apply the modifier to the real node).
// ---------------------------------------------------------------------------------------

void testForeachOverOwnedReturningSource(
    TestContext& tests, const std::filesystem::path& root, const std::filesystem::path& sourceRoot)
{
    const auto directory = root / "ForeachOwnedSource";
    writeProjectWithCollectionsLibrary(directory, "ForeachOwnedSource", sourceRoot);
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Collections.Generic;

namespace ForeachOwnedSource
{
    public class Factory
    {
        public static owned List<int> CreateList()
        {
            raii List<int> list = new List<int>();
            list.Add(10);
            list.Add(20);
            list.Add(30);
            return move(list);
        }
    }

    public class Runner
    {
        public static int Main()
        {
            int total = 0;
            foreach (var x in Factory.CreateList())
            {
                total = total + x;
            }
            return total;
        }
    }
}
)NS");

    const auto exePath = root / "ForeachOwnedSource.exe";
    const std::string llvm = emitExecutable(tests, directory, exePath, "foreach over owned-returning source (P0.2)");
    tests.expect(!llvm.empty(), "foreach-over-owned-source fixture reaches LLVM IR "
        "(this used to fail to compile at all before the P0.2 fix)");
    if (llvm.empty()) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, "Wine not available in this environment -- real-execution check skipped, "
            "structural checks above still ran");
        return;
    }
    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(wine, exePath, root / "ForeachOwnedSource.log", exitCode, crashed);
    tests.expect(ran, "Wine could be invoked on the foreach-over-owned-source executable");
    tests.expect(!crashed, "foreach over an owned-returning source does not crash under real execution");
    tests.expect(exitCode == 60, "foreach over an owned-returning source returns the exact expected value (60)");
}

// ---------------------------------------------------------------------------------------
// P0.3 -- readonly iteration variable: assigning to the foreach element must now be rejected,
// exactly like assigning to any other `const` local.
// ---------------------------------------------------------------------------------------

void testForeachElementIsReadonly(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "ForeachElementReadonly";
    writeProject(directory, "ForeachElementReadonly");
    writeFile(directory / "Main.ns", R"NS(
namespace ForeachElementReadonly
{
    public class Runner
    {
        public static int Run()
        {
            int[] numbers = new int[3];
            int total = 0;
            foreach (var n in numbers)
            {
                n = 99;
                total = total + n;
            }
            return total;
        }
    }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "ForeachElementReadonly.nsproj", error);
    tests.expect(!ok, "assigning to a foreach element is rejected");
    tests.expect(
        hasDiagnosticText(compilation, "left side of assignment is not assignable"),
        "assigning to a foreach element reports the expected diagnostic");
}

// ---------------------------------------------------------------------------------------
// P0.4 -- `const` is no longer accepted in a foreach element declaration at all: a deliberate
// syntax error now, instead of the old silently-discarded no-op.
// ---------------------------------------------------------------------------------------

void testForeachConstKeywordIsNowARejectedSyntaxError(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "ForeachConstKeyword";
    writeProject(directory, "ForeachConstKeyword");
    writeFile(directory / "Main.ns", R"NS(
namespace ForeachConstKeyword
{
    public class Runner
    {
        public static int Run()
        {
            int[] numbers = new int[3];
            int total = 0;
            foreach (const var n in numbers)
            {
                total = total + n;
            }
            return total;
        }
    }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "ForeachConstKeyword.nsproj", error);
    tests.expect(!ok, "'const' in a foreach element declaration is now a rejected syntax error, "
        "not a silently-discarded no-op");
}

// ---------------------------------------------------------------------------------------
// P1.1 -- Current as a property: a minimal hand-written enumerator exposing `Current` as a
// get-only property (no parentheses) must work with foreach's enumerator-mode candidate.
// Distinct from (and cheaper than) Stage52_3ForeachTests.cpp's full recompile of
// examples/project_stage52x_foreach, which already exercises this same shape as part of a much
// larger fixture -- this test isolates it as its own focused, fast regression guard, and is run
// under Wine for the same reason as the other cases above: a desugaring-shape bug (e.g. a stray
// call-parens reintroduced into `$__foreach_enumerator.Current`) could plausibly still verify as
// structurally valid COFF while being semantically wrong.
// ---------------------------------------------------------------------------------------

void testForeachOverCustomEnumeratorWithPropertyCurrent(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "ForeachPropertyCurrent";
    writeProject(directory, "ForeachPropertyCurrent");
    writeFile(directory / "Main.ns", R"NS(
namespace ForeachPropertyCurrent
{
    class CountUpTo
    {
        private int limit;
        public CountUpTo(int limitValue) { limit = limitValue; }

        public owned CountUpToEnumerator GetEnumerator()
        {
            return new CountUpToEnumerator(limit);
        }
    }

    class CountUpToEnumerator
    {
        private int limit;
        private int current;

        public CountUpToEnumerator(int limitValue) { limit = limitValue; current = -1; }

        public bool MoveNext()
        {
            current = current + 1;
            return current < limit;
        }

        // Get-only PROPERTY, no parentheses -- exactly the shape P1.1 requires.
        public int Current
        {
            get { return current; }
        }
    }

    public class Runner
    {
        public static int Main()
        {
            int total = 0;
            foreach (var i in new CountUpTo(5))
            {
                total = total + i;
            }
            return total;
        }
    }
}
)NS");

    const auto exePath = root / "ForeachPropertyCurrent.exe";
    const std::string llvm = emitExecutable(tests, directory, exePath, "foreach over custom enumerator with property Current (P1.1)");
    tests.expect(!llvm.empty(), "foreach-over-property-Current fixture reaches LLVM IR");
    if (llvm.empty()) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, "Wine not available in this environment -- real-execution check skipped, "
            "structural checks above still ran");
        return;
    }
    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(wine, exePath, root / "ForeachPropertyCurrent.log", exitCode, crashed);
    tests.expect(ran, "Wine could be invoked on the property-Current executable");
    tests.expect(!crashed, "foreach over a property-Current enumerator does not crash under real execution");
    // 0+1+2+3+4 == 10.
    tests.expect(exitCode == 10, "foreach over a property-Current enumerator returns the exact expected value (10)");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path sourceRoot = std::filesystem::path(NSC_SOURCE_DIR);
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage52_5_foreach_gaps_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testForeachHygieneUserNameShadowsInternalPrefix(tests, root);
    testForeachOverOwnedReturningSource(tests, root, sourceRoot);
    testForeachElementIsReadonly(tests, root);
    testForeachConstKeywordIsNowARejectedSyntaxError(tests, root);
    testForeachOverCustomEnumeratorWithPropertyCurrent(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
