// Stage 52.6 -- P1.2 of STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md: extending the
// GetEnumerator()/enumerator-mode foreach pattern from List<T> (52.x) to the rest of
// NativeSharp.Core.Collections.Generic: Queue<T>, Stack<T>, LinkedList<T>, HashSet<T>,
// SortedSet<T>, PriorityQueue<TElement,TPriority> -- each gets its own top-level XxxEnumerator<T>
// class (this compiler cannot express nested types, see ListEnumerator.ns's class comment) plus a
// `public owned XxxEnumerator<T> GetEnumerator()` method, following the exact design List<T>/
// ListEnumerator<T> already established and this closure round already verified end-to-end.
//
// Dictionary<TKey,TValue> now publishes a snapshot enumerator. NativeSharp's aggregate ABI can
// materialize KeyValuePair<TKey,TValue> values, while the enumerator keeps the pair fields as
// non-owning aliases into the dictionary. The dictionary must therefore outlive the loop.
// Direct foreach and the Keys()/Values() alternatives are both guarded below.
//
// Real execution (Wine, when available) is used throughout, for the same reason as every other
// Stage 52.x closure round in this repo: a desugaring/wiring bug here (wrong iteration order,
// wrong element count, wrong ownership on the enumerator's borrowed source) would very plausibly
// still produce a structurally valid COFF object while being observably wrong -- see
// Stage52_4OwnedCleanupRuntimeTests.cpp's own header comment for the general rationale.

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

// Duplicated across the Stage52.x test files per this codebase's established "duplication over
// shared helpers between independent test files" convention (see
// claude/05_metodo_de_trabajo_y_archivo_historico.md).
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
// Positive: foreach over each of the six newly-enumerable containers, all in one fixture (so a
// single Wine run covers all six), checking real iteration ORDER (not just "does it compile" or
// "does it not crash") -- FIFO for Queue<T>, top-to-bottom for Stack<T>, insertion order for
// LinkedList<T>, sorted order for SortedSet<T>, and element COUNT for HashSet<T>/PriorityQueue<>
// (whose internal order is unspecified by design, mirroring real C#).
// ---------------------------------------------------------------------------------------

void testForeachOverEachNewlyEnumerableCollection(
    TestContext& tests, const std::filesystem::path& root, const std::filesystem::path& sourceRoot)
{
    const auto directory = root / "ForeachCollections";
    writeProjectWithCollectionsLibrary(directory, "ForeachCollections", sourceRoot);
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Collections.Generic;

namespace ForeachCollections
{
    public class Runner
    {
        private static int HashInt(int value)
        {
            return Hashing.Int32(value);
        }

        public static int Main()
        {
            int total = 0;

            // Queue<T>: FIFO order (oldest-enqueued first) -- 1+2+3 == 6.
            raii Queue<int> q = new Queue<int>();
            q.Enqueue(1); q.Enqueue(2); q.Enqueue(3);
            foreach (var x in q) { total = total + x; }

            // Stack<T>: top-to-bottom order (most-recently-pushed first) -- checked digit by
            // digit (30,20,10 -> 302010), not just summed, so a wrong order would be caught.
            raii Stack<int> s = new Stack<int>();
            s.Push(10); s.Push(20); s.Push(30);
            int stackTotal = 0;
            foreach (var x in s) { stackTotal = stackTotal * 100 + x; }
            total = total + stackTotal;

            // LinkedList<T>: insertion order via AddLast -- 100+200+300 == 600.
            raii LinkedList<int> ll = new LinkedList<int>();
            ll.AddLast(100); ll.AddLast(200); ll.AddLast(300);
            foreach (var x in ll) { total = total + x; }

            // HashSet<T>: order is unspecified by design (same as real C#) -- only the single
            // element's VALUE is checked, not an order-dependent accumulation.
            HashFunc<int> intHash = HashInt;
            raii HashSet<int> hs = new HashSet<int>(intHash);
            hs.Add(1000);
            foreach (var x in hs) { total = total + x; }

            // SortedSet<T>: always sorted order regardless of insertion order -- inserted
            // 3,1,2, must iterate 1,2,3 (ssTotal == 123, not 312 or any other permutation).
            raii SortedSet<int> ss = new SortedSet<int>();
            ss.Add(3); ss.Add(1); ss.Add(2);
            int ssTotal = 0;
            foreach (var x in ss) { ssTotal = ssTotal * 10 + x; }
            total = total + ssTotal;

            // PriorityQueue<TElement,TPriority>: Current yields TElement only, in unspecified
            // heap-internal order (matching real C#) -- only the element COUNT is checked.
            raii PriorityQueue<int, int> pq = new PriorityQueue<int, int>();
            pq.Enqueue(7, 7);
            pq.Enqueue(3, 3);
            pq.Enqueue(5, 5);
            int pqCount = 0;
            foreach (var x in pq) { pqCount = pqCount + 1; }
            total = total + pqCount;

            return total;
        }
    }
}
)NS");

    const auto exePath = root / "ForeachCollections.exe";
    const std::string llvm = emitExecutable(tests, directory, exePath, "foreach over Queue/Stack/LinkedList/HashSet/SortedSet/PriorityQueue (P1.2)");
    tests.expect(!llvm.empty(), "foreach-over-new-collections fixture reaches LLVM IR");
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
    const bool ran = runUnderWine(wine, exePath, root / "ForeachCollections.log", exitCode, crashed);
    tests.expect(ran, "Wine could be invoked on the foreach-over-new-collections executable");
    tests.expect(!crashed, "foreach over the six newly-enumerable collections does not crash under real execution");
    // 6 (queue) + 302010 (stack, order-checked) + 600 (linked list) + 1000 (hash set) +
    // 123 (sorted set, order-checked) + 3 (priority queue count) == 303742, which wraps to
    // 303742 % 256 == 126 through the process exit code's 8-bit width.
    tests.expect(exitCode == 126,
        "foreach over the six newly-enumerable collections returns the exact expected value "
        "(303742, wraps to 126) -- confirms real iteration order/count, not just successful compilation");
}

// ---------------------------------------------------------------------------------------
// Positive: Dictionary<TKey,TValue> exposes a snapshot GetEnumerator() and direct foreach resolves
// through the ordinary enumerator protocol.
// ---------------------------------------------------------------------------------------

void testForeachOverDictionaryUsesGetEnumerator(
    TestContext& tests, const std::filesystem::path& root, const std::filesystem::path& sourceRoot)
{
    const auto directory = root / "ForeachDictionaryMissing";
    writeProjectWithCollectionsLibrary(directory, "ForeachDictionaryMissing", sourceRoot);
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Collections.Generic;

namespace ForeachDictionaryMissing
{
    public class Runner
    {
        private static int HashInt(int value)
        {
            return Hashing.Int32(value);
        }

        public static int Run()
        {
            HashFunc<int> intHash = HashInt;
            raii Dictionary<int, int> dict = new Dictionary<int, int>(intHash);
            dict.Add(1, 100);
            int total = 0;
            foreach (var kv in dict) { total = total + 1; }
            return total;
        }
    }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "ForeachDictionaryMissing.nsproj", error);
    tests.expect(ok, "foreach directly over a Dictionary<TKey,TValue> resolves its snapshot enumerator");
    tests.expect(
        !hasDiagnosticText(compilation, "no compatible member named 'GetEnumerator'"),
        "foreach over a Dictionary no longer reports the retired missing-enumerator diagnostic");
}

// ---------------------------------------------------------------------------------------
// Positive: the documented alternative -- `foreach (var key in dict.Keys())` -- already works
// today with ZERO additional code, since Keys()/Values() are plain single-type-parameter arrays
// and arrays are already foreach-compatible via the array-mode candidate.
// ---------------------------------------------------------------------------------------

void testForeachOverDictionaryKeysAndValues(
    TestContext& tests, const std::filesystem::path& root, const std::filesystem::path& sourceRoot)
{
    const auto directory = root / "ForeachDictionaryKeysValues";
    writeProjectWithCollectionsLibrary(directory, "ForeachDictionaryKeysValues", sourceRoot);
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Collections.Generic;

namespace ForeachDictionaryKeysValues
{
    public class Runner
    {
        private static int HashInt(int value)
        {
            return Hashing.Int32(value);
        }

        public static int Main()
        {
            HashFunc<int> intHash = HashInt;
            raii Dictionary<int, int> dict = new Dictionary<int, int>(intHash);
            dict.Add(1, 100);
            int total = 0;
            foreach (var key in dict.Keys()) { total = total + key; }
            foreach (var value in dict.Values()) { total = total + value; }
            return total;
        }
    }
}
)NS");

    const auto exePath = root / "ForeachDictionaryKeysValues.exe";
    const std::string llvm = emitExecutable(tests, directory, exePath, "foreach over Dictionary.Keys()/Values() (P1.2 documented alternative)");
    tests.expect(!llvm.empty(), "foreach-over-dictionary-keys-values fixture reaches LLVM IR");
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
    const bool ran = runUnderWine(wine, exePath, root / "ForeachDictionaryKeysValues.log", exitCode, crashed);
    tests.expect(ran, "Wine could be invoked on the foreach-over-dictionary-keys-values executable");
    tests.expect(!crashed, "foreach over Dictionary.Keys()/Values() does not crash under real execution");
    // 1 (the one key) + 100 (its value) == 101.
    tests.expect(exitCode == 101,
        "foreach over Dictionary.Keys()/Values() returns the exact expected value (101)");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path sourceRoot = std::filesystem::path(NSC_SOURCE_DIR);
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage52_6_foreach_collections_enumerators_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testForeachOverEachNewlyEnumerableCollection(tests, root, sourceRoot);
    testForeachOverDictionaryUsesGetEnumerator(tests, root, sourceRoot);
    testForeachOverDictionaryKeysAndValues(tests, root, sourceRoot);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
