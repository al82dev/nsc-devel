// Stage 52.x -- foreach conformance regression guard.
//
// foreach (see claude/../52_x_nativesharp_foreach.md and the design comment atop
// Parser::parseForeachStatement) is implemented as a parse-time desugaring into two candidate
// subtrees -- array mode (index/`.Length`) and enumerator mode
// (`GetEnumerator()`/`MoveNext()`/`Current()`) -- built entirely out of ordinary, already-tested
// AST node kinds. That means the strongest regression guard is exactly the same "compile a REAL
// example project end-to-end" pattern Stage52_1GenericCollectionsAdditionsTests.cpp already
// established: this test compiles `examples/project_stage52x_foreach`, which exercises BOTH
// candidates (array mode over `int[]`, enumerator mode over a hand-written structural class with
// no interface at all, AND enumerator mode over the real
// NativeSharp.Core.Collections.Generic.List<T> via its new GetEnumerator()/ListEnumerator<T>),
// plus break/continue/an empty collection/nested foreach -- all the way through semantic
// analysis, HIR, MIR, LLVM lowering, module verification, and a real Windows x64 COFF object
// emission + structural verification.
//
// This guards two real bugs found and fixed while building foreach:
//   1) the array-mode candidate's desugaring originally spliced the index increment as a
//      trailing statement after a WhileStatement body, which `continue` (correctly) jumps past --
//      an infinite loop on the first `continue`. Fixed by desugaring to a real ForStatement
//      instead, whose `continue` target IS the increment clause (see the parser's design note).
//   2) ProjectOopLlvmLowerer::emitOwnedParameterDestroy unconditionally treated an `owned`
//      parameter's storage as an 8-byte pointer even when the parameter was `owned` of a VALUE
//      type (legal and common -- e.g. List<T>.Add(owned T value) with T=int) -- corrupting
//      adjacent stack memory. This is the exact root cause WINE_EXECUTION_FINDINGS_STAGE52.md
//      identified in source but never actually fixed; foreach-over-List<int> testing hit it live
//      (List<int>.Count() silently stuck at 1 after the second Add()) and it is fixed here.
// COFF/MIR/HIR verification alone cannot catch either of these (both are real-execution bugs),
// so this project is ALSO run end-to-end under Wine during manual verification (see
// WORKLOG_CURRENT_STATE_52.md-style session notes) -- CI here is limited to the same structural
// verification every other Stage52.x test uses, exactly like
// Stage52_1GenericCollectionsAdditionsTests.cpp's own documented honest accounting of that gap.

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

void testProjectStage52xForeachCompilesToVerifiedObject(TestContext& tests, const std::filesystem::path& sourceRoot)
{
    const std::filesystem::path project =
        sourceRoot / "examples/project_stage52x_foreach/Stage52xForeach.nsproj";
    tests.expect(std::filesystem::exists(project), "examples/project_stage52x_foreach is packaged");
    if (!std::filesystem::exists(project)) return;

    ProjectCompilation mir;
    std::string error;
    const bool lowered = mir.compileProjectOopMir(project, error);
    tests.expect(lowered, "project_stage52x_foreach: lowers to OOP MIR");
    tests.expect(error.empty(), "project_stage52x_foreach: no MIR lowering error (" + error + ")");
    if (!lowered || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "project_stage52x_foreach: lowers to LLVM (" + lowerer.error() + ")");
    if (module == nullptr) return;

    std::string verifyError;
    tests.expect(module->verify(verifyError), "project_stage52x_foreach: LLVM module verifies clean (" + verifyError + ")");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "project_stage52x_foreach: obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError), "project_stage52x_foreach: applies Windows x64 data layout");

    const auto object = std::filesystem::temp_directory_path() / "nativesharp_stage52_3_foreach.obj";
    tests.expect(
        target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "project_stage52x_foreach: emits a real Windows x64 COFF object");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "project_stage52x_foreach: object passes structural COFF verification");
    std::error_code cleanupError;
    std::filesystem::remove(object, cleanupError);
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path sourceRoot = std::filesystem::path(NSC_SOURCE_DIR);
    testProjectStage52xForeachCompilesToVerifiedObject(tests, sourceRoot);
    return tests.result();
}
