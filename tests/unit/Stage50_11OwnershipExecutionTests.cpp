// Bloque 1 (Ownership soundness) -- ownership EXECUTION suite.
//
// Every ownership obligation here is verified by RUNNING a real Windows x64 PE and COUNTING how
// many times a destructor actually ran, at -O0 and -O2. A count is the only observation that
// separates the two opposite failure modes: 0 means the object leaked, 2 means it was destroyed
// twice. "Compiles" and "does not crash" distinguish neither, and every ownership bug found while
// writing this file was invisible to both.
//
// Why this file exists: an audit of this project's own suite found only 27 of 112 unit-test files
// execute anything, and Stage 50's ownership work was covered exclusively by IR-shape assertions
// (Stage50_8OwnershipIrLoweringTests.cpp asserts that "owned-parameter cleanups reuse the existing
// Win64 exceptional cleanup machinery" by matching IR text; Stage49RaiiMoveSemanticsTests.cpp
// freezes literal counts of RAII owners in HIR/MIR). None of that can tell whether an owner is
// actually destroyed. Probing the ten Bloque 1 items by execution found four real defects that had
// been shipping:
//
//  1. `owned T x = ...` as a LOCAL was parsed, type-checked, compiled and run -- and then never
//     destroyed. The `owned` qualifier on a local was computed by semantic analysis and silently
//     discarded (only its Borrowed sibling was ever consulted), so the local was treated as an
//     ordinary non-owning binding. A silent leak of a declared language feature.
//  2. Copying one `owned` local into another (`owned T b = a;`) was ACCEPTED, creating two owners
//     of one object -- a double-free hole. The identical `raii` code had always been rejected.
//  3. An `owned` local could not be used as a `move(...)` source, so the project's own Stage 50
//     syntax sample (`owned Resource local = new Resource(); return move(local);`) parsed but was
//     rejected by semantic analysis. Ownership could not be returned from a function that used the
//     `owned` spelling.
//  4. Move-assignment into a live owner (`a = move(b)`) destroyed a's old value correctly and then
//     LEAKED the value moved in: it was never destroyed at scope exit. This one was self-inflicted
//     -- a per-slot "already destroyed" flag introduced for EH bug B-006 -- and it is why that flag
//     no longer exists; a slot legitimately holds a sequence of owners over its lifetime, so a
//     one-way flag on the slot is the wrong shape. See emitCleanupGuardBranch's comment.
//
// Ownership through CONTAINER SLOTS was the other half of Bloque 1, and it landed in two very
// different states worth keeping straight:
//
//  * `owned` FIELDS are now implemented and asserted below. They used to be declarable and
//    unusable -- the syntax parsed, but no assignment target other than a RAII local or an array
//    element admitted a move, so nothing could ever be stored in one. A class now RELEASES the
//    fields it owns from its generated destruction entry, walking its inheritance chain, and
//    overwriting a live owned field releases the value it displaces (the same rule move-assignment
//    on a local has always followed).
//  * ARRAY elements were reported by an earlier version of this suite as "accepted and then
//    silently leaked". That was WRONG and is retracted: arrays in this language are manually
//    managed by design (`new[]`/`delete[]`, `delete` per element -- exactly what `~List()` does),
//    and with the discharge the model asks for the counts are exact. The earlier probe simply
//    never discharged. Corrected in BUGS_ENCONTRADOS/bug_ownership_en_slots_de_contenedor.md.
//
//  * PROPERTIES and INDEXERS can now carry `owned`, and the scenarios at the end of this file
//    assert both halves of what that means: writing through the setter transfers ownership, and
//    reading through the getter yields a borrow that cannot be laundered back into ownership.
//    Before this, the getter of an `owned` property was rejected outright, so the declaration was
//    unwritable and `List<T>` had to expose a read-only indexer plus `SetAt(int, owned T)`.
//
// What genuinely remains open there: a plain non-owner reference assigned into a slot that later
// discharges is accepted with no ownership check -- a dangling-alias hole the collections library
// itself names. It is tracked in that same document and is NOT asserted here, because asserting
// today's behaviour would encode a defect as correct.

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

// The owned-string scenario needs a real heap-allocated string, and the only honest producer is
// the library's String.Copy (literals are constant globals and are rejected as owning-store RHS),
// so that one scenario references the real NativeSharp.Core project.
void writeProjectWithCoreReference(const std::filesystem::path& directory, std::string_view name)
{
    const std::filesystem::path core =
        std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj";
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup>"
            "<ItemGroup><ProjectReference Include=\"" + core.string() + "\" /></ItemGroup></Project>");
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

int runUnderWine(const std::string& wine, const std::filesystem::path& exePath)
{
    const std::string command = "WINEDEBUG=-all timeout 25 " + wine + " " + exePath.string() +
        " >/dev/null 2>&1";
    const int raw = std::system(command.c_str());
#ifdef WIFEXITED
    return WIFEXITED(raw) ? WEXITSTATUS(raw) : (raw & 0xFF);
#else
    return raw & 0xFF;
#endif
}

void compileAndRun(
    TestContext& tests,
    const std::filesystem::path& directory,
    std::string_view optimizationFlag,
    std::string_view label,
    int expected)
{
    const std::string projectText = (directory / (directory.filename().string() + ".nsproj")).string();
    const auto exePath = directory / (std::string(optimizationFlag.substr(1)) + ".exe");
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, optimizationFlag, "-o", exeText};

    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    tests.expect(result == ExitCode::Success, std::string(label) + ": compiles (" + err.str() + ")");
    if (result != ExitCode::Success) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string(label) + ": Wine unavailable -- execution skipped");
        return;
    }
    const int observed = runUnderWine(wine, exePath);
    tests.expect(
        observed == expected,
        std::string(label) + ": observed " + std::to_string(observed) + ", expected " +
            std::to_string(expected));
}

void run(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    int expected)
{
    const auto directory = root / std::string(name);
    writeProject(directory, name);
    writeFile(directory / "Main.ns", source);
    compileAndRun(tests, directory, "-O0", std::string(name) + " [-O0]", expected);
    compileAndRun(tests, directory, "-O2", std::string(name) + " [-O2]", expected);
}

// A program that MUST be rejected, with a diagnostic that actually names the problem. Ownership
// errors are only useful if they are caught at compile time -- once a double-ownership program
// links, nothing downstream can recover.
void reject(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view expectedFragment)
{
    const auto directory = root / std::string(name);
    writeProject(directory, name);
    writeFile(directory / "Main.ns", source);
    const std::string projectText = (directory / (directory.filename().string() + ".nsproj")).string();
    const std::array<std::string_view, 2> args = {"--check-project-oop-hir", projectText};

    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    tests.expect(result != ExitCode::Success, std::string(name) + ": is rejected at compile time");
    const std::string diagnostics = err.str() + out.str();
    tests.expect(
        diagnostics.find(std::string(expectedFragment)) != std::string::npos,
        std::string(name) + ": diagnostic mentions '" + std::string(expectedFragment) + "' (got: " +
            diagnostics.substr(0, 200) + ")");
}

// `Resource` reports its own destruction count through a static, which is what every scenario
// returns. `Tag()` lets a scenario also prove WHICH object survived a transfer, not merely how
// many were destroyed -- a move that destroyed the right number of objects but kept the wrong one
// would otherwise pass.
constexpr std::string_view kPrelude = R"NS(
namespace NativeSharp.Core { public class Exception { public string Message; } }
using NativeSharp.Core;

class Resource
{
    private int tag;
    private static int destructions;
    public Resource(int initial) { tag = initial; }
    ~Resource() { destructions = destructions + 1; }
    public int Tag() { return tag; }
    public static int Destructions() { return destructions; }
}
)NS";

// --- item 3/4/8: owners are destroyed exactly once, and only the surviving owner destroys -------

void testOwnedLocalIsDestroyed(TestContext& tests, const std::filesystem::path& root)
{
    // Defect 1: this returned 0 -- the `owned` qualifier on a local was discarded and the object
    // was never destroyed. `raii` in the same position always worked, which is what made the gap
    // invisible: both spellings were documented, only one of them owned anything.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Scope() { owned Resource local = new Resource(1); }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "OwnedLocalIsDestroyed", source, 1);
}

void testRaiiLocalIsDestroyed(TestContext& tests, const std::filesystem::path& root)
{
    // Control for the scenario above: the two spellings must agree.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Scope() { raii Resource local = new Resource(1); }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "RaiiLocalIsDestroyed", source, 1);
}

void testTwoOwnersEachDestroyedOnce(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Scope()
        {
            raii Resource first = new Resource(1);
            owned Resource second = new Resource(2);
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "TwoOwnersEachDestroyedOnce", source, 2);
}

void testMovedFromSourceIsNotDestroyedAgain(TestContext& tests, const std::filesystem::path& root)
{
    // The moved-from source must not destroy what it no longer owns: 2 here would be a double free.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Sink(owned Resource value) { }
        public static void Scope()
        {
            raii Resource local = new Resource(1);
            Sink(move(local));
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "MovedFromSourceNotDestroyedAgain", source, 1);
}

void testMoveChainThroughThreeOwners(TestContext& tests, const std::filesystem::path& root)
{
    // Ownership relayed through three consecutive owned parameters: exactly one destruction, at the
    // end of the chain. Any intermediate frame destroying it would give 2 or more.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Sink(owned Resource value) { }
        public static void Middle(owned Resource value) { Sink(move(value)); }
        public static void Outer(owned Resource value) { Middle(move(value)); }
        public static void Scope()
        {
            raii Resource local = new Resource(1);
            Outer(move(local));
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "MoveChainThroughThreeOwners", source, 1);
}

// --- item 7: return ownership ---------------------------------------------------------------

void testReturnOwnershipFromOwnedLocal(TestContext& tests, const std::filesystem::path& root)
{
    // Defect 3: this did not compile -- an `owned` local was not accepted as a `move(...)` source,
    // so the shape the project's own Stage 50 syntax sample documents could not be built.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static owned Resource Make()
        {
            owned Resource local = new Resource(1);
            return move(local);
        }
        public static void Scope() { raii Resource received = Make(); }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "ReturnOwnershipFromOwnedLocal", source, 1);
}

void testOwnedLocalMovedIntoOwnedLocal(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Scope()
        {
            owned Resource first = new Resource(1);
            owned Resource second = move(first);
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "OwnedLocalMovedIntoOwnedLocal", source, 1);
}

// --- item 3: move-assignment over a LIVE owner ------------------------------------------------

void testMoveAssignmentOverLiveOwner(TestContext& tests, const std::filesystem::path& root)
{
    // Defect 4, and the scenario that pins all three obligations of a move-assignment at once by
    // encoding them in one number: inside the scope, the surviving owner must hold the object that
    // was moved IN (tag 2, not 1) and exactly one destruction must have happened already (a's old
    // value); after the scope, the total must be 2. Reported as tag*10 + count, then *10 + final:
    //   212 = correct.
    //   211 = the moved-in object never destroyed (the leak this suite was written against).
    //   112 = the wrong object survived the assignment.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static int Scope()
        {
            raii Resource first = new Resource(1);
            raii Resource second = new Resource(2);
            first = move(second);
            return first.Tag() * 10 + Resource.Destructions();
        }
        public static int Main()
        {
            int inScope = Scope();
            return inScope * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "MoveAssignmentOverLiveOwner", source, 212);
}

void testMoveAssignmentOverLiveOwnedLocal(TestContext& tests, const std::filesystem::path& root)
{
    // The `owned` spelling of the same obligation. Kept separate because the two spellings reached
    // the owner machinery by different routes until this round, and a regression could hit one
    // without the other.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static int Scope()
        {
            owned Resource first = new Resource(1);
            owned Resource second = new Resource(2);
            first = move(second);
            return first.Tag() * 10 + Resource.Destructions();
        }
        public static int Main()
        {
            int inScope = Scope();
            return inScope * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "MoveAssignmentOverLiveOwnedLocal", source, 212);
}

// --- item 8: destruction traversal order ------------------------------------------------------

void testNestedScopesDestroyInReverseOrder(TestContext& tests, const std::filesystem::path& root)
{
    // Reverse construction order, checked by observation rather than by inspecting MIR: the inner
    // owner must be destroyed while the outer one is still alive. Encoded so that the wrong order
    // produces a different number instead of the same total.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        private static int innerSeenAt;
        public static void Scope()
        {
            raii Resource outer = new Resource(1);
            if (outer.Tag() == 1)
            {
                raii Resource inner = new Resource(2);
            }
            innerSeenAt = Resource.Destructions();
        }
        public static int Main()
        {
            Scope();
            return innerSeenAt * 10 + Resource.Destructions();
        }
    }
)NS";
    // After the inner block: 1 destruction (the inner owner). After the whole scope: 2.
    run(tests, root, "NestedScopesDestroyInReverseOrder", source, 12);
}

// --- items 5/6/10: the negative half ----------------------------------------------------------

void testCopyingAnOwnedLocalIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // Defect 2: this was ACCEPTED, producing two owners of one object. The `raii` spelling of the
    // identical program had always been rejected, which is what made the hole easy to miss.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static int Main()
        {
            owned Resource first = new Resource(1);
            owned Resource second = first;
            return 0;
        }
    }
)NS";
    reject(tests, root, "CopyingAnOwnedLocalIsRejected", source, "cannot be copied");
}

void testCopyingARaiiLocalIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static int Main()
        {
            raii Resource first = new Resource(1);
            raii Resource second = first;
            return 0;
        }
    }
)NS";
    reject(tests, root, "CopyingARaiiLocalIsRejected", source, "cannot be copied");
}

void testUseAfterMoveOfOwnedLocalIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Sink(owned Resource value) { }
        public static int Main()
        {
            owned Resource local = new Resource(1);
            Sink(move(local));
            return local.Tag();
        }
    }
)NS";
    reject(tests, root, "UseAfterMoveOfOwnedLocalIsRejected", source, "move");
}

void testUseAfterMoveOfRaiiLocalIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Sink(owned Resource value) { }
        public static int Main()
        {
            raii Resource local = new Resource(1);
            Sink(move(local));
            return local.Tag();
        }
    }
)NS";
    reject(tests, root, "UseAfterMoveOfRaiiLocalIsRejected", source, "move");
}

void testOwnedLocalRequiresAnInitializer(TestContext& tests, const std::filesystem::path& root)
{
    // An owner with no initializer has nothing to own and no defined destruction obligation. The
    // diagnostic must name the spelling the user actually wrote -- reporting "raii local" for an
    // `owned` declaration sends them looking at the wrong keyword.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static int Main()
        {
            owned Resource local;
            return 0;
        }
    }
)NS";
    reject(tests, root, "OwnedLocalRequiresAnInitializer", source, "owned local requires an initializer");
}

// --- item 1: owning field storage --------------------------------------------------------------

void testOwnedFieldIsDestroyedWithItsContainer(TestContext& tests, const std::filesystem::path& root)
{
    // Before this round `public owned Resource Slot;` parsed and type-checked -- Stage 50's syntax
    // test asserts exactly that -- but no assignment target other than a RAII local or an array
    // element admitted a move, so nothing could ever be put into the field. The feature was
    // declarable and unusable. Admitting the target is only half of it: the container now RELEASES
    // what it owns from its generated destruction entry, or the transfer would just relocate a leak.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource Slot; }

    class Program
    {
        public static void Scope()
        {
            raii Resource local = new Resource(1);
            raii Holder holder = new Holder();
            holder.Slot = move(local);
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "OwnedFieldDestroyedWithContainer", source, 1);
}

void testUnassignedOwnedFieldDestroysNothing(TestContext& tests, const std::filesystem::path& root)
{
    // The traversal is guarded by each field's null state, so a container whose owned field was
    // never filled must not try to release anything. A crash or a non-zero count here would mean
    // the guard is missing.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource Slot; }

    class Program
    {
        public static void Scope() { raii Holder holder = new Holder(); }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "UnassignedOwnedFieldDestroysNothing", source, 0);
}

void testOwnedFieldStaysUsableWhileContainerLives(TestContext& tests, const std::filesystem::path& root)
{
    // Proves the transfer really moved the object into the field rather than merely satisfying the
    // checker: the field is read back through the container, and nothing has been destroyed yet.
    // 70 = tag 7 still readable, 0 destructions so far.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource Slot; }

    class Program
    {
        public static int Scope()
        {
            raii Resource local = new Resource(7);
            raii Holder holder = new Holder();
            holder.Slot = move(local);
            return holder.Slot.Tag() * 10 + Resource.Destructions();
        }
        public static int Main() { return Scope(); }
    }
)NS";
    run(tests, root, "OwnedFieldUsableWhileContainerLives", source, 70);
}

void testOwnedFieldInheritedFromBaseIsDestroyed(TestContext& tests, const std::filesystem::path& root)
{
    // The traversal walks the inheritance chain, so a field a DERIVED object inherits is released
    // too. Without the chain walk this returns 0 -- a leak that only shows up with inheritance.
    const std::string source = std::string(kPrelude) + R"NS(
    class Base { public owned Resource Slot; }
    class Derived : Base { public int Extra; }

    class Program
    {
        public static void Scope()
        {
            raii Resource local = new Resource(1);
            raii Derived derived = new Derived();
            derived.Slot = move(local);
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "OwnedFieldInheritedIsDestroyed", source, 1);
}

void testOwnedFieldAssignedThroughImplicitThis(TestContext& tests, const std::filesystem::path& root)
{
    // The common shape: a method takes ownership and parks it in its own field, referring to the
    // field without `this.`. Exercises a different name-binding path than `holder.Slot` above.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        public owned Resource Slot;
        public void Take(owned Resource value) { Slot = move(value); }
    }

    class Program
    {
        public static void Scope()
        {
            raii Resource local = new Resource(1);
            raii Holder holder = new Holder();
            holder.Take(move(local));
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "OwnedFieldAssignedThroughThis", source, 1);
}

void testOverwritingALiveOwnedFieldReleasesTheOldValue(
    TestContext& tests, const std::filesystem::path& root)
{
    // The obligation that makes the feature safe rather than merely usable, and the one this
    // implementation got wrong first: moving into a field that ALREADY owns something must release
    // the displaced value, or the overwrite silently leaks it. Same encoding as
    // MoveAssignmentOverLiveOwner, so the failure modes stay distinguishable:
    //   212 = correct.
    //   201 = the displaced value was never released (the leak this scenario was written against).
    //   112 = the wrong object survived the overwrite.
    //
    // The rule is not invented here -- it is exactly what `a = move(b)` on a local has always done.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource Slot; }

    class Program
    {
        public static int Scope()
        {
            raii Resource first = new Resource(1);
            raii Resource second = new Resource(2);
            raii Holder holder = new Holder();
            holder.Slot = move(first);
            holder.Slot = move(second);
            return holder.Slot.Tag() * 10 + Resource.Destructions();
        }
        public static int Main()
        {
            int inScope = Scope();
            return inScope * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "OverwritingLiveOwnedFieldReleasesOld", source, 212);
}

// --- item 9: ownership-aware properties and indexers -------------------------------------------
//
// `owned` on a property or indexer is deliberately ASYMMETRIC, and these scenarios exist to pin
// that asymmetry down by execution rather than by comment:
//
//   * writing through the setter TRANSFERS ownership (`h.Slot = move(a)`), so from then on the
//     container is the only thing that destroys the object;
//   * reading through the getter yields a NON-OWNING PLAIN REFERENCE -- deliberately not a formal
//     `borrowed`, which would require provable lifetime provenance a field read cannot offer --
//     so reading repeatedly destroys nothing extra and the result cannot be captured as an owner.
//
// The alternative -- a getter that hands out ownership -- is not a lowering gap, it is unsound:
// the object would still be holding the same pointer in a field its own destruction entry
// releases. See ClassMemberSymbol::ownsAccessorStorage.

void testOwnedPropertySetterTakesOwnership(TestContext& tests, const std::filesystem::path& root)
{
    // The declaration that used to be impossible to write. Before item 9 the GETTER was rejected
    // ("owned return requires an ownership-transfer expression...") and, past that, the call site
    // was rejected too ("move(...) is only valid for a direct RAII local or owned parameter...").
    // 21 = the object tagged 2 survived the transfer and was destroyed exactly once.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private owned Resource backing;
        public owned Resource Slot { get { return backing; } set { backing = move(value); } }
    }

    class Program
    {
        public static int Scope()
        {
            raii Resource local = new Resource(2);
            raii Holder holder = new Holder();
            holder.Slot = move(local);
            return holder.Slot.Tag();
        }
        public static int Main()
        {
            int surviving = Scope();
            return surviving * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "OwnedPropertySetterTakesOwnership", source, 21);
}

void testAutomaticOwnedPropertyTakesOwnership(TestContext& tests, const std::filesystem::path& root)
{
    // `{ get; set; }` on an `owned` property must generate `backing = move(value);`, not
    // `backing = value;`. Both the generated backing field and the implicit `value` parameter are
    // owners, so a plain assignment between them is a copy of an owner: the caller's object would
    // end up with two, and the setter's own owned-parameter cleanup would free it on the way out
    // while the field kept pointing at it.
    //
    // That was measured, not assumed. The hand-written equivalent (`set { backing = value; }`)
    // compiled and reported 22 -- destroyed twice -- which is how the missing copy-owner rule for
    // an owned PARAMETER source was found; see CopyingAnOwnedParameterByAssignmentRejected. It is
    // now a compile error, so this scenario and that one guard the same hole from both sides: the
    // generated body transfers, and the non-transferring body no longer compiles.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource Slot { get; set; } }

    class Program
    {
        public static int Scope()
        {
            raii Resource local = new Resource(2);
            raii Holder holder = new Holder();
            holder.Slot = move(local);
            return holder.Slot.Tag();
        }
        public static int Main()
        {
            int surviving = Scope();
            return surviving * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "AutomaticOwnedPropertyTakesOwnership", source, 21);
}

void testOwnedIndexerSetterTakesOwnership(TestContext& tests, const std::filesystem::path& root)
{
    // The shape the collections library actually wants: `list[i] = move(item)`. Until item 9 this
    // could only be spelled as a separate `SetAt(int, owned T)` method, which is why `List<T>`
    // exposes a read-only indexer to this day.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private owned Resource backing;
        public owned Resource this[int index] { get { return backing; } set { backing = move(value); } }
    }

    class Program
    {
        public static int Scope()
        {
            raii Resource local = new Resource(2);
            raii Holder holder = new Holder();
            holder[0] = move(local);
            return holder[0].Tag();
        }
        public static int Main()
        {
            int surviving = Scope();
            return surviving * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "OwnedIndexerSetterTakesOwnership", source, 21);
}

void testOwnedPropertyGetterHandsOutABorrow(TestContext& tests, const std::filesystem::path& root)
{
    // Reading an `owned` property yields a plain non-owning reference (NOT a formal `borrowed`
    // with lifetime tracking): three reads, still exactly one destruction. If the
    // getter transferred ownership instead, each read would produce another owner and this would
    // report several destructions of one allocation -- or crash, once the frees overlapped.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private owned Resource backing;
        public owned Resource Slot { get { return backing; } set { backing = move(value); } }
    }

    class Program
    {
        public static int Scope()
        {
            raii Resource local = new Resource(3);
            raii Holder holder = new Holder();
            holder.Slot = move(local);
            Resource first = holder.Slot;
            Resource second = holder.Slot;
            return first.Tag() + second.Tag() + holder.Slot.Tag();
        }
        public static int Main()
        {
            int sum = Scope();
            return sum * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "OwnedPropertyGetterHandsOutBorrow", source, 91);
}

void testOverwritingALiveOwnedPropertyReleasesTheOldValue(
    TestContext& tests, const std::filesystem::path& root)
{
    // Same obligation as the owned-FIELD overwrite, reached through a setter call instead of a
    // direct field store: the displaced owner has to be released. It holds here for free precisely
    // because the setter's body is an ordinary owned-field move-assignment -- which is the point of
    // routing the feature through the field machinery instead of giving properties their own.
    //   212 = correct. 201 = the displaced value leaked. 112 = the wrong object survived.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private owned Resource backing;
        public owned Resource Slot { get { return backing; } set { backing = move(value); } }
    }

    class Program
    {
        public static int Scope()
        {
            raii Resource first = new Resource(1);
            raii Resource second = new Resource(2);
            raii Holder holder = new Holder();
            holder.Slot = move(first);
            holder.Slot = move(second);
            return holder.Slot.Tag() * 10 + Resource.Destructions();
        }
        public static int Main()
        {
            int inScope = Scope();
            return inScope * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "OverwritingLiveOwnedPropertyReleasesOld", source, 212);
}

void testCopyingAnOwnerThroughAnOwnedSetterIsRejected(
    TestContext& tests, const std::filesystem::path& root)
{
    // Widening the setter into a move(...) destination must not widen it into a COPY destination.
    // `holder.Slot = local` without move would hand the container a pointer the local still owns.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private owned Resource backing;
        public owned Resource Slot { get { return backing; } set { backing = move(value); } }
    }

    class Program
    {
        public static int Main()
        {
            raii Resource local = new Resource(1);
            raii Holder holder = new Holder();
            holder.Slot = local;
            return 0;
        }
    }
)NS";
    reject(
        tests, root, "CopyingOwnerThroughOwnedSetterRejected", source,
        "cannot be copied by assignment");
}

void testUseAfterMoveThroughAnOwnedSetterIsRejected(
    TestContext& tests, const std::filesystem::path& root)
{
    // The transfer really moves: the source is moved-from afterwards, exactly as after
    // `other = move(local)`. Without this the "transfer" would be an aliasing copy that merely
    // happens to be spelled with move().
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private owned Resource backing;
        public owned Resource Slot { get { return backing; } set { backing = move(value); } }
    }

    class Program
    {
        public static int Main()
        {
            raii Resource local = new Resource(1);
            raii Holder holder = new Holder();
            holder.Slot = move(local);
            return local.Tag();
        }
    }
)NS";
    reject(
        tests, root, "UseAfterMoveThroughOwnedSetterRejected", source,
        "use of moved-from RAII local");
}

void testOwnedPropertyGetterCannotBeCapturedAsAnOwner(
    TestContext& tests, const std::filesystem::path& root)
{
    // The other half of "the getter hands out a plain reference": the caller cannot launder that
    // reference back into ownership by binding it to a RAII local. Were this accepted, the local
    // and the container would both destroy the same object.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private owned Resource backing;
        public owned Resource Slot { get { return backing; } set { backing = move(value); } }
    }

    class Program
    {
        public static int Main()
        {
            raii Resource local = new Resource(1);
            raii Holder holder = new Holder();
            holder.Slot = move(local);
            raii Resource stolen = holder.Slot;
            return 0;
        }
    }
)NS";
    reject(
        tests, root, "OwnedPropertyGetterCannotBeCapturedAsOwner", source,
        "RAII ownership cannot be copied or borrowed");
}

void testMoveIntoAnUnqualifiedSetterIsStillRejected(
    TestContext& tests, const std::filesystem::path& root)
{
    // The widening is scoped to `owned` properties only. A property that never claimed ownership
    // must still refuse a move, or every setter in the language would silently become a transfer
    // destination and quietly consume its argument.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private Resource backing;
        public Resource Slot { get { return backing; } set { backing = value; } }
    }

    class Program
    {
        public static int Main()
        {
            raii Resource local = new Resource(1);
            raii Holder holder = new Holder();
            holder.Slot = move(local);
            return 0;
        }
    }
)NS";
    reject(
        tests, root, "MoveIntoUnqualifiedSetterRejected", source,
        "move(...) is only valid for a direct RAII local or owned parameter");
}

void testStaticOwnedFieldIsRejectedAtItsDeclaration(
    TestContext& tests, const std::filesystem::path& root)
{
    // Ownership is discharged by a destruction entry, and a static field has no containing object
    // and no end of life -- so an `owned` static could only ever be a leak wearing an ownership
    // annotation. What makes this worth a scenario is WHERE the error lands: before the check, the
    // declaration was accepted and the first `backing = move(value)` failed with "move(...) is only
    // valid for a direct RAII local or owned parameter...", pointing at the transfer and never
    // mentioning the `static` that actually caused it. The assertion is on the message naming the
    // static field, not merely on rejection happening.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private static owned Resource backing;
        public static owned Resource Slot { get { return backing; } set { backing = move(value); } }
    }

    class Program
    {
        public static int Main()
        {
            raii Resource local = new Resource(1);
            Holder.Slot = move(local);
            return 0;
        }
    }
)NS";
    reject(
        tests, root, "StaticOwnedFieldRejected", source,
        "static field 'backing' cannot be owned because no destruction ever releases it");
}

// --- item 10: the negative half of the ownership contract --------------------------------------
//
// Every rule below was already implemented and every one of them was, until now, asserted by
// nothing but the implementation that enforces it. That is the exact shape of the failures this
// project keeps finding: a rule nobody re-checks is a rule that can be deleted by an unrelated
// refactor and stay deleted, because the positive scenarios all still pass -- a compiler that
// stops rejecting bad programs looks perfectly healthy from the outside.
//
// So these assert on the MESSAGE, not merely on non-zero exit. A rejection that happens for the
// wrong reason (a parse error, an unrelated diagnostic) is not the rule holding, and an assertion
// that cannot tell the difference would keep passing while the rule rotted away.

void testBorrowedFieldIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // A field outlives whatever local or parameter the borrow came from, so a borrowed field is a
    // dangling pointer with a lifetime annotation on it.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { private borrowed Resource slot; }
    class Program { public static int Main() { return 0; } }
)NS";
    reject(tests, root, "BorrowedFieldRejected", source, "borrowed field 'slot' is rejected");
}

void testBorrowedReturnOfALocalOwnerIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // The local is destroyed by its own cleanup on the way out, so the borrow handed back to the
    // caller points at freed memory before the caller's first instruction runs.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static borrowed Resource Leak() { raii Resource local = new Resource(1); return local; }
        public static int Main() { return 0; }
    }
)NS";
    reject(
        tests, root, "BorrowedReturnOutlivesLocalOwner", source,
        "borrowed return value outlives local owner 'local'");
}

void testStoringABorrowedValueInAFieldIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // Same lifetime argument as the borrowed field, reached from the other direction: the field is
    // unqualified, but what is being stored into it is a borrow.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        public Resource slot;
        public void Keep(borrowed Resource value) { slot = value; }
    }
    class Program { public static int Main() { return 0; } }
)NS";
    reject(
        tests, root, "BorrowedValueStoredInFieldRejected", source,
        "borrowed value cannot be stored in field 'slot'");
}

void testStoringABorrowedValueThroughASetterIsRejected(
    TestContext& tests, const std::filesystem::path& root)
{
    // The property/indexer path has to close the same hole the direct field store closes -- a
    // setter is just a field store with a method call in front of it, and retention past the
    // borrow's lifetime cannot be proven from the call site.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private Resource backing;
        public Resource Slot { get { return backing; } set { backing = value; } }
        public void Keep(borrowed Resource value) { Slot = value; }
    }
    class Program { public static int Main() { return 0; } }
)NS";
    reject(
        tests, root, "BorrowedValueStoredThroughSetterRejected", source,
        "borrowed value cannot be stored through a property/indexer setter");
}

void testReturningARaiiOwnerWithoutMoveIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // Returning the owner by name would leave the local's cleanup to destroy the object the caller
    // just received. The diagnostic names the fix (`return move(owner)`) because the correct
    // program is one token away and the wrong one is a use-after-free.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static owned Resource Make() { raii Resource local = new Resource(1); return local; }
        public static int Main() { return 0; }
    }
)NS";
    reject(
        tests, root, "ReturningRaiiOwnerWithoutMoveRejected", source,
        "returning RAII owner 'local' requires return move(owner)");
}

void testReturningAnOwnedParameterWithoutMoveIsRejected(
    TestContext& tests, const std::filesystem::path& root)
{
    // The `owned` spelling of the same rule. Both spellings must reject it, or the two ways of
    // saying "this is an owner" would not mean the same thing.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static owned Resource Forward(owned Resource value) { return value; }
        public static int Main() { return 0; }
    }
)NS";
    reject(
        tests, root, "ReturningOwnedParameterWithoutMoveRejected", source,
        "returning owned parameter 'value' requires return move(owner)");
}

void testSelfMoveAssignmentIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // `a = move(a)` would destroy a's current value as the displaced owner and then store that same
    // freed pointer back into the slot -- the one case where the release-then-store rule turns on
    // itself.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static int Main() { raii Resource a = new Resource(1); a = move(a); return 0; }
    }
)NS";
    reject(tests, root, "SelfMoveAssignmentRejected", source, "self move-assignment of a RAII local");
}

void testOverwritingAnOwnerWithALiveBorrowIsRejected(
    TestContext& tests, const std::filesystem::path& root)
{
    // Move-assignment destroys the displaced value, which would leave any outstanding borrow of it
    // dangling. This is the rule that makes borrows usable at all: a borrow is only safe if the
    // owner cannot be replaced underneath it.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static int Main()
        {
            raii Resource a = new Resource(1);
            borrowed Resource b = a;
            a = move(new Resource(2));
            return b.Tag();
        }
    }
)NS";
    reject(
        tests, root, "OverwritingOwnerWithLiveBorrowRejected", source,
        "cannot overwrite owner 'a' while a borrowed local is alive");
}

void testOwnedReturnWithoutATransferIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // Declaring an owned return promises the caller a new obligation. Handing back a plain field
    // read promises it while the object it came from still owns the same pointer -- two owners.
    // This is exactly the diagnostic that made `owned` properties undeclarable until item 9; it
    // still fires, correctly, for an ordinary method.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public Resource plain; }
    class Program
    {
        public static owned Resource Take(Holder holder) { return holder.plain; }
        public static int Main() { return 0; }
    }
)NS";
    reject(
        tests, root, "OwnedReturnWithoutTransferRejected", source,
        "owned return requires an ownership-transfer expression");
}

void testBorrowedValueEscapingAnUnqualifiedReturnIsRejected(
    TestContext& tests, const std::filesystem::path& root)
{
    // A borrow may not be laundered into an unqualified return: the caller would receive something
    // with a lifetime it was never told about and has no way to respect.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static Resource Escape(borrowed Resource value) { return value; }
        public static int Main() { return 0; }
    }
)NS";
    reject(
        tests, root, "BorrowedEscapesUnqualifiedReturnRejected", source,
        "borrowed value cannot escape through an unqualified return type");
}

void testMovingFromAPlainLocalIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // move() transfers an obligation, so there has to be one to transfer. A plain local never owned
    // the object, and accepting this would manufacture an owner out of an alias -- the source would
    // keep being usable while a new owner took responsibility for freeing it.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static int Main()
        {
            Resource plain = new Resource(1);
            raii Resource owner = move(plain);
            return 0;
        }
    }
)NS";
    reject(
        tests, root, "MoveFromPlainLocalRejected", source,
        "move(...) source must be a RAII local or owned parameter");
}

void testCopyingAnOwnedParameterByAssignmentIsRejected(
    TestContext& tests, const std::filesystem::path& root)
{
    // The hole the owned-field work of this same block opened, found by mutating the automatic
    // property setter rather than by reading code: an owned PARAMETER copied into a field was
    // accepted, and the program then destroyed the object twice (measured: 22 where 21 is
    // correct). The parameter's end-of-scope cleanup and the field's release both fired on one
    // allocation.
    //
    // Every other destination already rejected it -- owned parameter into a RAII local, owned
    // parameter into an owned argument, owner local into a field. Only "owned parameter into a
    // field" was missing, which is exactly the store that had just become reachable. This scenario
    // is here so the rule cannot quietly go missing again the next time a new destination opens.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        public owned Resource slot;
        public void Take(owned Resource value) { slot = value; }
    }
    class Program { public static int Main() { return 0; } }
)NS";
    reject(
        tests, root, "CopyingAnOwnedParameterByAssignmentRejected", source,
        "cannot copy owned value 'value' by assignment; use move(value)");
}

// --- Bloque 1 closure: the owning-slot contract -------------------------------------------------
//
// The audit's central observation was that ownership rules lived on the SYNTAX of each assignment
// rather than on the storage: the displaced-value release fired only when the RHS was textually
// `move(...)`, so `h.slot = null` leaked (measured: 0 destructions), `h.slot = new Res(2)` leaked
// the displaced owner (measured: 1 of 2), and `h.slot = ownedCall()` leaked it too. These
// scenarios pin the unified contract: EVERY write to an owning slot releases what it displaces,
// every RHS must be an ownership-transfer expression, `delete` on an owning slot is a compile
// error, move-out empties the slot, and the destruction order is a frozen, executed contract.

void testOwnedFieldOverwriteWithNullReleases(TestContext& tests, const std::filesystem::path& root)
{
    // `slot = null` is the language's Clear operation: release what the slot holds, leave it
    // empty. Before the unification this reported 0 -- the object was simply lost.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource slot; }

    class Program
    {
        public static void Scope()
        {
            raii Resource local = new Resource(1);
            raii Holder holder = new Holder();
            holder.slot = move(local);
            holder.slot = null;
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "OwnedFieldOverwriteWithNullReleases", source, 1);
}

void testOwnedFieldOverwriteWithNewReleasesOld(TestContext& tests, const std::filesystem::path& root)
{
    // `new T(...)` is an ownership-transfer expression -- the same status it has always had in
    // owned returns and owned-local initialization -- so storing one into a live slot must
    // release the displaced owner. 212 = the new object survived in the slot (tag 2), exactly one
    // destruction inside the scope (the displaced tag 1), and both destroyed by the end.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource slot; }

    class Program
    {
        public static int Scope()
        {
            raii Resource local = new Resource(1);
            raii Holder holder = new Holder();
            holder.slot = move(local);
            holder.slot = new Resource(2);
            return holder.slot.Tag() * 10 + Resource.Destructions();
        }
        public static int Main()
        {
            int inScope = Scope();
            return inScope * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "OwnedFieldOverwriteWithNewReleasesOld", source, 212);
}

void testOwnedFieldOverwriteWithOwnedCallReleasesOld(
    TestContext& tests, const std::filesystem::path& root)
{
    // The third transfer form: an owned-returning call. Also leaked the displaced value before
    // the unification (measured: 1 of 2).
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource slot; }

    class Program
    {
        public static owned Resource Make(int tag) { return new Resource(tag); }
        public static void Scope()
        {
            raii Holder holder = new Holder();
            holder.slot = Make(1);
            holder.slot = Make(2);
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "OwnedFieldOverwriteWithOwnedCallReleasesOld", source, 2);
}

void testMoveOutOfOwnedFieldTransfers(TestContext& tests, const std::filesystem::path& root)
{
    // move(holder.slot): the slot was an ownership SINK before this existed -- values could be
    // transferred in but never taken back out. 21 = the taken object is alive and readable (tag
    // 2), destroyed exactly once by its new owner; the vacated slot contributed nothing when the
    // holder died.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource slot; }

    class Program
    {
        public static int Scope()
        {
            raii Resource local = new Resource(2);
            raii Holder holder = new Holder();
            holder.slot = move(local);
            raii Resource taken = move(holder.slot);
            return taken.Tag();
        }
        public static int Main()
        {
            int surviving = Scope();
            return surviving * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "MoveOutOfOwnedFieldTransfers", source, 21);
}

void testTakeMethodReturnsOwnershipFromImplicitThisField(
    TestContext& tests, const std::filesystem::path& root)
{
    // The container idiom the feature exists for: `return move(slot)` through the implicit this.
    // This is exactly the Take()/Dequeue() shape array-backed containers already have via
    // move(items[i]), now expressible for a single owned field.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private owned Resource slot;
        public void Put(owned Resource value) { slot = move(value); }
        public owned Resource Take() { return move(slot); }
    }

    class Program
    {
        public static int Scope()
        {
            raii Resource local = new Resource(2);
            raii Holder holder = new Holder();
            holder.Put(move(local));
            raii Resource taken = holder.Take();
            return taken.Tag();
        }
        public static int Main()
        {
            int surviving = Scope();
            return surviving * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "TakeMethodReturnsOwnershipFromImplicitThisField", source, 21);
}

void testSecondMoveOutOfEmptiedSlotYieldsNull(TestContext& tests, const std::filesystem::path& root)
{
    // The vacated slot holds null, so a second move-out hands over an empty owner and destroys
    // nothing extra -- 1 destruction total for 1 object, however many times the slot is drained.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource slot; }

    class Program
    {
        public static void Scope()
        {
            raii Resource local = new Resource(1);
            raii Holder holder = new Holder();
            holder.slot = move(local);
            raii Resource taken = move(holder.slot);
            raii Resource empty = move(holder.slot);
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "SecondMoveOutOfEmptiedSlotYieldsNull", source, 1);
}

void testMoveInOutInCycleDestroysEachOnce(TestContext& tests, const std::filesystem::path& root)
{
    // A slot legitimately hosts a SEQUENCE of owners: in, out, in again. Two objects, two
    // destructions -- the same invariant move-assignment on locals has always kept.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource slot; }

    class Program
    {
        public static void Scope()
        {
            raii Resource first = new Resource(1);
            raii Resource second = new Resource(2);
            raii Holder holder = new Holder();
            holder.slot = move(first);
            raii Resource taken = move(holder.slot);
            holder.slot = move(second);
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "MoveInOutInCycleDestroysEachOnce", source, 2);
}

void testOwnedStringFieldOverwriteReleasesThroughStringPath(
    TestContext& tests, const std::filesystem::path& root)
{
    // A native string owns a flat allocation with NO class descriptor. The overwrite release used
    // to dispatch the displaced value through a descriptor read unconditionally -- measured as a
    // crash (exit 5) at -O0 and -O2 -- while the final-destruction traversal already forked on
    // isNativeString. One slot type, one destruction semantics: the overwrite now takes the same
    // fork. String literals are CONSTANT GLOBALS and are rejected as owning-store RHS (releasing
    // one would free static memory), which is why this scenario allocates through String.Copy --
    // an owned-returning call, the honest spelling.
    const std::string source = R"NS(
using NativeSharp.Core;
namespace OwnedStringOverwrite
{
    class Holder { public owned string text; }

    class Program
    {
        public static int Main()
        {
            raii Holder holder = new Holder();
            owned string first = String.Copy("first");
            owned string second = String.Copy("SECOND");
            holder.text = move(first);
            holder.text = move(second);
            if (holder.text.Length != 6) { return 90; }
            return 0;
        }
    }
}
)NS";
    const auto directory = root / "OwnedStringFieldOverwrite";
    writeProjectWithCoreReference(directory, "OwnedStringFieldOverwrite");
    writeFile(directory / "Main.ns", source);
    compileAndRun(tests, directory, "-O0", "OwnedStringFieldOverwrite [-O0]", 0);
    compileAndRun(tests, directory, "-O2", "OwnedStringFieldOverwrite [-O2]", 0);
}

void testDestructionOrderContractInterleaves(TestContext& tests, const std::filesystem::path& root)
{
    // THE destruction-order contract, observed by execution (the audit was explicit that a
    // comment is not a contract and an IR dump is not an observation):
    //
    //     derived destructor body -> derived owned fields (reverse decl) -> base body -> base fields
    //
    // Encoded positionally in base 4 so the single exit code captures both presence AND order:
    // body ids 1 (Derived) / 0 (Base), field tags 3 (derived's) / 2 (base's).
    // Expected: 1,3,0,2 -> 1*64 + 3*16 + 0*4 + 2 = 114. Any other order is a different number --
    // the old implementation (all bodies first, then fields base-first) produced 75.
    const std::string source = R"NS(
namespace NativeSharp.Core { public class Exception { public string Message; } }
using NativeSharp.Core;

class Log
{
    private static int seq;
    public static void Step(int id) { seq = seq * 4 + id; }
    public static int Value() { return seq; }
}

class Tracked
{
    private int id;
    public Tracked(int initial) { id = initial; }
    ~Tracked() { Log.Step(id); }
}

class Base
{
    public owned Tracked baseField;
    ~Base() { Log.Step(0); }
}

class Derived : Base
{
    public owned Tracked derivedField;
    ~Derived() { Log.Step(1); }
}

class Program
{
    public static void Scope()
    {
        raii Tracked forBase = new Tracked(2);
        raii Tracked forDerived = new Tracked(3);
        raii Derived derived = new Derived();
        derived.baseField = move(forBase);
        derived.derivedField = move(forDerived);
    }
    public static int Main() { Scope(); return Log.Value(); }
}
)NS";
    run(tests, root, "DestructionOrderContractInterleaves", source, 114);
}

void testDestructionOrderReverseDeclarationWithinClass(
    TestContext& tests, const std::filesystem::path& root)
{
    // Within one class: destructor body first (its fields are still alive for it to read), then
    // fields in REVERSE declaration order -- destruction is the reverse of construction, so a
    // later-declared field may safely reference an earlier-declared one from its destructor.
    // Expected: body(1), second(3), first(2) -> 1*16 + 3*4 + 2 = 30.
    const std::string source = R"NS(
namespace NativeSharp.Core { public class Exception { public string Message; } }
using NativeSharp.Core;

class Log
{
    private static int seq;
    public static void Step(int id) { seq = seq * 4 + id; }
    public static int Value() { return seq; }
}

class Tracked
{
    private int id;
    public Tracked(int initial) { id = initial; }
    ~Tracked() { Log.Step(id); }
}

class Holder
{
    public owned Tracked first;
    public owned Tracked second;
    ~Holder() { Log.Step(1); }
}

class Program
{
    public static void Scope()
    {
        raii Tracked a = new Tracked(2);
        raii Tracked b = new Tracked(3);
        raii Holder holder = new Holder();
        holder.first = move(a);
        holder.second = move(b);
    }
    public static int Main() { Scope(); return Log.Value(); }
}
)NS";
    run(tests, root, "DestructionOrderReverseDeclarationWithinClass", source, 30);
}

void testPlainAliasIntoOwnedFieldIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // The audit's "convertir una referencia no-owner en ownership efectivo": a plain reference
    // stored into an owning slot silently made the container the destroyer of an object somebody
    // else still references. Every RHS reaching an owning field must be a transfer expression.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource slot; }

    class Program
    {
        public static int Main()
        {
            Resource plain = new Resource(1);
            raii Holder holder = new Holder();
            holder.slot = plain;
            return 0;
        }
    }
)NS";
    reject(
        tests, root, "PlainAliasIntoOwnedFieldRejected", source,
        "requires an ownership-transfer expression");
}

void testDeleteOfOwnedFieldIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // Measured before the rule: `delete h.slot` destroyed the object AND left the pointer in the
    // slot, so the container's destruction entry released it again -- 2 destructions of 1 object,
    // a compiler-guaranteed double free. Prohibited outright (the audit's recommended option):
    // the honest spellings are `= null` to clear, overwrite to replace, move(...) to take.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned Resource slot; }

    class Program
    {
        public static int Main()
        {
            raii Resource local = new Resource(1);
            raii Holder holder = new Holder();
            holder.slot = move(local);
            delete holder.slot;
            return 0;
        }
    }
)NS";
    reject(
        tests, root, "DeleteOfOwnedFieldRejected", source,
        "explicit delete of owned field 'slot' is forbidden");
}

void testStringLiteralIntoOwnedStringFieldIsRejected(
    TestContext& tests, const std::filesystem::path& root)
{
    // A string literal lowers to a CONSTANT GLOBAL (deduplicated, static storage). Storing one
    // into an owning slot would eventually feed that static pointer to the deallocator -- so it
    // is not an ownership-transfer expression and the store is rejected. The working spellings
    // are move(ownedString), String.Copy(...) (an owned-returning call), or null.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder { public owned string text; }

    class Program
    {
        public static int Main()
        {
            raii Holder holder = new Holder();
            holder.text = "static storage";
            return 0;
        }
    }
)NS";
    reject(
        tests, root, "StringLiteralIntoOwnedStringFieldRejected", source,
        "requires an ownership-transfer expression");
}

void testMoveOutOfPropertyIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // move(...) takes from STORAGE it can null out. A property read is a call, not storage --
    // there is no slot for move to vacate -- so the container must expose an explicit
    // Take()-shaped method that does the move-out on its own field.
    const std::string source = std::string(kPrelude) + R"NS(
    class Holder
    {
        private owned Resource backing;
        public owned Resource Slot { get { return backing; } set { backing = move(value); } }
    }

    class Program
    {
        public static int Main()
        {
            raii Resource local = new Resource(1);
            raii Holder holder = new Holder();
            holder.Slot = move(local);
            raii Resource taken = move(holder.Slot);
            return 0;
        }
    }
)NS";
    reject(
        tests, root, "MoveOutOfPropertyRejected", source,
        "move(...) member source must be an assignable owned field");
}

// --- SHR-005: el residuo, y era el ARREGLO FIJO -----------------------------------------------
//
// EL HUECO, medido. `arrayElementMoveTarget` y la comprobacion de fuente de `move(...)` decidian
// las dos con `ProjectTypeConversion::isNativeArray`, que compara el SUFIJO "[]". Un arreglo fijo
// (`Resource[2]`) termina en "2]", asi que no era ninguna de las dos cosas:
//
//   * `slots[i] = move(x)`  -- rechazado, porque el destino no admitia un move
//   * `move(slots[i])`      -- rechazado, "must be a direct native array element access"
//
// Y `slots[i] = alias` se aceptaba SIN comprobar nada, que es el lado silencioso del mismo hueco.
// No era una exclusion escrita en ningun sitio: era el sufijo de una cadena. Doc 68 nombra el
// arreglo fijo explicitamente como lo que quedaba de SHR-005.
//
// POR QUE ES EL MISMO CASO Y NO UNO NUEVO. Lo unico que la bajada necesita de este ligado es la
// DIRECCION de la ranura, y el HIR ya la sabe dar para un elemento de arreglo fijo (caso
// RawElement, "C2.1 fixed arrays"). `MoveArrayElement` recibe esa direccion y hace el
// load-and-null de siempre, gobernado por `elementIsReferenceLike`, que sale del tipo del
// ELEMENTO. Un arreglo fijo de un tipo por valor sigue siendo el no-op que ya era.
//
// EL CONTRATO ES EL DEL ARREGLO NATIVO, no el del campo `owned`: nada destruye los elementos
// automaticamente. Un arreglo fijo es almacenamiento en linea sin longitud -- el nivel crudo del
// lenguaje, el mismo donde viven `*p` y `&x` -- y su contenido lo gobierna quien lo escribio,
// igual que `move(items[i])` en un `T[]` desde Stage 52. Lo que el move SI garantiza es que la
// ranura queda en null, para que una lectura posterior no vea un valor ya transferido.

void testMoveIntoAndOutOfAFixedArraySlot(TestContext& tests, const std::filesystem::path& root)
{
    // 71 = el objeto transferido sigue vivo y legible (tag 7) y se destruye EXACTAMENTE UNA VEZ,
    // en su nuevo duenno. Si el move de entrada no transfiriera, `first` tambien lo destruiria y
    // esto seria 72.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static int Scope()
        {
            raii Resource first = new Resource(7);
            Resource[2] slots;
            slots[0] = null;
            slots[1] = null;
            slots[0] = move(first);
            raii Resource taken = move(slots[0]);
            return taken.Tag();
        }
        public static int Main()
        {
            int surviving = Scope();
            return surviving * 10 + Resource.Destructions();
        }
    }
)NS";
    run(tests, root, "MoveIntoAndOutOfAFixedArraySlot", source, 71);
}

void testSecondMoveOutOfAFixedArraySlotYieldsNull(TestContext& tests, const std::filesystem::path& root)
{
    // La ranura vaciada queda en NULL, que es lo unico que impide el doble free. Un objeto, una
    // destruccion, por muchas veces que se drene la ranura -- la misma invariante que ya tiene el
    // campo `owned` (SecondMoveOutOfEmptiedSlotYieldsNull).
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Scope()
        {
            raii Resource first = new Resource(1);
            Resource[2] slots;
            slots[0] = null;
            slots[1] = null;
            slots[0] = move(first);
            raii Resource taken = move(slots[0]);
            raii Resource empty = move(slots[0]);
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "SecondMoveOutOfAFixedArraySlotYieldsNull", source, 1);
}

void testRelocationBetweenTwoFixedArraySlots(TestContext& tests, const std::filesystem::path& root)
{
    // La operacion que el arreglo NATIVO ya tenia desde Stage 52 -- `grown[i] = move(items[i])`,
    // reubicar un elemento de un almacenamiento a otro -- escrita sobre arreglos fijos. Dos
    // objetos, dos destrucciones: ninguna ranura se queda con un alias del otro.
    const std::string source = std::string(kPrelude) + R"NS(
    class Program
    {
        public static void Scope()
        {
            raii Resource a = new Resource(1);
            raii Resource b = new Resource(2);
            Resource[2] source;
            Resource[2] target;
            source[0] = null;
            source[1] = null;
            target[0] = null;
            target[1] = null;
            source[0] = move(a);
            source[1] = move(b);
            target[0] = move(source[0]);
            target[1] = move(source[1]);
            raii Resource firstOut = move(target[0]);
            raii Resource secondOut = move(target[1]);
        }
        public static int Main() { Scope(); return Resource.Destructions(); }
    }
)NS";
    run(tests, root, "RelocationBetweenTwoFixedArraySlots", source, 2);
}

}

int main()
{
    TestContext tests;
    const auto root =
        std::filesystem::temp_directory_path() / "nativesharp_stage50_11_ownership_execution_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testOwnedLocalIsDestroyed(tests, root);
    testRaiiLocalIsDestroyed(tests, root);
    testTwoOwnersEachDestroyedOnce(tests, root);
    testMovedFromSourceIsNotDestroyedAgain(tests, root);
    testMoveChainThroughThreeOwners(tests, root);
    testReturnOwnershipFromOwnedLocal(tests, root);
    testOwnedLocalMovedIntoOwnedLocal(tests, root);
    testMoveAssignmentOverLiveOwner(tests, root);
    testMoveAssignmentOverLiveOwnedLocal(tests, root);
    testNestedScopesDestroyInReverseOrder(tests, root);

    testCopyingAnOwnedLocalIsRejected(tests, root);
    testCopyingARaiiLocalIsRejected(tests, root);
    testUseAfterMoveOfOwnedLocalIsRejected(tests, root);
    testUseAfterMoveOfRaiiLocalIsRejected(tests, root);
    testOwnedLocalRequiresAnInitializer(tests, root);

    testOwnedFieldIsDestroyedWithItsContainer(tests, root);
    testUnassignedOwnedFieldDestroysNothing(tests, root);
    testOwnedFieldStaysUsableWhileContainerLives(tests, root);
    testOwnedFieldInheritedFromBaseIsDestroyed(tests, root);
    testOwnedFieldAssignedThroughImplicitThis(tests, root);
    testOverwritingALiveOwnedFieldReleasesTheOldValue(tests, root);

    testOwnedPropertySetterTakesOwnership(tests, root);
    testAutomaticOwnedPropertyTakesOwnership(tests, root);
    testOwnedIndexerSetterTakesOwnership(tests, root);
    testOwnedPropertyGetterHandsOutABorrow(tests, root);
    testOverwritingALiveOwnedPropertyReleasesTheOldValue(tests, root);
    testCopyingAnOwnerThroughAnOwnedSetterIsRejected(tests, root);
    testUseAfterMoveThroughAnOwnedSetterIsRejected(tests, root);
    testOwnedPropertyGetterCannotBeCapturedAsAnOwner(tests, root);
    testMoveIntoAnUnqualifiedSetterIsStillRejected(tests, root);
    testStaticOwnedFieldIsRejectedAtItsDeclaration(tests, root);

    testBorrowedFieldIsRejected(tests, root);
    testBorrowedReturnOfALocalOwnerIsRejected(tests, root);
    testStoringABorrowedValueInAFieldIsRejected(tests, root);
    testStoringABorrowedValueThroughASetterIsRejected(tests, root);
    testReturningARaiiOwnerWithoutMoveIsRejected(tests, root);
    testReturningAnOwnedParameterWithoutMoveIsRejected(tests, root);
    testSelfMoveAssignmentIsRejected(tests, root);
    testOverwritingAnOwnerWithALiveBorrowIsRejected(tests, root);
    testOwnedReturnWithoutATransferIsRejected(tests, root);
    testBorrowedValueEscapingAnUnqualifiedReturnIsRejected(tests, root);
    testMovingFromAPlainLocalIsRejected(tests, root);
    testCopyingAnOwnedParameterByAssignmentIsRejected(tests, root);

    testOwnedFieldOverwriteWithNullReleases(tests, root);
    testOwnedFieldOverwriteWithNewReleasesOld(tests, root);
    testOwnedFieldOverwriteWithOwnedCallReleasesOld(tests, root);
    testMoveOutOfOwnedFieldTransfers(tests, root);
    testTakeMethodReturnsOwnershipFromImplicitThisField(tests, root);
    testSecondMoveOutOfEmptiedSlotYieldsNull(tests, root);
    testMoveInOutInCycleDestroysEachOnce(tests, root);
    testOwnedStringFieldOverwriteReleasesThroughStringPath(tests, root);
    testDestructionOrderContractInterleaves(tests, root);
    testDestructionOrderReverseDeclarationWithinClass(tests, root);
    testPlainAliasIntoOwnedFieldIsRejected(tests, root);
    testDeleteOfOwnedFieldIsRejected(tests, root);
    testStringLiteralIntoOwnedStringFieldIsRejected(tests, root);
    testMoveOutOfPropertyIsRejected(tests, root);

    // SHR-005 -- el residuo, y era el arreglo fijo.
    testMoveIntoAndOutOfAFixedArraySlot(tests, root);
    testSecondMoveOutOfAFixedArraySlotYieldsNull(tests, root);
    testRelocationBetweenTwoFixedArraySlots(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
