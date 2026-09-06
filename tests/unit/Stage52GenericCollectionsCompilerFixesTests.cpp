// Stage 52 -- Compiler-level fixes required for generic collections.
//
// Stage 52's goal (see claude/01_roadmap_nativesharp_0_2.md) is a practically-complete port of
// C#'s System.Collections.Generic surface: List<T>, Dictionary<TKey,TValue>, HashSet<T>, Queue<T>,
// Stack<T>, LinkedList<T>, SortedList/SortedDictionary/SortedSet, and friends. Writing that library
// as *generic* NativeSharp classes exposed four real compiler gaps -- none of them exotic language
// features, all of them plain wiring gaps between the (pre-existing) ownership system and the
// (pre-existing) generics/monomorphization system, which had simply never been exercised together
// with a value-typed T before. This file is the regression guard for all four fixes, end to end
// through real semantic analysis, HIR, MIR, and LLVM lowering -- exactly the fixes described in
// claude/07... / claude/08_estado_stage52.md.
//
// Fix 1 (delete no-op for value types): a container's Clear()/destructor must be able to run
//   `delete items[i];` uniformly for every T, but a monomorphized T=int has nothing to free.
// Fix 2 (ownership qualifier lost during generic specialization): `owned T value` monomorphized to
//   an UNQUALIFIED parameter once T closed over a reference type, because ProjectGenericModel's
//   cloneType() carried over pointer/array modifiers but not the separate ownershipQualifier field.
// Fix 3 (owned/borrowed validation too strict for value-typed T): once fix 2 correctly propagates
//   `owned T`/`borrowed T` for *every* T, plain value calls like `box.Add(3)` must not be forced
//   through move()/ownership-transfer machinery that only makes sense for reference types.
// Fix 4 (move() from a native array element): Queue<T>.Dequeue()/Stack<T>.Pop() (and every
//   collection's own internal grow/resize step) need to extract an owned element out of backing
//   array storage; move() previously only accepted a named RAII local or an owned parameter.
// Fix 5 (return inside try/catch crashed the LLVM lowerer): discovered while giving
//   Dictionary<TKey,TValue>'s indexer a real KeyNotFoundException, `try { return x; } catch
//   (...) { ... }` -- an entirely ordinary shape, no throw/rethrow or RAII cleanup required --
//   hit "Stage 45.10 return through cleanup/catch has no return spill slot", an internal lowerer
//   error, not a diagnostic. containsExceptionalControl() (the pass that decides whether a
//   function needs the EH return-value spill slot) checked for Throw/Rethrow and a
//   cleanupRegionOrdinal on Try/Finally/Using, but never checked for a CatchRegion itself --
//   so a try/catch with neither fell through undetected, and the funclet the catch created blew
//   up the assumption in lowerStructuredExit's complexExit check.
// Fix 6 (cross-project generic delegate/primitive parse failure): collectTypeSeedsInScope (the
//   parser's lightweight cross-file "known type names" pre-scanner, used to disambiguate `<` as
//   a generic argument list vs. a less-than comparison) never recognized a `delegate` declaration
//   at all -- so a generic delegate name declared in a REFERENCED project (e.g. Predicate<T> in a
//   shared library) was invisible to that scanner, and "Predicate<int> p = ...;" in the consuming
//   project failed to parse (misread as a comparison). A class type argument happened to
//   disambiguate through a different, unaffected path, which is why this went unnoticed until a
//   delegate closed over a builtin primitive was tried cross-project.
// Fix 7 (cross-project generic instantiation with a consuming-project type argument): the
//   overwhelmingly common real shape, "List<MyOwnClass>" where List<T> lives in a shared library
//   and MyOwnClass is declared only in the application project referencing that library, failed
//   with "unknown type name 'MyOwnClass'" -- name lookup for a clone's substituted argument
//   always used the TEMPLATE's own project/namespace, never the REQUESTING call site's. Fixed via
//   TypeSyntax::originProjectOrdinal/originNamespace overrides threaded through
//   ProjectGenericModel::cloneType. Two further bugs surfaced while nailing this down, both
//   covered here: (a) a NESTED specialization discovered while walking the outer specialization's
//   own cloned body (e.g. List<Payload>'s internal Predicate<T> use) re-stamped an
//   already-correctly-tagged argument with the wrong (library-own) origin, fixed by only
//   propagating tags forward, never re-deriving them once set; (b) an earlier version of this fix
//   mutated the argument's baseName() itself to qualify it, which broke generic specialization
//   *identity* -- ProjectGenericModel's specialization cache key is the argument's raw spelling,
//   so the same closed type ("Predicate<Payload>") spelled differently depending on whether it
//   came from the application's own top-level declaration or from List<Payload>'s internal reuse,
//   silently producing two incompatible specializations of what should be one type and breaking
//   overload resolution between them.
// Fix 8 (move() into a native array element assignment target): `grown[i] = move(items[i]);` --
//   the exact step EnsureCapacity-style growable generic storage needs -- was rejected with
//   "move(...) is only valid for a direct RAII local or owned parameter..." because
//   analyzeAssignment only ever allowed a move()-valued RHS when the assignment TARGET was a
//   named RAII local identifier; an array-element target (itself already a legal, ownership-safe
//   relocation within one owned array -- see List.ns's class comment) was never considered.
// Fix 9 (default(T) was not implemented at all): List<T>.Find's "return default(T);" (returning
//   the zero-equivalent value when nothing matches, exactly like real C#) needs default(T), which
//   turned out not to exist anywhere in the compiler -- not lexed, not parsed, not lowered.
//   Implemented end-to-end as a genuine language feature (KwDefault keyword, DefaultValueExpression
//   AST/HIR/MIR node, LLVMConstNull-based lowering that is uniformly correct for every closed T:
//   numeric zero, a null reference, or an all-zero-fields struct), not special-cased to Stage 52's
//   collections alone.
// Fix 10 (move() on a value-typed owned local/parameter, as a CALL ARGUMENT, was miscompiled in
//   two layers): discovered via LinkedList<int>.AddFirst's "created[0].SetValue(move(value))" --
//   an owned int PARAMETER moved into another owned-int-parameter method call.
//   (a) Semantic overload-matching bug: the call-argument viability loop's ownership checks were
//   an if/else-if chain gated as "parameterOwnership == Owned && parameterOwnable" / "... ==
//   Borrowed && parameterOwnable" / else (catch-all: reject any ownership-transfer argument into
//   an "unqualified" parameter). A value-typed "owned T"/"borrowed T" parameter (parameterOwnable
//   false) fell through BOTH real branches into that catch-all and was wrongly rejected as
//   non-viable when called with an explicit move(...) argument -- "no viable overload of
//   'SetValue'" -- even though the exact same call with a PLAIN (non-move) value-typed argument
//   was already correctly accepted elsewhere (fix 3). Fixed by checking !parameterOwnable FIRST,
//   uniformly bypassing all ownership viability checks for a non-ownable parameter type.
//   (b) LLVM lowering bug, uncovered once (a) let the call through: MoveValue (the pre-Stage-52
//   move()-from-a-named-local/parameter lowering) unconditionally loaded and null-stored the
//   source storage AS A POINTER -- correct for Stage 49/50, where move() only ever targeted a
//   reference-typed RAII owner, but wrong once a generic T can close over a value type: loading
//   an i32-typed alloca as a pointer produced a call-parameter type mismatch that the LLVM
//   verifier rejected outright. Fixed by loading/storing using the move's real typeKey (like
//   MoveArrayElement already did) and gating the null-store on a new reference-like flag,
//   threaded through MethodMoveBinding::elementIsReferenceLike (now computed for every move()
//   source shape, not just an array element).
//
// Known, pre-existing, and deliberately UN-fixed limitation discovered while writing
// Dictionary<TKey,TValue> (documented, not "Fix 11", because it is general struct-representation
// behavior with nothing to do with generics or ownership -- confirmed by reproducing it with a
// wholly non-generic, two-`long`-field struct): Stage 40's Windows x64 struct-value ABI only
// lowers a struct TYPE (as a local variable, a call argument, or a return value) when its total
// size is 1, 2, 4, or 8 bytes. A struct over 8 bytes -- e.g. KeyValuePair<TKey,TValue> whenever
// TKey and TValue together exceed that -- cannot be declared as a local, passed as an argument,
// or returned by value at all; it CAN still be used as an array element type, accessed through
// direct field access on the indexed element (`arr[i].Field = ...`), because that path addresses
// memory in place and never materializes the whole struct as a first-class value. This shaped two
// Dictionary<TKey,TValue> members: ToArray() still returns KeyValuePair<TKey,TValue>[], written
// and meant to be read via `result[i].Key`/`result[i].Value` field access, never by copying an
// element into a standalone `KeyValuePair<TKey,TValue>` local; ForEach takes a two-parameter
// `KeyValueAction<TKey,TValue>(TKey key, TValue value)` delegate instead of a
// one-parameter `Action<KeyValuePair<TKey,TValue>>`, sidestepping the limitation entirely for the
// common iteration case.

#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

// Extracts the LLVM IR text of one function whose mangled name contains `nameFragment`, i.e. the
// span from its "define ..." line up to the matching top-level closing "}". Empty if not found.
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

// Same as writeProject, but with a <ProjectReference> to another project's .nsproj file --
// used by the Fix 6/7 cross-project fixtures (a shared "library" project referenced by a
// consuming "application" project, exactly like NativeSharp.Core.Collections.Generic and the
// application code that uses it).
void writeProjectWithReference(
    const std::filesystem::path& directory,
    std::string_view name,
    const std::filesystem::path& referencedNsproj)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /><ProjectReference Include=\"" +
            referencedNsproj.generic_string() + "\" /></ItemGroup></Project>");
}

// Compiles a fixture all the way to a verified, emitted COFF object, exactly like a real build.
// Returns the generated LLVM IR text (for callers that want to inspect specific instructions),
// or an empty string on any failure (with a diagnostic already reported via `tests.expect`).
std::string compileAndEmitObject(
    TestContext& tests,
    const std::filesystem::path& directory,
    std::string_view label)
{
    ProjectCompilation mir;
    std::string error;
    const bool lowered = mir.compileProjectOopMir(directory / (directory.filename().string() + ".nsproj"), error);
    tests.expect(lowered, std::string(label) + ": lowers to OOP MIR");
    tests.expect(error.empty(), std::string(label) + ": no MIR lowering error (" + error + ")");
    if (!lowered || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return {};

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        std::string(label) + ": lowers to LLVM (" + lowerer.error() + ")");
    if (module == nullptr) return {};

    std::string verifyError;
    tests.expect(module->verify(verifyError), std::string(label) + ": LLVM module verifies clean (" + verifyError + ")");

    const std::string llvm = module->text();

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), std::string(label) + ": obtains Windows x64 target machine");
    if (target == nullptr) return llvm;
    tests.expect(target->configureModuleForTarget(*module, targetError), std::string(label) + ": applies Windows x64 data layout");

    const auto object = std::filesystem::temp_directory_path() /
        ("nativesharp_stage52_" + directory.filename().string() + ".obj");
    tests.expect(
        target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        std::string(label) + ": emits a real Windows x64 COFF object");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), std::string(label) + ": object passes structural COFF verification");
    std::error_code cleanupError;
    std::filesystem::remove(object, cleanupError);

    return llvm;
}

// ---------------------------------------------------------------------------------------
// Fix 1 + 2 + 3 combined: a single generic Box<T> exercising owned-array storage, an
// owned-parameter Add(), and a Clear() that deletes every element -- instantiated with BOTH a
// reference-typed T (Payload, a plain class) and a value-typed T (int) in the same fixture.
// This is the actual shape List<T>/HashSet<T>/Dictionary<TKey,TValue> need: uniform storage and
// uniform cleanup code, regardless of what T monomorphizes to.
// ---------------------------------------------------------------------------------------

void testGenericOwnedArrayAddAndClearForBothValueAndReferenceT(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "BoxAddClear";
    writeProject(directory, "BoxAddClear");
    writeFile(directory / "Main.ns", R"NS(
namespace BoxAddClear
{
    public class Payload
    {
        public int Value;
    }

    public class Box<T>
    {
        private owned T[] items;
        private int count;

        public Box(int capacity)
        {
            items = new T[capacity];
            count = 0;
        }

        // Fix 2/3: `owned T value` must monomorphize to a real owned parameter when T is a
        // reference type (requiring move() at every call site) and to a plain-value parameter
        // when T is a value type (requiring no ownership ceremony at all) -- from the SAME
        // generic source text.
        //
        // Bloque 1 correction: this body used to read `items[count] = value;`, and that was a
        // DOUBLE FREE, not a stylistic detail. Measured on this exact fixture made executable --
        // two Payloads added and then Cleared reported FOUR destructions at both -O0 and -O2,
        // because the owned parameter's own end-of-scope cleanup fired in Add() and `delete
        // items[i]` fired again in Clear() on the same allocation. With move() it reports two.
        //
        // It survived since Stage 52 because this suite only ever asserted that the fixture
        // "lowers to OOP MIR" -- an IR-shape check cannot tell a correct store from one that
        // frees the object twice, which is precisely the pattern the test audit flagged. The
        // fixture is fixed rather than the assertion loosened: the compiler now rejects the
        // no-move form outright (see Stage50_11's CopyingAnOwnedParameterByAssignmentRejected),
        // so leaving the old text here would only assert that a rejected program lowers.
        public void Add(owned T value)
        {
            items[count] = move(value);
            count = count + 1;
        }

        // Fix 1: `delete items[i];` must be a legal, uniform cleanup step for every T. For a
        // reference-typed T it frees each element; for a value-typed T it is a harmless no-op
        // (the operand is still evaluated, so out-of-range access still traps).
        public void Clear()
        {
            int i = 0;
            while (i < count)
            {
                delete items[i];
                i = i + 1;
            }
            count = 0;
        }

        public int Count() { return count; }
    }

    public class Runner
    {
        public static int Main()
        {
            raii Payload p1 = new Payload();
            raii Payload p2 = new Payload();
            Box<Payload> refBox = new Box<Payload>(4);
            refBox.Add(move(p1));
            refBox.Add(move(p2));
            refBox.Clear();

            Box<int> valueBox = new Box<int>(4);
            valueBox.Add(1);
            valueBox.Add(2);
            valueBox.Add(3);
            valueBox.Clear();

            return refBox.Count() + valueBox.Count();
        }
    }
}
)NS");

    const std::string llvm = compileAndEmitObject(tests, directory, "generic owned-array Add/Clear (ref T and value T)");
    if (llvm.empty()) return;

    // Fix 1's actual runtime effect: the reference-typed Box<Payload>.Clear() must call the
    // allocator-free hook at least once (one per Add'ed element), while nothing about the
    // value-typed Box<int>.Clear() should require it -- both bodies exist in the same module, so
    // this is a coarse but meaningful sanity check that value-typed deletion did not silently
    // start freeing raw integers.
    tests.expect(llvm.find("__nativesharp_free") != std::string::npos,
        "reference-typed Box<Payload>.Clear() still emits a real free call for each deleted element");
}

// ---------------------------------------------------------------------------------------
// Fix 4: move() from a native array element (`move(items[i])`), the mechanism Queue<T>.Dequeue()
// and Stack<T>.Pop() need to hand back ownership of a stored element. Verified for both a
// reference-typed T (must null the vacated slot after loading, LLVM `store ptr null`) and a
// value-typed T (must NOT touch the slot at all -- there is nothing to null).
// ---------------------------------------------------------------------------------------

void testMoveFromArrayElementForBothValueAndReferenceT(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "MoveArrayElement";
    writeProject(directory, "MoveArrayElement");
    writeFile(directory / "Main.ns", R"NS(
namespace MoveArrayElement
{
    public class Payload
    {
        public int Value;
    }

    public class RefSlot<T>
    {
        private owned T[] items;

        public RefSlot(int capacity) { items = new T[capacity]; }

        // Bloque 1 sweep: see Grower.Add below -- `= value` from an owned parameter is a double free.
        public void Put(owned T value) { items[0] = move(value); }

        // The Dequeue()/Pop() shape: hand back ownership of a stored element via move().
        public owned T Take() { return move(items[0]); }
    }

    public class ValueSlot<T>
    {
        private T[] items;

        public ValueSlot(int capacity) { items = new T[capacity]; }

        public void Put(T value) { items[0] = value; }

        public T Take() { return move(items[0]); }
    }

    public class Runner
    {
        public static int Main()
        {
            raii Payload p = new Payload();
            p.Value = 11;
            RefSlot<Payload> refSlot = new RefSlot<Payload>(2);
            refSlot.Put(move(p));
            raii Payload taken = refSlot.Take();

            ValueSlot<int> valueSlot = new ValueSlot<int>(2);
            valueSlot.Put(31);
            int takenValue = valueSlot.Take();

            return taken.Value + takenValue;
        }
    }
}
)NS");

    const std::string llvm = compileAndEmitObject(tests, directory, "move(items[i]) for ref T and value T");
    if (llvm.empty()) return;

    tests.expect(llvm.find("store ptr null") != std::string::npos,
        "reference-typed move(items[i]) nulls the vacated slot to prevent later double-use/double-free");
}

// ---------------------------------------------------------------------------------------
// Regression guard: move() must still reject a source that is not a direct native array element
// (e.g. a custom indexer call on a non-array type) -- fix 4 only widens move()'s accepted source
// shapes, it does not turn move() into an "extract ownership from anything" escape hatch.
// ---------------------------------------------------------------------------------------

void testMoveStillRejectsNonNativeArrayIndexSource(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "MoveRejectsIndexer";
    writeProject(directory, "MoveRejectsIndexer");
    writeFile(directory / "Main.ns", R"NS(
namespace MoveRejectsIndexer
{
    public class Payload { public int Value; }

    public class Indexed
    {
        private owned Payload[] backing;
        public Indexed() { backing = new Payload[4]; }
        public owned Payload this[int i]
        {
            get { return move(backing[i]); }
            // Bloque 1 sweep: the setter's implicit `value` is an owned parameter here (the
            // indexer is declared `owned Payload`), so storing it without move() is the same
            // double free as every other site corrected in this file.
            set { backing[i] = move(value); }
        }
    }

    public class Runner
    {
        public static int Main()
        {
            Indexed ix = new Indexed();
            raii Payload taken = move(ix[0]);
            return 0;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "MoveRejectsIndexer.nsproj", error);
    tests.expect(!ok, "move() on a custom indexer call (not a direct native array element) is still rejected");

    // Method-body semantic diagnostics (as opposed to the earlier load/lex/parse stages) are
    // recorded on the compilation's DiagnosticEngine rather than forwarded through the
    // `loadError` out-parameter, so the specific message is checked there.
    //
    // SHR-005 (2026-09-02) amplio este mensaje: la admision de fuente de move(...) dejo de ser
    // solo el arreglo NATIVO y pasó a incluir tambien el arreglo FIJO, que quedaba fuera por un
    // accidente --`isNativeArray` compara el sufijo "[]" y `T[n]` termina en "n]"-- y no por una
    // decision. Lo que esta prueba mide no cambia: que la negativa nombre LA restriccion concreta
    // y no un error cualquiera de mas arriba. Lo que cambia es que la restriccion ya no es la
    // misma frase.
    bool foundExpectedMessage = false;
    for (const nativesharp::diagnostics::Diagnostic& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find(
                "move(...) array-element source must be a direct native array or fixed array element access") !=
            std::string::npos)
        {
            foundExpectedMessage = true;
            break;
        }
    }
    tests.expect(foundExpectedMessage,
        "the rejection names the specific move()-array-element restriction, not an unrelated error");
}

// ---------------------------------------------------------------------------------------
// Fix 6 + Fix 7: a shared "library" project (LibA) with a generic delegate and a generic class
// whose method uses that delegate internally, referenced by a consuming "application" project
// (AppB) that instantiates both with a BUILTIN type (Fix 6: the delegate parser-seed gap) and
// with a type declared only in AppB itself (Fix 7: cross-project name resolution), including the
// specialization-identity regression Fix 7 introduced and then had to fix again: AppB's own
// top-level "Predicate<Payload>" declaration and Holder<Payload>.Matches's internal
// "Predicate<T>" -> "Predicate<Payload>" substitution must specialize to the SAME type for the
// call `payloadHolder.Matches(payloadPred)` to type-check at all.
// ---------------------------------------------------------------------------------------

void testCrossProjectGenericDelegateAndClassInstantiation(TestContext& tests, const std::filesystem::path& root)
{
    const auto libDirectory = root / "CrossProjectLibA";
    writeProject(libDirectory, "CrossProjectLibA");
    writeFile(libDirectory / "Predicate.ns", R"NS(
namespace CrossProjectLibA
{
    // Fix 6: a generic delegate declaration -- collectTypeSeedsInScope must recognize this so
    // that "Predicate<int>" (a delegate closed over a BUILTIN primitive) parses correctly from a
    // REFERENCING project's source.
    public delegate bool Predicate<T>(T value);
}
)NS");
    writeFile(libDirectory / "Holder.ns", R"NS(
namespace CrossProjectLibA
{
    // Fix 7: a generic class whose own method uses Predicate<T> internally -- specializing
    // Holder<X> for any X forces a NESTED Predicate<T> -> Predicate<X> specialization discovered
    // while walking Holder<X>'s own cloned body, exactly like List<T>.FindIndex/Find/Exists use
    // Predicate<T> inside List.ns.
    public class Holder<T>
    {
        private owned T[] items;

        // Constructors deliberately never take an `owned T` parameter, matching the established
        // NativeSharp idiom (List<T>'s own constructors) -- ownership is transferred through a
        // dedicated method call afterward instead.
        public Holder()
        {
            items = new T[1];
        }

        public void Put(owned T v)
        {
            // Bloque 1 sweep: see Grower.Add below -- storing an owned parameter without move()
            // leaves two owners of one allocation.
            items[0] = move(v);
        }

        public bool Matches(Predicate<T> match)
        {
            return match(items[0]);
        }
    }
}
)NS");

    const auto appDirectory = root / "CrossProjectAppB";
    writeProjectWithReference(
        appDirectory, "CrossProjectAppB", libDirectory / "CrossProjectLibA.nsproj");
    writeFile(appDirectory / "Main.ns", R"NS(
using CrossProjectLibA;

namespace CrossProjectAppB
{
    // Fix 7: a type declared ONLY in the consuming project -- the overwhelmingly common real
    // shape ("List<MyOwnClass>").
    public class Payload
    {
        public int Value;
    }

    public class Runner
    {
        public static bool IsPositive(int value)
        {
            return value > 0;
        }

        public static bool PayloadIsPositive(Payload value)
        {
            return value.Value > 0;
        }

        public static int Main()
        {
            // Fix 6: Predicate<int> -- a cross-project generic delegate closed over a builtin.
            Predicate<int> intPredicate = IsPositive;
            Holder<int> intHolder = new Holder<int>();
            intHolder.Put(5);
            bool intMatch = intHolder.Matches(intPredicate);

            // Fix 7 (+ the specialization-identity regression it introduced): Predicate<Payload>
            // declared here at the AppB call site must be the SAME specialization
            // Holder<Payload>.Matches's internal Predicate<T> use closes over.
            raii Payload p = new Payload();
            p.Value = 9;
            Predicate<Payload> payloadPredicate = PayloadIsPositive;
            Holder<Payload> payloadHolder = new Holder<Payload>();
            payloadHolder.Put(move(p));
            bool payloadMatch = payloadHolder.Matches(payloadPredicate);

            if (intMatch && payloadMatch) { return 1; }
            return 0;
        }
    }
}
)NS");

    const std::string llvm = compileAndEmitObject(
        tests, appDirectory, "cross-project generic delegate + class instantiation (fix 6 + fix 7)");
    tests.expect(!llvm.empty(),
        "AppB referencing CrossProjectLibA's Predicate<T>/Holder<T> reaches LLVM IR "
        "(both builtin- and consumer-type-closed instantiations resolve and specialize identically)");
}

// ---------------------------------------------------------------------------------------
// NSC-LIMIT-022: the type argument is declared by a producer project, while the generic
// template lives in a library and the same closed type is written again by a consumer. Both
// `Carrier<FontChar>` occurrences resolve to the one Producer::Saru.FontChar declaration and
// must therefore share one monomorphization, despite being written in different projects.
// ---------------------------------------------------------------------------------------

void testCrossProjectClosedGenericReturnKeepsOneIdentity(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const auto genericDirectory = root / "Limit022GenericLib";
    writeProject(genericDirectory, "Limit022GenericLib");
    writeFile(genericDirectory / "Carrier.ns", R"NS(
namespace Limit022GenericLib
{
    public class Carrier<T>
    {
        public T Value;
    }
}
)NS");

    const auto producerDirectory = root / "Limit022Producer";
    writeProjectWithReference(
        producerDirectory,
        "Limit022Producer",
        genericDirectory / "Limit022GenericLib.nsproj");
    writeFile(producerDirectory / "FontParser.ns", R"NS(
using Limit022GenericLib;

namespace Saru
{
    public struct FontChar
    {
        public int Id;
    }

    public class FontParser
    {
        public static owned Carrier<FontChar> Parse()
        {
            raii Carrier<FontChar> result = new Carrier<FontChar>();
            result.Value.Id = 65;
            return move(result);
        }
    }
}
)NS");

    const auto consumerDirectory = root / "Limit022Consumer";
    writeFile(
        consumerDirectory / "Limit022Consumer.nsproj",
        "<Project><PropertyGroup><Name>Limit022Consumer</Name></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" /><ProjectReference Include=\"" +
            producerDirectory.generic_string() + "/Limit022Producer.nsproj\" />"
        "<ProjectReference Include=\"" + genericDirectory.generic_string() +
            "/Limit022GenericLib.nsproj\" /></ItemGroup></Project>");
    writeFile(consumerDirectory / "Main.ns", R"NS(
using Limit022GenericLib;

namespace Saru
{
    public class Runner
    {
        public static int Main()
        {
            raii Carrier<FontChar> glyphs = FontParser.Parse();
            FontChar glyph = glyphs.Value;
            return glyph.Id;
        }
    }
}
)NS");

    ProjectCompilation identityCompilation;
    std::string identityError;
    const bool identitiesBuilt = identityCompilation.compileProjectGenerics(
        consumerDirectory / "Limit022Consumer.nsproj", identityError);
    tests.expect(identitiesBuilt && identityError.empty() &&
            identityCompilation.genericModel() != nullptr,
        "cross-project closed generic return builds its specialization model");
    if (identitiesBuilt && identityCompilation.genericModel() != nullptr)
    {
        std::size_t carrierCount = 0;
        bool namesProducerDeclaration = false;
        bool containsWritingSiteFallback = false;
        for (const auto& specialization : identityCompilation.genericModel()->specializations())
        {
            if (specialization.templateKey.find(
                    "Limit022GenericLib::Limit022GenericLib.Carrier`1") == std::string::npos)
            {
                continue;
            }
            ++carrierCount;
            namesProducerDeclaration = namesProducerDeclaration ||
                specialization.specializationKey.find(
                    "type:Limit022Producer::Saru.FontChar") != std::string::npos;
            containsWritingSiteFallback = containsWritingSiteFallback ||
                specialization.specializationKey.find("@origin-module") != std::string::npos;
        }
        tests.expect(carrierCount == 1,
            "producer and consumer share exactly one Carrier<FontChar> monomorphization");
        tests.expect(namesProducerDeclaration,
            "the specialization key names FontChar's declaring module, not its writing site");
        tests.expect(!containsWritingSiteFallback,
            "a resolved cross-project type argument does not use the origin-module fallback");
    }

    const std::string llvm = compileAndEmitObject(
        tests, consumerDirectory, "cross-project closed generic return identity (NSC-LIMIT-022)");
    tests.expect(!llvm.empty(),
        "a closed generic returned by a producer is assignable to the identical type in its consumer");
}

std::size_t countOccurrences(std::string_view text, std::string_view needle)
{
    if (needle.empty()) return 0;
    std::size_t count = 0;
    std::size_t offset = 0;
    while ((offset = text.find(needle, offset)) != std::string_view::npos)
    {
        ++count;
        offset += needle.size();
    }
    return count;
}

// ---------------------------------------------------------------------------------------
// NSC-LIMIT-011: a constraint belongs to the library declaration, but its substituted
// argument belongs to the consumer. Both names must therefore be resolved under different
// visibility contexts after the specialization has been cloned into the library's AST.
// ---------------------------------------------------------------------------------------

void testCrossProjectGenericConstraintWithConsumerType(TestContext& tests, const std::filesystem::path& root)
{
    const auto libDirectory = root / "Limit011Lib";
    writeProject(libDirectory, "Limit011Lib");
    writeFile(libDirectory / "Catalog.ns", R"NS(
namespace Limit011Lib
{
    public interface IComponent
    {
        int Score();
    }

    public interface INever
    {
        int Missing();
    }

    public class Catalog
    {
        public static int Read<T>(T value) where T : class, IComponent
        {
            return value.Score();
        }

        // Both overloads are materialized. Processing the first one's Reader<T> changes the
        // ambient instantiation context recursively; the second clone must still use the
        // original consumer context for its substituted constraint argument.
        public static int Choose<T>(int value) where T : INever
        {
            Reader<T> ignored = new Reader<T>();
            return value;
        }

        public static int Choose<T>(T value) where T : IComponent
        {
            return value.Score();
        }
    }

    public class Reader<T> where T : class, IComponent
    {
        public int Read(T value) { return value.Score(); }
    }

    public class Envelope<T> where T : class, IComponent
    {
        public int Read(T value)
        {
            Reader<T> nested = new Reader<T>();
            return nested.Read(value);
        }
    }
}
)NS");

    const auto appDirectory = root / "Limit011App";
    writeProjectWithReference(appDirectory, "Limit011App", libDirectory / "Limit011Lib.nsproj");
    writeFile(appDirectory / "Main.ns", R"NS(
using Limit011Lib;

namespace Limit011Game
{
    public class Payload : IComponent
    {
        public int Score() { return 42; }
    }

    public class Runner
    {
        public static int Main()
        {
            raii Payload payload = new Payload();
            Envelope<Payload> envelope = new Envelope<Payload>();
            if (Catalog.Choose<Payload>(payload) != 42) { return 1; }
            return Catalog.Read<Payload>(payload) + envelope.Read(payload) - 42;
        }
    }
}
)NS");

    const std::string llvm = compileAndEmitObject(
        tests, appDirectory, "cross-project generic constraint with consumer-owned argument type");
    tests.expect(!llvm.empty(),
        "a library constraint resolves its declaration in the library and its substituted "
        "argument in the consuming project");
}

void testCrossProjectGenericConstraintWithGlobalConsumerType(TestContext& tests, const std::filesystem::path& root)
{
    const auto libDirectory = root / "Limit011GlobalLib";
    writeProject(libDirectory, "Limit011GlobalLib");
    writeFile(libDirectory / "Catalog.ns", R"NS(
namespace Limit011GlobalLib
{
    public interface IComponent { int Score(); }
    public class Catalog
    {
        public static int Read<T>(T value) where T : class, IComponent { return value.Score(); }
    }
}
)NS");

    const auto appDirectory = root / "Limit011GlobalApp";
    writeProjectWithReference(appDirectory, "Limit011GlobalApp", libDirectory / "Limit011GlobalLib.nsproj");
    writeFile(appDirectory / "Main.ns", R"NS(
using Limit011GlobalLib;

public class GlobalPayload : IComponent
{
    public int Score() { return 42; }
}

public class GlobalRunner
{
    public static int Main()
    {
        raii GlobalPayload payload = new GlobalPayload();
        return Catalog.Read<GlobalPayload>(payload);
    }
}
)NS");

    const std::string llvm = compileAndEmitObject(
        tests, appDirectory, "cross-project generic constraint from the consumer global namespace");
    tests.expect(!llvm.empty(),
        "an explicit empty origin namespace resolves in the consumer global namespace, not in the library namespace");
}

void testCrossProjectGenericConstraintKeepsHomonymousConsumersDistinct(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const auto libDirectory = root / "Limit011HomonymLib";
    writeProject(libDirectory, "Limit011HomonymLib");
    writeFile(libDirectory / "Catalog.ns", R"NS(
namespace Limit011HomonymLib
{
    public interface IComponent { int Score(); }
    public class Catalog
    {
        public static int Read<T>(T value) where T : class, IComponent { return value.Score(); }
    }
}
)NS");

    const auto firstDirectory = root / "Limit011ConsumerA";
    writeProjectWithReference(firstDirectory, "Limit011ConsumerA", libDirectory / "Limit011HomonymLib.nsproj");
    writeFile(firstDirectory / "Entry.ns", R"NS(
using Limit011HomonymLib;
namespace Limit011SharedGame
{
    public class Payload : IComponent { public int Score() { return 20; } }
    public class EntryA
    {
        public static int Run()
        {
            raii Payload value = new Payload();
            return Catalog.Read<Payload>(value);
        }
    }
}
)NS");

    const auto secondDirectory = root / "Limit011ConsumerB";
    writeProjectWithReference(secondDirectory, "Limit011ConsumerB", libDirectory / "Limit011HomonymLib.nsproj");
    writeFile(secondDirectory / "Entry.ns", R"NS(
using Limit011HomonymLib;
namespace Limit011SharedGame
{
    public class Payload : IComponent { public int Score() { return 22; } }
    public class EntryB
    {
        public static int Run()
        {
            raii Payload value = new Payload();
            return Catalog.Read<Payload>(value);
        }
    }
}
)NS");

    const auto hostDirectory = root / "Limit011Host";
    writeFile(
        hostDirectory / "Limit011Host.nsproj",
        "<Project><PropertyGroup><Name>Limit011Host</Name></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" /><ProjectReference Include=\"" +
            firstDirectory.generic_string() + "/Limit011ConsumerA.nsproj\" />"
        "<ProjectReference Include=\"" + secondDirectory.generic_string() +
            "/Limit011ConsumerB.nsproj\" /></ItemGroup></Project>");
    writeFile(hostDirectory / "Main.ns", R"NS(
using Limit011SharedGame;
public class HostRunner
{
    public static int Main() { return EntryA.Run() + EntryB.Run(); }
}
)NS");

    const std::string llvm = compileAndEmitObject(
        tests, hostDirectory, "homonymous consumer arguments of one library generic constraint");
    tests.expect(!llvm.empty(),
        "two consumers may close one library template over distinct same-spelled types");
    tests.expect(countOccurrences(llvm, "Read__g1__Payload__h") >= 2,
        "homonymous consumer types produce distinct specializations instead of sharing the first consumer's clone");

    ProjectCompilation identityCompilation;
    std::string identityError;
    const bool identitiesBuilt = identityCompilation.compileProjectGenerics(
        hostDirectory / "Limit011Host.nsproj", identityError);
    tests.expect(identitiesBuilt && identityError.empty() && identityCompilation.genericModel() != nullptr,
        "the homonymous-consumer project exposes its specialization identities");
    if (identitiesBuilt && identityCompilation.genericModel() != nullptr)
    {
        bool foundConsumerA = false;
        bool foundConsumerB = false;
        bool foundGraphOrdinal = false;
        for (const auto& specialization : identityCompilation.genericModel()->specializations())
        {
            if (specialization.templateKey.find("Limit011HomonymLib::Limit011HomonymLib.Catalog.Read") ==
                std::string::npos)
            {
                continue;
            }
            foundConsumerA = foundConsumerA ||
                specialization.specializationKey.find("Limit011ConsumerA") != std::string::npos;
            foundConsumerB = foundConsumerB ||
                specialization.specializationKey.find("Limit011ConsumerB") != std::string::npos;
            foundGraphOrdinal = foundGraphOrdinal ||
                specialization.specializationKey.find("origin-project=") != std::string::npos;
        }
        tests.expect(foundConsumerA && foundConsumerB,
            "specialization identity records the stable module identity of each homonymous consumer");
        tests.expect(!foundGraphOrdinal,
            "specialization identity never exposes the graph-order-dependent project ordinal");
    }
}

void testCrossProjectGenericConstraintRejectsAtConsumerSite(TestContext& tests, const std::filesystem::path& root)
{
    const auto libDirectory = root / "Limit011RejectLib";
    writeProject(libDirectory, "Limit011RejectLib");
    writeFile(libDirectory / "Catalog.ns", R"NS(
namespace Limit011RejectLib
{
    public interface IComponent { int Score(); }
    public class Catalog
    {
        public static int Read<T>(T value) where T : class, IComponent { return value.Score(); }
    }
}
)NS");

    const auto appDirectory = root / "Limit011RejectApp";
    writeProjectWithReference(appDirectory, "Limit011RejectApp", libDirectory / "Limit011RejectLib.nsproj");
    writeFile(appDirectory / "Main.ns", R"NS(
using Limit011RejectLib;
namespace Limit011RejectGame
{
    public class Wrong { }
    public class Runner
    {
        public static int Main()
        {
            raii Wrong wrong = new Wrong();
            return Catalog.Read<Wrong>(wrong);
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectGenericConstraints(
        appDirectory / "Limit011RejectApp.nsproj", error);
    tests.expect(!ok, "an invalid consumer type is rejected by the library's interface constraint");
    tests.expect(error.empty(), "the semantic constraint rejection is not reported as a project-load failure");

    std::size_t matchingDiagnostics = 0;
    for (const nativesharp::diagnostics::Diagnostic& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find("argument 'Wrong' does not implement it") == std::string::npos)
        {
            continue;
        }
        ++matchingDiagnostics;
        tests.expect(diagnostic.message().find("Limit011RejectLib") != std::string::npos,
            "the cross-project constraint diagnostic names the library module that owns the contract");
        tests.expect(compilation.sourceManager().getPath(diagnostic.location()).find("Limit011RejectApp") !=
                std::string_view::npos,
            "the cross-project constraint diagnostic points into the consuming project");
        tests.expect(diagnostic.range().has_value() &&
                compilation.sourceManager().getText(*diagnostic.range()).find("Catalog.Read<Wrong>(wrong)") !=
                    std::string_view::npos,
            "the cross-project constraint diagnostic highlights the invalid consumer call");
    }
    tests.expect(matchingDiagnostics == 1,
        "the invalid cross-project specialization emits exactly one relevant constraint diagnostic");
}

// ---------------------------------------------------------------------------------------
// Fix 8: move() into a native array element ASSIGNMENT TARGET, not just as a move() source --
// the exact "grown[i] = move(items[i]);" shape a growable owned generic array (List<T>.
// EnsureCapacity) needs to relocate elements into a new, larger backing array. Verified for both
// a reference-typed T (must null the vacated source slot, same as fix 4) and a value-typed T
// (must compile with no null-store, since there is no ownership to transfer).
// ---------------------------------------------------------------------------------------

void testMoveIntoArrayElementAssignmentTarget(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "MoveIntoArrayElementTarget";
    writeProject(directory, "MoveIntoArrayElementTarget");
    writeFile(directory / "Main.ns", R"NS(
namespace MoveIntoArrayElementTarget
{
    public class Payload { public int Value; }

    public class Grower<T>
    {
        private owned T[] items;
        private int count;

        public Grower(int capacity)
        {
            items = new T[capacity];
            count = 0;
        }

        public void Add(owned T value)
        {
        // Bloque 1 sweep: `= value` here was a DOUBLE FREE -- the owned parameter's own
        // end-of-scope cleanup and the container's later `delete` both fired on one
        // allocation (measured: 4 destructions for 2 objects, at -O0 and -O2). Five
        // fixtures in this file had the same shape and every one of them passed, because
        // this suite only ever asserted that they "lower to OOP MIR". The compiler now
        // rejects the no-move form; see Stage50_11's CopyingAnOwnedParameterByAssignmentRejected.
            items[count] = move(value);
            count = count + 1;
        }

        // The EnsureCapacity shape: relocate every element from the old backing array into a
        // freshly allocated, larger one via move() on BOTH sides of the assignment.
        public void Grow(int newCapacity)
        {
            const T[] grown = new T[newCapacity];
            int i = 0;
            while (i < count)
            {
                grown[i] = move(items[i]);
                i = i + 1;
            }
            delete[] items;
            items = grown;
        }

        public int Count() { return count; }
    }

    public class Runner
    {
        public static int Main()
        {
            raii Payload p1 = new Payload();
            raii Payload p2 = new Payload();
            Grower<Payload> refGrower = new Grower<Payload>(2);
            refGrower.Add(move(p1));
            refGrower.Add(move(p2));
            refGrower.Grow(8);

            Grower<int> valueGrower = new Grower<int>(2);
            valueGrower.Add(1);
            valueGrower.Add(2);
            valueGrower.Grow(8);

            return refGrower.Count() + valueGrower.Count();
        }
    }
}
)NS");

    const std::string llvm = compileAndEmitObject(
        tests, directory, "move() into a native array element assignment target (fix 8)");
    if (llvm.empty()) return;

    tests.expect(llvm.find("store ptr null") != std::string::npos,
        "reference-typed grown[i] = move(items[i]) nulls the vacated source slot");
}

// ---------------------------------------------------------------------------------------
// Fix 9: default(T) -- the zero-equivalent value of T, needed by List<T>.Find to return an
// honest "nothing matched" result. Verified for a reference-typed T (must be a null value) and a
// value-typed T (must be numeric zero), plus the guard that default(void) is still rejected.
// ---------------------------------------------------------------------------------------

void testDefaultValueExpressionForBothValueAndReferenceT(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "DefaultValueExpression";
    writeProject(directory, "DefaultValueExpression");
    writeFile(directory / "Main.ns", R"NS(
namespace DefaultValueExpression
{
    public class Payload { public int Value; }

    public class Box<T>
    {
        public T Default()
        {
            return default(T);
        }
    }

    public class Runner
    {
        public static int Main()
        {
            Box<Payload> refBox = new Box<Payload>();
            Payload refDefault = refBox.Default();

            Box<int> valueBox = new Box<int>();
            int valueDefault = valueBox.Default();

            if (refDefault == null && valueDefault == 0) { return 1; }
            return 0;
        }
    }
}
)NS");

    const std::string llvm = compileAndEmitObject(tests, directory, "default(T) for ref T and value T (fix 9)");
    if (llvm.empty()) return;

    const std::string refDefaultBody = extractFunctionBody(llvm, "Box__g1__Payload");
    tests.expect(refDefaultBody.find("ret ptr null") != std::string::npos ||
        refDefaultBody.find("null") != std::string::npos,
        "default(T) for a reference-typed T lowers to a null value");

    const std::string valueDefaultBody = extractFunctionBody(llvm, "Box__g1__int");
    tests.expect(valueDefaultBody.find("ret i32 0") != std::string::npos,
        "default(T) for a value-typed T (int) lowers to numeric zero");
}

void testDefaultOfVoidIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "DefaultOfVoid";
    writeProject(directory, "DefaultOfVoid");
    writeFile(directory / "Main.ns", R"NS(
namespace DefaultOfVoid
{
    public class Runner
    {
        public static int Main()
        {
            default(void);
            return 0;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(directory / "DefaultOfVoid.nsproj", error);
    tests.expect(!ok, "default(void) is rejected");
}

// ---------------------------------------------------------------------------------------
// Fix 10: move() on a value-typed owned local/parameter, passed as a CALL ARGUMENT to another
// owned-parameter method, in a nested generic instantiation. This is the exact shape
// LinkedList<T>.AddFirst/AddLast need: a scratch `owned Box<T>[] created` single-element array
// (the established workaround for RAII-local reassignment restrictions), then
// `created[0].SetValue(move(value))` where `value` is itself an `owned T` PARAMETER. Verified for
// BOTH a value-typed T (int -- the shape that actually reproduced the bug) and a reference-typed T
// (Payload -- the shape that already worked, kept here as a non-regression companion case), and
// through a real, LLVM-verified, emitted object (exercising both sub-bugs: (a) the call would not
// even semantically resolve without the overload-viability fix, and (b) the emitted object would
// not verify without the MoveValue load/store type fix).
// ---------------------------------------------------------------------------------------

void testMoveOfOwnedParameterAsCallArgumentForBothValueAndReferenceT(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "MoveOwnedParameterAsCallArgument";
    writeProject(directory, "MoveOwnedParameterAsCallArgument");
    writeFile(directory / "Main.ns", R"NS(
namespace MoveOwnedParameterAsCallArgument
{
    public class Payload { public int Value; }

    public class Box<T>
    {
        private owned T[] slot;

        public Box()
        {
            slot = new T[1];
        }

        public void SetValue(owned T value)
        {
            // Bloque 1 sweep: see Grower.Add above -- an owned parameter must be moved into the
            // slot, not copied, or the parameter cleanup and the slot both free it.
            slot[0] = move(value);
        }
    }

    // Mirrors LinkedList<T>.AddFirst's real shape: receive an owned T parameter, construct a
    // fresh nested generic instance via the scratch single-element-array idiom (Box<T> cannot be
    // reassigned as a raii local with a non-move() RHS), then relocate the parameter into it with
    // an explicit move() call argument.
    public class Holder<T>
    {
        public owned Box<T> Wrap(owned T value)
        {
            owned Box<T>[] created = new Box<T>[1];
            created[0] = new Box<T>();
            created[0].SetValue(move(value));
            return move(created[0]);
        }
    }

    public class Runner
    {
        public static int Main()
        {
            raii Payload p = new Payload();
            Holder<Payload> refHolder = new Holder<Payload>();
            raii Box<Payload> refBox = refHolder.Wrap(move(p));

            Holder<int> valueHolder = new Holder<int>();
            raii Box<int> valueBox = valueHolder.Wrap(42);

            if (refBox != null && valueBox != null) { return 1; }
            return 0;
        }
    }
}
)NS");

    compileAndEmitObject(
        tests, directory,
        "move() of an owned value-typed parameter as a call argument, nested generic (fix 10)");
}

// ---------------------------------------------------------------------------------------
// Fix 1 regression guard: `delete` on a genuinely non-deletable operand (a plain int expression
// used directly, with no array/element context at all) is unaffected by the value-type-no-op
// relaxation where it doesn't apply -- the relaxation only ever fires through an actual generic
// T substitution site, not for e.g. `delete 5;`-shaped nonsense. (`delete` on a bare int literal
// is syntactically an expression-statement operand like any other, so this doubles as a check
// that the parser/semantic layer still accepts it as the harmless no-op fix 1 intends -- there is
// no separate "not deletable at all" residual case left once null/reference/string/pointer/value
// are all covered.)
// ---------------------------------------------------------------------------------------

void testDeleteValueTypeNoOpDoesNotSkipBoundsCheck(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "DeleteBoundsCheck";
    writeProject(directory, "DeleteBoundsCheck");
    writeFile(directory / "Main.ns", R"NS(
namespace DeleteBoundsCheck
{
    public class IntBox<T>
    {
        private T[] items;
        public IntBox(int capacity) { items = new T[capacity]; }

        // delete on a value-typed element must still evaluate the index expression (so an
        // out-of-range index still traps at runtime) even though no free call is emitted.
        public void DeleteAt(int index) { delete items[index]; }
    }

    public class Runner
    {
        public static int Main()
        {
            IntBox<int> box = new IntBox<int>(4);
            box.DeleteAt(0);
            return 0;
        }
    }
}
)NS");

    const std::string llvm = compileAndEmitObject(tests, directory, "delete on value-typed array element");
    if (llvm.empty()) return;

    // Scope the check to DeleteAt() itself -- the module as a whole legitimately declares and
    // may call __nativesharp_free elsewhere (e.g. the constructor's own exception-cleanup path
    // for `new T[capacity]`), which is unrelated to this fix.
    const std::string deleteAtBody = extractFunctionBody(llvm, "DeleteAt");
    tests.expect(!deleteAtBody.empty(), "DeleteAt() function body found in the emitted LLVM IR");
    tests.expect(deleteAtBody.find("native.index") != std::string::npos,
        "delete on a value-typed array element still emits the array bounds check");
    tests.expect(deleteAtBody.find("__nativesharp_free") == std::string::npos,
        "delete on a value-typed array element emits no spurious free call inside DeleteAt() itself");
}

// ---------------------------------------------------------------------------------------
// Fix 5: `return` lexically inside a try block that has a catch clause must not crash the LLVM
// lowerer, whether or not the protected body actually throws. Covers both the "no throw
// reachable at all" shape (a plain `return` inside try/catch) and the "throw is reachable but
// does not fire on this path" shape (the one Dictionary<TKey,TValue>'s KeyNotFoundException-
// throwing indexer needs: `get { if (!found) { throw ...; } return value; }` called from inside
// a caller's try/catch).
// ---------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------
// Known-limitation regression guard (see the file header): a struct over 8 bytes is legal as an
// array ELEMENT TYPE, accessed only through direct field access on the indexed element, but is
// rejected as a local variable, a call argument, or a return value. Both halves are pinned down
// here -- the working array-element-field-access shape (Dictionary<TKey,TValue>.ToArray()'s own
// pattern) actually compiles and lowers cleanly, and the standalone-local shape is confirmed to
// still fail with the expected diagnostic rather than silently miscompiling -- with a wholly
// NON-generic struct (two plain `long` fields, no reference type involved at all), proving this
// is general struct-representation behavior, not anything specific to generics or ownership.
// ---------------------------------------------------------------------------------------

void testOversizedStructWorksAsArrayElementButNotAsAStandaloneValue(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "OversizedStructArrayElement";
        writeProject(directory, "OversizedStructArrayElement");
        writeFile(directory / "Main.ns", R"NS(
namespace OversizedStructArrayElement
{
    // 16 bytes -- over the 8-byte direct-struct-value ABI limit -- with no generic type and no
    // reference type involved at all.
    public struct TwoLongs
    {
        public long A;
        public long B;
    }

    public class Runner
    {
        public static int Main()
        {
            TwoLongs[] items = new TwoLongs[2];
            items[0].A = 1;
            items[0].B = 2;
            items[1].A = 3;
            items[1].B = 4;
            long sum = items[0].A + items[0].B + items[1].A + items[1].B;
            delete[] items;
            return (int)sum;
        }
    }
}
)NS");

        const std::string llvm = compileAndEmitObject(
            tests, directory, "over-8-byte struct as an array element, field access only");
        tests.expect(!llvm.empty(),
            "an over-8-byte struct used only via array-element field access compiles and lowers cleanly");
    }

    {
        const auto directory = root / "OversizedStructStandaloneLocal";
        writeProject(directory, "OversizedStructStandaloneLocal");
        writeFile(directory / "Main.ns", R"NS(
namespace OversizedStructStandaloneLocal
{
    public struct TwoLongs
    {
        public long A;
        public long B;
    }

    public class Runner
    {
        public static int Main()
        {
            TwoLongs p;
            p.A = 1;
            p.B = 2;
            return (int)p.A;
        }
    }
}
)NS");

        ProjectCompilation mir;
        std::string error;
        const bool lowered = mir.compileProjectOopMir(directory / "OversizedStructStandaloneLocal.nsproj", error);
        tests.expect(lowered, "an over-8-byte struct standalone local still reaches OOP MIR (no earlier-stage rejection)");
        if (lowered && mir.nameLookup() != nullptr && mir.oopMirModel() != nullptr)
        {
            nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
            auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
            // HISTORY: this used to assert the standalone 16-byte local was REJECTED by the
            // Stage 40 direct-struct-value check -- the documented limitation, pinned so it
            // could not become a silent miscompile. The limitation no longer exists: the local
            // is memory-homed in a [16 x i8] carrier and lowers cleanly (Win64 indirect
            // aggregate ABI round), so the assertion flips to acceptance.
            tests.expect(module != nullptr && lowerer.error().empty(),
                "an over-8-byte struct standalone local now lowers through the [N x i8] carrier (" +
                    lowerer.error() + ")");
        }
    }
}

void testReturnInsideTryCatchDoesNotCrashLlvmLowering(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "ReturnInTryCatch";
    writeProject(directory, "ReturnInTryCatch");
    writeFile(directory / "Main.ns", R"NS(
namespace NativeSharp.Core
{
    // A self-contained stand-in: this test fixture has no ProjectReference to the real
    // lib/NativeSharp.Core package, and only needs the throw/catch root-type name to exist.
    public class Exception { }
}

using NativeSharp.Core;

namespace ReturnInTryCatch
{
    public class Boom : Exception { }

    public class Runner
    {
        // No throw/rethrow anywhere in this function, and no RAII/using cleanup either -- the
        // *only* thing that made this shape "exceptional" before fix 5 was the catch clause
        // itself, which containsExceptionalControl() did not check for.
        public static int PlainReturnInsideTry()
        {
            try
            {
                return 5;
            }
            catch (Boom b)
            {
                return -1;
            }
        }

        // The Dictionary<TKey,TValue> indexer shape: a throw is reachable in the callee, but not
        // taken on this call path, and the caller's return-of-the-call is itself inside a
        // try/catch.
        public static int MaybeThrow(bool doThrow)
        {
            if (doThrow)
            {
                throw new Boom();
            }
            return 7;
        }

        public static int CallerReturnsThroughTry()
        {
            try
            {
                return MaybeThrow(false);
            }
            catch (Boom b)
            {
                return -1;
            }
        }

        public static int Main()
        {
            return PlainReturnInsideTry() + CallerReturnsThroughTry();
        }
    }
}
)NS");

    const std::string llvm = compileAndEmitObject(tests, directory, "return inside try/catch (fix 5)");
    tests.expect(!llvm.empty(), "return-inside-try/catch fixture reaches LLVM IR without the Stage 45.10 spill-slot crash");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage52_generic_collections_compiler_fixes_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testGenericOwnedArrayAddAndClearForBothValueAndReferenceT(tests, root);
    testMoveFromArrayElementForBothValueAndReferenceT(tests, root);
    testMoveStillRejectsNonNativeArrayIndexSource(tests, root);
    testCrossProjectGenericDelegateAndClassInstantiation(tests, root);
    testCrossProjectClosedGenericReturnKeepsOneIdentity(tests, root);
    testCrossProjectGenericConstraintWithConsumerType(tests, root);
    testCrossProjectGenericConstraintWithGlobalConsumerType(tests, root);
    testCrossProjectGenericConstraintKeepsHomonymousConsumersDistinct(tests, root);
    testCrossProjectGenericConstraintRejectsAtConsumerSite(tests, root);
    testMoveIntoArrayElementAssignmentTarget(tests, root);
    testDefaultValueExpressionForBothValueAndReferenceT(tests, root);
    testDefaultOfVoidIsRejected(tests, root);
    testMoveOfOwnedParameterAsCallArgumentForBothValueAndReferenceT(tests, root);
    testOversizedStructWorksAsArrayElementButNotAsAStandaloneValue(tests, root);
    testDeleteValueTypeNoOpDoesNotSkipBoundsCheck(tests, root);
    testReturnInsideTryCatchDoesNotCrashLlvmLowering(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
