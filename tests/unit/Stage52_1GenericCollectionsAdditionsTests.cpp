// Stage 52.x -- Regression guard for the P1 "high-value member" additions made in response to
// the external Stage 52 closure review (see claude/STAGE52_FALTANTES_Y_CRITERIOS_DE_CIERRE.md and
// claude/10_stage52_correcciones_cierre.md): List<T>.AddRange/InsertRange/RemoveRange/GetRange/
// FindAll/FindLast/FindLastIndex/EnsureCapacity/TrimExcess, Queue<T>/Stack<T>.Try*/EnsureCapacity/
// TrimExcess/CopyTo/capacity constructor, LinkedList<T>.AddAfter/AddBefore/Find/FindLast/
// Remove(node), Dictionary<TKey,TValue>.TryAdd/EnsureCapacity/TrimExcess, HashSet<T>'s full
// set-theory surface (UnionWith/IntersectWith/ExceptWith/SymmetricExceptWith/IsSubsetOf/
// IsProperSubsetOf/IsSupersetOf/IsProperSupersetOf/Overlaps/SetEquals/RemoveWhere/TryGetValue/
// EnsureCapacity/TrimExcess), the same set-theory surface plus GetViewBetween/Reverse on
// SortedSet<T>, and an explicit `Comparison<TKey>`/`Comparison<T>` constructor on
// SortedList<TKey,TValue>/SortedDictionary<TKey,TValue>/SortedSet<T> (closing the
// Delegates.ns/SortedList doc-vs-code mismatch the review flagged).
//
// Rather than re-deriving small synthetic fixtures for every one of those ~50 new members
// individually, this test compiles the REAL, delivered `examples/project_stage52` project --
// which now exercises every one of them (see its Main.ns `Run*Additions` functions) -- all the
// way through semantic analysis, HIR, MIR, LLVM lowering, module verification, and a real
// Windows x64 COFF object emission + structural verification. This is exactly the same
// end-to-end path `nsc --emit-project-oop-obj` takes for a real build, so a compile failure here
// is a real compiler regression, not a fixture-authoring artifact -- and it is the strongest
// verification available in an environment with no Windows/Wine to actually RUN the resulting
// object (see claude/10_stage52_correcciones_cierre.md's Windows-execution section for the
// full, honest accounting of that specific limitation).

#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <string>

namespace
{
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void testProjectStage52CompilesToVerifiedObject(TestContext& tests, const std::filesystem::path& sourceRoot)
{
    const std::filesystem::path project =
        sourceRoot / "examples/project_stage52/Stage52Collections.nsproj";
    tests.expect(std::filesystem::exists(project), "examples/project_stage52 is packaged");
    if (!std::filesystem::exists(project)) return;

    ProjectCompilation mir;
    std::string error;
    const bool lowered = mir.compileProjectOopMir(project, error);
    tests.expect(lowered, "project_stage52 (with Stage 52.x additions): lowers to OOP MIR");
    tests.expect(error.empty(), "project_stage52: no MIR lowering error (" + error + ")");
    if (!lowered || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "project_stage52: lowers to LLVM (" + lowerer.error() + ")");
    if (module == nullptr) return;

    std::string verifyError;
    tests.expect(module->verify(verifyError), "project_stage52: LLVM module verifies clean (" + verifyError + ")");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "project_stage52: obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError), "project_stage52: applies Windows x64 data layout");

    const auto object = std::filesystem::temp_directory_path() / "nativesharp_stage52_1_additions.obj";
    tests.expect(
        target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "project_stage52: emits a real Windows x64 COFF object");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "project_stage52: object passes structural COFF verification");
    std::error_code cleanupError;
    std::filesystem::remove(object, cleanupError);
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path sourceRoot = std::filesystem::path(NSC_SOURCE_DIR);
    testProjectStage52CompilesToVerifiedObject(tests, sourceRoot);
    return tests.result();
}
