// Stage 52.4 -- Owned-parameter cleanup correctness, verified through REAL EXECUTION.
//
// Every other Stage 52 test in this suite stops at "compiles, LLVM verifies, COFF is
// structurally valid" -- and that is exactly what let two real, crashing bugs ship undetected
// for a full closure cycle. Both bugs lived in `emitOwnedParameterDestroy`
// (src/LLVM/ProjectOopLlvmLowerer.cpp) and its interaction with the generic collections library
// (lib/NativeSharp.Core.Collections.Generic), and BOTH produced perfectly valid LLVM IR and a
// perfectly valid COFF/PE object -- the corruption only manifested as a page fault when the
// resulting .exe was actually RUN. This file closes that gap: it uses the real `nsc` Driver
// in-process to emit a genuine Windows x64 PE executable, then (when Wine is available on the
// host, gracefully skipped otherwise -- see commandAvailable()/wineAvailable() below, the same
// pattern Stage48_6CppCliHostClosureTests.cpp already uses for optional host-toolchain checks)
// actually EXECUTES it and checks the real exit code, exactly per this repo's Stage 52 closure
// checklist ("no basta con verificar únicamente COFF/PE; este bug fue precisamente invisible a
// la validación estructural").
//
// Bug A -- "Stage 50.8" owned-value-type cleanup (P0 item 2/3 of the closure checklist):
//   emitOwnedParameterDestroy treated EVERY owned parameter's storage as an 8-byte pointer,
//   including an `owned int` parameter backed by a 4-byte `alloca i32`. The resulting
//   unconditional 8-byte load/store corrupted whatever stack memory sat next to that alloca --
//   invisibly, unless the enclosing method also made a nested call (which is what forces the
//   corrupted slot to actually be read back before it's overwritten again). Fixed by an early,
//   no-op return in emitOwnedParameterDestroy for any parameter whose type is not reference-like
//   (a class/interface reference) and not builtin:string. testOwnedValueParameterWithNestedCall
//   below is the minimal repro from the closure checklist itself (Holder.Consume(owned int
//   value) { NestedCall(); }), verified end-to-end including real Wine execution.
//
// Bug B -- owned-parameter-into-owned-array-slot without move() (found while chasing the
//   closure checklist's own P0 item 4, "Stage52Collections.exe debe correr sin crash": bisected
//   down through List<Payload>.Add(), past a false lead ruling out the GetEnumerator()/
//   ListEnumerator<T> circular generic reference, past ruling out generics/nested-calls/loop-vs-
//   constant-index entirely, to the actual root cause): `emitOwnedParameterDestroy` is *correct*
//   to destroy any owned reference-typed parameter that is still non-null when its method
//   returns -- that is the whole point of the owned-parameter contract, mirroring what a `raii`
//   local's own end-of-scope cleanup already does. But `List<T>.Add()` (and, it turned out,
//   Queue<T>.Enqueue/Stack<T>.Push/Dictionary<TKey,TValue>'s InsertNew+AddOrUpdate+SetValue/
//   SortedList<TKey,TValue>'s InsertAt+AddOrUpdate+SetValue/PriorityQueue<TElement,TPriority>.
//   Enqueue/LinkedListNode<T>.SetValue+SetNext -- the entire collections library, audited and
//   fixed together once this pattern was understood) stored the owned parameter into its backing
//   array/field slot with a PLAIN `items[count] = value;`, never `move(value)`. A plain
//   assignment copies the pointer but does not tell the compiler ownership was transferred, so
//   the parameter's own storage is still non-null when Add() returns -- its automatic cleanup
//   then destroys the very object that a copy of the same pointer was just stored into. The
//   result is a genuine double-free/use-after-free: the array slot holds a dangling pointer from
//   the moment Add() returns, and the crash only surfaces later (this repo's real case: three
//   steps later, in an unrelated destructor loop, on Windows/Wine, at a stack address nowhere
//   near the actual bug) whenever that slot is next read and its object's vtable/descriptor is
//   dereferenced. testOwnedReferenceParameterStoredWithMoveRunsCleanly pins down the fix
//   (`move(value)` at the storage site) as the REQUIRED shape, executed for real and checked
//   against the exact expected value.
//
//   BLOQUE 1 UPDATE -- the buggy shape is no longer writable. This header used to say the bug
//   was "a runtime correctness bug, not a compile-time rejection", and its negative test asserted
//   that the buggy program still compiled. Both were wrong, and the mistake was one of framing:
//   the hazard was read as "a plain array-element assignment", which really is legal, when it is
//   actually "an OWNED PARAMETER used as the right-hand side of a store", which creates two
//   owners of one allocation regardless of the destination. Under that reading semantic analysis
//   rejects it precisely -- and every other destination in the language (owned parameter into a
//   RAII local, owned parameter into an owned argument) had been rejecting exactly that all
//   along. Only the store was missing the rule.
//
//   testOwnedReferenceParameterStoredWithoutMoveIsRejected now asserts the compile-time rejection
//   and the exact diagnostic. Together with the positive half, the two remain the regression guard
//   that stops List.ns (or any future owned-array-backed generic container) from reintroducing
//   this double-free -- except that the guard is now the compiler itself rather than a test that
//   watched the program crash.

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
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

// Extracts the LLVM IR text of one function whose mangled name contains `nameFragment` -- same
// helper as Stage52GenericCollectionsCompilerFixesTests.cpp, duplicated here per this codebase's
// established "duplication over shared helpers between independent test files" convention (see
// claude/05_metodo_de_trabajo_y_archivo_historico.md).
std::string extractFunctionBody(const std::string& llvm, std::string_view nameFragment)
{
    std::size_t searchFrom = 0;
    while (true)
    {
        const std::size_t defineAt = llvm.find("\ndefine ", searchFrom);
        if (defineAt == std::string::npos) return {};
        const std::size_t lineEnd = llvm.find('\n', defineAt + 1);
        const std::string headerLine = llvm.substr(defineAt, lineEnd == std::string::npos ? std::string::npos : lineEnd - defineAt);
        const std::size_t bodyEnd = llvm.find("\n}", defineAt);
        if (headerLine.find(nameFragment) != std::string::npos && bodyEnd != std::string::npos)
        {
            return llvm.substr(defineAt, bodyEnd + 2 - defineAt);
        }
        if (bodyEnd == std::string::npos) return {};
        searchFrom = bodyEnd + 2;
    }
}

bool commandAvailable(std::string_view command)
{
#ifdef _WIN32
    return std::system(("where " + std::string(command) + " >nul 2>nul").c_str()) == 0;
#else
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
#endif
}

// This sandbox's Wine is installed at a fixed, non-PATH location (see 52_x_nativesharp_foreach.md
// section 5's evidence table); a real Windows host or a differently-configured Linux host may
// instead have a plain `wine`/`wine64` on PATH. Try both so this test exercises real execution
// wherever it can, and degrades to a structural-only check (never a hard failure) otherwise.
std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}

// Runs `exePath` under Wine with a timeout, returning the process's real exit code (Linux
// WEXITSTATUS convention -- Windows exit codes wrap mod 256 through Wine's process bridge,
// exactly like every other real-execution check in this Stage 52.x closure effort) via
// `exitCode`, and whether Wine itself reported an unhandled native crash (a page fault / access
// violation) via `crashed`. Returns false only if Wine could not be invoked at all.
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

// Compiles `directory`'s project all the way to a verified, real Windows x64 PE executable via
// the actual `nsc` Driver (the exact code path `nsc --emit-project-oop-exe ... -o out.exe` runs
// -- not a hand-rolled subset of it), and also returns the generated LLVM IR text for tests that
// want to inspect specific instructions/absence-of-instructions.
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

    // Also lower to text IR through the same in-process pipeline the other Stage 52 test files
    // use, purely so callers can inspect the generated instructions -- the Driver call above is
    // the actual thing under test (it is what real `nsc` usage runs), this is just introspection.
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
// Bug A / closure checklist P0 item 3: `Holder.Consume(owned int value) { NestedCall(); }`,
// verified end to end -- sema, IR shape (no class-descriptor destruction over the int
// parameter), COFF, and (when Wine is available) the real executable's actual exit code.
// ---------------------------------------------------------------------------------------

void testOwnedValueParameterWithNestedCall(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "OwnedIntNestedCall";
    writeProject(directory, "OwnedIntNestedCall");
    writeFile(directory / "Main.ns", R"NS(
namespace OwnedIntNestedCall
{
    // The exact minimal shape named by the Stage 52 closure checklist (P0 item 3): an `owned`
    // VALUE-typed parameter, consumed by a method that also makes a NESTED call -- the nested
    // call is what forces the corrupted 8-byte-wide stack write (Bug A, pre-fix) to actually be
    // read back before anything else overwrites it, which is why this shape crashed and a
    // flatter one would not have.
    public class Holder
    {
        private int total;

        public void Consume(owned int value)
        {
            NestedCall();
            total = total + value;
        }

        private void NestedCall()
        {
            total = total + 1;
        }

        public int Total() { return total; }
    }

    public class Runner
    {
        public static int Main()
        {
            Holder h = new Holder();
            h.Consume(7);
            h.Consume(7);
            return h.Total();
        }
    }
}
)NS");

    const auto exePath = root / "OwnedIntNestedCall.exe";
    const std::string llvm = emitExecutable(tests, directory, exePath, "owned int + nested call (Bug A)");
    tests.expect(!llvm.empty(), "owned int + nested call fixture reaches LLVM IR");
    if (llvm.empty()) return;

    // Checklist item 4: HIR/MIR/LLVM must not treat the int parameter as reference-like -- the
    // most direct, mechanical way to confirm that from the emitted IR text is that Consume()'s
    // own body contains none of the block-name landmarks emitOwnedParameterDestroy's
    // reference-like cleanup path emits (see the "owned.param.*" names in
    // ProjectOopLlvmLowerer.cpp's emitOwnedParameterDestroy): with the Bug A fix in place, a
    // value-typed owned parameter's cleanup is skipped entirely at codegen time, so these names
    // must never appear for Consume().
    const std::string consumeBody = extractFunctionBody(llvm, "Consume");
    tests.expect(!consumeBody.empty(), "Consume() function body found in the emitted LLVM IR");
    tests.expect(consumeBody.find("owned.param.destroy") == std::string::npos,
        "Consume()'s IR contains no owned-parameter dynamic-destruction block for its int parameter");
    tests.expect(consumeBody.find("owned.param.descriptor") == std::string::npos,
        "Consume()'s IR never loads a class descriptor for its int parameter");

    // Checklist items 5/6: real COFF, and (per this repo's own "no basta con verificar
    // únicamente COFF/PE" lesson) real execution with the real expected exit code. Expected:
    // Consume(7) twice, each call runs NestedCall() (total += 1) then total += value (+= 7) --
    // (1+7) + (1+7) == 16.
    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, "Wine not available in this environment -- real-execution check skipped, "
            "structural checks above still ran (see WINE_CHECKLIST_STAGE52.md priority: real Windows first)");
        return;
    }
    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(wine, exePath, root / "OwnedIntNestedCall.log", exitCode, crashed);
    tests.expect(ran, "Wine could be invoked on the owned-int-nested-call executable");
    tests.expect(!crashed, "owned int + nested call executable does not crash under real execution (Bug A fixed)");
    tests.expect(exitCode == 16, "owned int + nested call executable returns the exact expected exit code (16)");
}

// ---------------------------------------------------------------------------------------
// Bug B, negative half: an owned REFERENCE-typed parameter stored into an owned array slot with
// a PLAIN assignment (no move()) -- the exact anti-pattern List.ns/Queue.ns/Stack.ns/
// Dictionary.ns/SortedList.ns/PriorityQueue.ns/LinkedListNode.ns all had before the Stage 52.4
// fix.
//
// REWRITTEN IN BLOQUE 1, and the reason matters more than the rewrite.
//
// This scenario used to assert that the buggy program "MUST still compile and structurally emit
// (the bug is a genuine runtime double-free, NOT something semantic analysis can reject in
// general -- a plain array-element assignment is legal NativeSharp)", and then tried to observe
// the crash under Wine, with a documented fallback for Wine's heap divergence. That premise was
// wrong. Semantic analysis CAN reject it, and narrowly: the hazard is not "a plain array-element
// assignment", it is specifically an OWNED PARAMETER used as the right-hand side of a store,
// which leaves two owners of one allocation no matter what the destination is. Every other
// destination in the language already rejected exactly that (owned parameter into a RAII local,
// owned parameter into an owned argument); only the store was missing the rule.
//
// So the scenario now asserts the rejection instead of the crash. That is a strictly stronger
// guarantee than what it asserted before: Alberto confirmed on real Windows (2026-08-28) that
// this shape genuinely double-frees, and the whole class of program is now unwritable rather
// than merely documented as dangerous. The Wine-divergence caveat that used to live here is moot
// for this fixture -- there is no executable to run.
//
// The seven library call sites that motivated Stage 52.4 keep their move() spelling, and the
// positive half below still proves that spelling produces exactly one destruction. What changed
// is that writing them the old way is now a compile error rather than a latent double-free.
// ---------------------------------------------------------------------------------------

void testOwnedReferenceParameterStoredWithoutMoveIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "OwnedRefStoredWithoutMove";
    writeProject(directory, "OwnedRefStoredWithoutMove");
    writeFile(directory / "Main.ns", R"NS(
namespace OwnedRefStoredWithoutMove
{
    public class Payload
    {
        public int Value;
    }

    // Mirrors List<T>.Add() BEFORE the Stage 52.4 fix. `items[0] = value;` leaves the `value`
    // PARAMETER's own storage non-null, so Add()'s automatic owned-parameter cleanup destroys the
    // very object just stored -- and the container then frees it again.
    public class BuggyHolder
    {
        private owned Payload[] items;

        public BuggyHolder() { items = new Payload[1]; }

        public void Add(owned Payload value)
        {
            items[0] = value;
        }

        public void UseStoredElement()
        {
            delete items[0];
        }
    }

    public class Runner
    {
        public static int Main()
        {
            BuggyHolder h = new BuggyHolder();
            raii Payload p = new Payload();
            p.Value = 30;
            h.Add(move(p));
            h.UseStoredElement();
            return 9;
        }
    }
}
)NS");

    const std::string projectText = (directory / (directory.filename().string() + ".nsproj")).string();
    const std::array<std::string_view, 2> args = {"--check-project-oop-hir", projectText};

    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    tests.expect(
        result != ExitCode::Success,
        "owned reference parameter stored without move() is rejected at compile time");

    // Assert on the MESSAGE, not merely on failure. A rejection for some unrelated reason would
    // leave the actual hazard unguarded while this check kept passing -- which is the failure mode
    // that let the original bug live in seven library files.
    const std::string diagnostics = err.str() + out.str();
    tests.expect(
        diagnostics.find("cannot copy owned value 'value' by assignment") != std::string::npos,
        "the diagnostic names the copied owner and the fix (got: " + diagnostics.substr(0, 240) + ")");
    tests.expect(
        diagnostics.find("use move(value)") != std::string::npos,
        "the diagnostic points at move(value) as the correction");
}

// ---------------------------------------------------------------------------------------
// Bug B, positive half (the actual fix, and the shape now used throughout
// lib/NativeSharp.Core.Collections.Generic): the SAME shape as above, but storing with
// `move(value)` -- must run cleanly and return the exact expected value, for real, under Wine.
// This is the regression guard against List.ns (or any future owned-array-backed container)
// silently reintroducing the plain-assignment version.
// ---------------------------------------------------------------------------------------

void testOwnedReferenceParameterStoredWithMoveRunsCleanly(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "OwnedRefStoredWithMove";
    writeProject(directory, "OwnedRefStoredWithMove");
    writeFile(directory / "Main.ns", R"NS(
namespace OwnedRefStoredWithMove
{
    public class Payload
    {
        public int Value;
    }

    // The fixed shape -- identical to BuggyHolder above except for one word: move(value).
    public class FixedHolder
    {
        private owned Payload[] items;

        public FixedHolder() { items = new Payload[1]; }

        public void Add(owned Payload value)
        {
            items[0] = move(value);
        }

        public int ReadAndDestroy()
        {
            int value = items[0].Value;
            delete items[0];
            return value;
        }
    }

    public class Runner
    {
        public static int Main()
        {
            FixedHolder h = new FixedHolder();
            raii Payload p = new Payload();
            p.Value = 30;
            h.Add(move(p));
            return h.ReadAndDestroy();
        }
    }
}
)NS");

    const auto exePath = root / "OwnedRefStoredWithMove.exe";
    const std::string llvm = emitExecutable(tests, directory, exePath, "owned reference param stored with move() (Bug B fix)");
    tests.expect(!llvm.empty(), "owned-reference-with-move fixture compiles and reaches LLVM IR");
    if (llvm.empty()) return;

    tests.expect(llvm.find("store ptr null") != std::string::npos,
        "move(value) into the array slot nulls the parameter's own storage, "
        "so its end-of-scope cleanup correctly becomes a no-op");

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, "Wine not available in this environment -- real-execution check skipped, "
            "structural checks above still ran");
        return;
    }
    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(wine, exePath, root / "OwnedRefStoredWithMove.log", exitCode, crashed);
    tests.expect(ran, "Wine could be invoked on the owned-reference-with-move executable");
    tests.expect(!crashed, "owned reference parameter stored with move() does not crash under real execution");
    tests.expect(exitCode == 30, "owned reference parameter stored with move() returns the exact expected value (30)");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage52_4_owned_cleanup_runtime_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testOwnedValueParameterWithNestedCall(tests, root);
    testOwnedReferenceParameterStoredWithoutMoveIsRejected(tests, root);
    testOwnedReferenceParameterStoredWithMoveRunsCleanly(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
