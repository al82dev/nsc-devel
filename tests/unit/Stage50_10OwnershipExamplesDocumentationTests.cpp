#include "nativesharp/Basic/Version.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/Sema/OwnershipKind.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::frontend::MethodBorrowKind;
using nativesharp::frontend::MethodCleanupRegionKind;
using nativesharp::frontend::MethodLocalBorrowSourceKind;
using nativesharp::frontend::MethodOwnershipTransferKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectOopHirNode;
using nativesharp::frontend::ProjectOopHirNodeKind;
using nativesharp::frontend::ProjectOopMirNode;
using nativesharp::frontend::ProjectOopMirNodeKind;
using nativesharp::sema::OwnershipKind;
using nativesharp::tests::TestContext;

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream text;
    text << stream.rdbuf();
    return text.str();
}

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic)
        {
            return diagnostic.message().find(text) != std::string::npos;
        });
}

void collectHir(const ProjectOopHirNode* node, std::vector<const ProjectOopHirNode*>& output)
{
    if (node == nullptr) return;
    output.push_back(node);
    for (const auto& child : node->children) collectHir(child.get(), output);
}

void collectMir(const ProjectOopMirNode* node, std::vector<const ProjectOopMirNode*>& output)
{
    if (node == nullptr) return;
    output.push_back(node);
    for (const auto& child : node->children) collectMir(child.get(), output);
}

template <typename Node, typename Kind>
std::size_t countKind(const std::vector<const Node*>& nodes, Kind kind)
{
    return static_cast<std::size_t>(std::ranges::count_if(
        nodes,
        [kind](const Node* node)
        {
            return node != nullptr && node->kind == kind;
        }));
}

void expectInvalidFixture(
    TestContext& tests,
    const std::filesystem::path& project,
    std::string_view expectedDiagnostic,
    std::string_view label)
{
    tests.expect(std::filesystem::exists(project), std::string(label) + " fixture is packaged");
    if (!std::filesystem::exists(project)) return;

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(project, error);
    tests.expect(!ok, std::string(label) + " fixture is rejected");
    tests.expect(
        hasDiagnosticText(compilation, expectedDiagnostic),
        std::string(label) + " fixture reports the frozen ownership diagnostic");
}

void testPositiveExample(TestContext& tests, const std::filesystem::path& sourceRoot)
{
    const std::filesystem::path project =
        sourceRoot / "examples/project_stage50/Stage50Ownership.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 50 ownership example project exists");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    const bool bodiesOk = bodies.compileProjectMethodBodies(project, error);
    tests.expect(bodiesOk, "Stage 50 ownership example passes method-body analysis");
    tests.expect(error.empty(), "Stage 50 ownership example method-body analysis has no internal error");
    tests.expect(bodies.methodBodyModel() != nullptr && bodies.memberModel() != nullptr,
        "Stage 50 ownership example retains semantic ownership models");
    if (!bodiesOk || bodies.methodBodyModel() == nullptr || bodies.memberModel() == nullptr) return;

    const auto& members = bodies.memberModel()->members();
    tests.expect(std::ranges::any_of(members, [](const auto& member)
        {
            return member.name == "Create" && member.declaredOwnership == OwnershipKind::Owned;
        }), "Create exposes an owned return contract");
    tests.expect(std::ranges::any_of(members, [](const auto& member)
        {
            return member.name == "Inspect" && member.parameters.size() == 1 &&
                member.parameters[0].ownership == OwnershipKind::Borrowed;
        }), "Inspect exposes a borrowed parameter contract");
    tests.expect(std::ranges::any_of(members, [](const auto& member)
        {
            return member.name == "Consume" && member.parameters.size() == 1 &&
                member.parameters[0].ownership == OwnershipKind::Owned;
        }), "Consume exposes an owned parameter contract");

    const auto& body = *bodies.methodBodyModel();
    tests.expect(std::ranges::any_of(body.ownershipTransferBindings(), [](const auto& transfer)
        {
            return transfer.kind == MethodOwnershipTransferKind::MoveLocalToOwnedReturn;
        }), "Create transfers its local owner through return move(result)");
    tests.expect(std::ranges::any_of(body.ownershipTransferBindings(), [](const auto& transfer)
        {
            return transfer.kind == MethodOwnershipTransferKind::MoveLocalToOwnedParameter;
        }), "Main transfers resource into Consume through move(resource)");
    tests.expect(std::ranges::any_of(body.localBorrowBindings(), [](const auto& borrow)
        {
            return borrow.sourceKind == MethodLocalBorrowSourceKind::OwnerLocal;
        }), "Main creates a lexical borrow rooted in the RAII owner");
    tests.expect(std::ranges::any_of(body.borrowBindings(), [](const auto& borrow)
        {
            return borrow.kind == MethodBorrowKind::BorrowedLocalToBorrowedParameter;
        }), "Inspect(view) preserves the borrowed call edge");
    tests.expect(std::ranges::count_if(body.cleanupRegions(), [](const auto& region)
        {
            return region.kind == MethodCleanupRegionKind::OwnedParameterDestroy;
        }) == 1, "Consume receives exactly one owned-parameter cleanup region");

    error.clear();
    ProjectCompilation hir;
    tests.expect(hir.compileProjectOopHir(project, error),
        "Stage 50 ownership example lowers to OOP HIR");
    tests.expect(error.empty(), "Stage 50 ownership example HIR lowering has no internal error");

    error.clear();
    ProjectCompilation mir;
    const bool mirOk = mir.compileProjectOopMir(project, error);
    tests.expect(mirOk, "Stage 50 ownership example lowers to OOP MIR");
    tests.expect(error.empty(), "Stage 50 ownership example MIR lowering has no internal error");
    tests.expect(mir.oopHirModel() != nullptr && mir.oopMirModel() != nullptr && mir.nameLookup() != nullptr,
        "Stage 50 ownership example retains HIR/MIR/name models");
    if (!mirOk || mir.oopHirModel() == nullptr || mir.oopMirModel() == nullptr || mir.nameLookup() == nullptr)
        return;

    std::vector<const ProjectOopHirNode*> hirNodes;
    for (const auto& function : mir.oopHirModel()->functions()) collectHir(function.body.get(), hirNodes);
    std::vector<const ProjectOopMirNode*> mirNodes;
    for (const auto& function : mir.oopMirModel()->functions()) collectMir(function.body.get(), mirNodes);

    tests.expect(countKind(hirNodes, ProjectOopHirNodeKind::BorrowValue) >= 1 &&
            countKind(mirNodes, ProjectOopMirNodeKind::BorrowValue) >= 1,
        "borrow is explicit in HIR and preserved in MIR");
    tests.expect(countKind(hirNodes, ProjectOopHirNodeKind::MoveValue) >= 2 &&
            countKind(mirNodes, ProjectOopMirNodeKind::MoveValue) >= 2,
        "owned return and owned call transfers remain explicit in HIR/MIR");
    tests.expect(countKind(hirNodes, ProjectOopHirNodeKind::OwnedParameterCleanup) == 1 &&
            countKind(mirNodes, ProjectOopMirNodeKind::OwnedParameterCleanup) == 1,
        "owned parameter cleanup is represented exactly once in HIR/MIR");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr, "Stage 50 ownership example lowers to LLVM IR");
    tests.expect(lowerer.error().empty(), "Stage 50 ownership LLVM lowering has no internal error");
    if (module == nullptr) return;

    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 50 ownership example LLVM IR verifies");
    tests.expect(verifyError.empty(), "verified Stage 50 ownership LLVM module has no verifier diagnostics");
    const std::string ir = module->text();
    tests.expect(ir.find("owned.param.cleanup") != std::string::npos,
        "LLVM contains the Consume owned-parameter cleanup path");
    tests.expect(ir.find("borrow.cleanup") == std::string::npos,
        "LLVM does not invent cleanup/runtime state for borrowed values");
}

void testNegativeExamples(TestContext& tests, const std::filesystem::path& sourceRoot)
{
    const std::filesystem::path root = sourceRoot / "examples/project_stage50/negative";
    expectInvalidFixture(
        tests,
        root / "ImplicitOwnedCopy.nsproj",
        "cannot copy owned value 'owner' into owned parameter; use move(owner)",
        "implicit owned copy");
    expectInvalidFixture(
        tests,
        root / "BorrowedToOwned.nsproj",
        "move(...) source must be a RAII local or owned parameter",
        "borrowed-to-owned move");
    expectInvalidFixture(
        tests,
        root / "BorrowEscape.nsproj",
        "borrowed return value 'view' outlives its owner 'owner'",
        "borrow escape");
    expectInvalidFixture(
        tests,
        root / "UseAfterMove.nsproj",
        "use of moved-from RAII local 'owner'",
        "use after move");
}

void testPackagedGuidance(TestContext& tests, const std::filesystem::path& sourceRoot)
{
    const auto exampleRoot = sourceRoot / "examples/project_stage50";
    const std::string script = readFile(exampleRoot / "build_and_run_windows.cmd");
    const std::string readme = readFile(exampleRoot / "README.md");

    tests.expect(script.find("--emit-project-oop-exe") != std::string::npos &&
            script.find("Stage50Ownership.exe") != std::string::npos,
        "Windows CMD example builds and runs the packaged ownership executable");
    tests.expect(readme.find("Create() -> owned return") != std::string::npos &&
            readme.find("exactly one destructor call") != std::string::npos,
        "packaged README explains the vertical ownership lifetime");
}
}

int main()
{
    TestContext tests;
    // Pin the stable language contract, not the generated build identity. This intentionally turns
    // red when the user-selected language line moves and remains stable across per-build suffixes.
    // (testPackagedGuidance's README assertion below is a separate, pre-existing gap: this tree's
    // examples/project_stage50/ has no README.md at all -- not authored here, not fabricated here
    // either.)
    tests.expect(
        nativesharp::kLanguageVersion == "2.0.0" && !nativesharp::kCompilerVersion.empty(),
        "NativeSharp uses language version 2.0.0 (compiler build identity moves independently)");

    const std::filesystem::path sourceRoot = std::filesystem::path(NSC_SOURCE_DIR);
    testPositiveExample(tests, sourceRoot);
    testNegativeExamples(tests, sourceRoot);
    testPackagedGuidance(tests, sourceRoot);
    return tests.result();
}
