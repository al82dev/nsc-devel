#include "nativesharp/Basic/Version.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModelDumper.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

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
using nativesharp::frontend::MethodCleanupRegionKind;
using nativesharp::frontend::MethodStructuredExitKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectOopHirNode;
using nativesharp::frontend::ProjectOopHirNodeKind;
using nativesharp::frontend::ProjectOopMirNode;
using nativesharp::frontend::ProjectOopMirNodeKind;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name><OutputType>StaticLibrary</OutputType></PropertyGroup>"
            "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic) { return diagnostic.message().find(text) != std::string::npos; });
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
    return static_cast<std::size_t>(std::ranges::count_if(nodes, [kind](const Node* node) {
        return node != nullptr && node->kind == kind;
    }));
}

template <typename Node, typename Kind, typename Predicate>
const Node* findNode(const std::vector<const Node*>& nodes, Kind kind, Predicate predicate)
{
    const auto found = std::ranges::find_if(nodes, [&](const Node* node) {
        return node != nullptr && node->kind == kind && predicate(*node);
    });
    return found == nodes.end() ? nullptr : *found;
}

bool cleanupPathContainsKind(
    const std::vector<std::size_t>& ordinals,
    const nativesharp::frontend::ProjectMethodBodyModel& body,
    MethodCleanupRegionKind kind)
{
    return std::ranges::any_of(ordinals, [&](std::size_t ordinal) {
        return ordinal < body.cleanupRegions().size() && body.cleanupRegions()[ordinal].kind == kind;
    });
}

void expectInvalid(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view caseName,
    std::string_view source,
    std::string_view diagnostic)
{
    const std::filesystem::path directory = root / std::string(caseName);
    writeProject(directory, caseName, source);

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / (std::string(caseName) + ".nsproj"), error);
    tests.expect(!ok, std::string(caseName) + " is rejected by Stage 49 sema");
    tests.expect(
        hasDiagnosticText(compilation, diagnostic),
        std::string(caseName) + " reports the Stage 49 ownership diagnostic");
}

std::string positiveSource()
{
    return R"NS(
namespace NativeSharp.Core
{
    public class Exception { }
}
using NativeSharp.Core;

class Problem : Exception { }

class Resource
{
    public int Value;
    public Resource(int value) { Value = value; }
    public int Read() { return Value; }
    ~Resource() { Value = 0; }
}

class Harness
{
    public static int Run(bool choose, bool loop)
    {
        raii Resource a = new Resource(1);
        {
            raii Resource nested = new Resource(2);
            int observed = nested.Read();
        }

        raii Resource b = move(a);
        raii Resource c = null;
        c = move(b);

        if (choose)
        {
            raii Resource branch = move(c);
            c = move(branch);
        }
        else
        {
            int alternate = c.Read();
        }

        while (loop)
        {
            raii Resource roundTrip = move(c);
            c = move(roundTrip);
            loop = false;
        }

        for (raii Resource loopOwner = new Resource(9); choose; choose = false)
        {
            raii Resource iteration = new Resource(10);
            continue;
        }

        return c.Read();
    }

    public static int BreakFlow(bool go)
    {
        while (go)
        {
            raii Resource temporary = new Resource(4);
            break;
        }
        return 1;
    }

    public static Resource Escape()
    {
        raii Resource escapedOwner = new Resource(5);
        return move(escapedOwner);
    }

    public static void Throwing()
    {
        raii Resource protectedOwner = new Resource(6);
        throw new Problem();
    }
}
)NS";
}

void testSemanticModel(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Positive";
    writeProject(directory, "Stage49Positive", positiveSource());

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "Stage49Positive.nsproj", error);
    tests.expect(ok, "Stage 49 positive ownership sample binds through method-body sema");
    tests.expect(error.empty(), "Stage 49 positive method-body compilation has no load/lowering error");
    tests.expect(compilation.methodBodyModel() != nullptr, "Stage 49 exposes a method-body ownership model");
    tests.expect(compilation.memberModel() != nullptr && compilation.nameLookup() != nullptr,
        "Stage 49 retains member/name models required for ownership diagnostics and dumps");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.memberModel() == nullptr ||
        compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto& body = *compilation.methodBodyModel();
    const std::size_t raiiLocals = static_cast<std::size_t>(std::ranges::count_if(body.locals(), [](const auto& local) {
        return local.isRaii;
    }));
    tests.expect(raiiLocals == 11, "Stage 49 freezes eleven lexical RAII owner locals in the positive sample");
    tests.expect(body.moveBindings().size() == 7, "Stage 49 freezes seven explicit ownership transfers");
    tests.expect(std::ranges::all_of(body.moveBindings(), [](const auto& move) {
        return move.syntax != nullptr && move.typeKey.find("Resource") != std::string::npos;
    }), "every move binding retains syntax identity and the moved resource type");

    const std::size_t raiiCleanups = static_cast<std::size_t>(std::ranges::count_if(body.cleanupRegions(), [](const auto& region) {
        return region.kind == MethodCleanupRegionKind::RaiiDestroy;
    }));
    tests.expect(raiiCleanups == raiiLocals, "every RAII owner local receives exactly one lexical destroy region");
    tests.expect(std::ranges::all_of(body.cleanupRegions(), [](const auto& region) {
        return region.kind != MethodCleanupRegionKind::RaiiDestroy || region.resourceLocalOrdinal.has_value();
    }), "RAII cleanup regions identify the local storage they conditionally destroy");

    tests.expect(std::ranges::any_of(body.structuredExits(), [&](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Return &&
            cleanupPathContainsKind(exit.cleanupRegionOrdinals, body, MethodCleanupRegionKind::RaiiDestroy);
    }), "return paths carry pending RAII destruction in their cleanup path");
    tests.expect(std::ranges::any_of(body.structuredExits(), [&](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Break &&
            cleanupPathContainsKind(exit.cleanupRegionOrdinals, body, MethodCleanupRegionKind::RaiiDestroy);
    }), "break paths destroy RAII owners whose lexical scope is exited");
    tests.expect(std::ranges::any_of(body.structuredExits(), [&](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Continue && exit.cleanupRegionOrdinals.size() == 1 &&
            cleanupPathContainsKind(exit.cleanupRegionOrdinals, body, MethodCleanupRegionKind::RaiiDestroy);
    }), "continue destroys the iteration-local owner but retains the for-initializer owner");

    std::ostringstream dump;
    nativesharp::frontend::ProjectMethodBodyModelDumper{}.dump(
        body, *compilation.memberModel(), *compilation.nameLookup(), dump);
    const std::string text = dump.str();
    tests.expect(text.find("raii=true") != std::string::npos,
        "method-body dump explicitly marks RAII locals for didactic inspection");
    tests.expect(text.find("move-bindings=7") != std::string::npos && text.find("source-local=") != std::string::npos,
        "method-body dump exposes explicit move bindings and source-local identity");
    tests.expect(text.find("kind=raii-destroy") != std::string::npos,
        "method-body dump exposes lexical RAII destroy regions");
}

void testHirMirAndLlvm(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Lowering";
    writeProject(directory, "Stage49Lowering", positiveSource());

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage49Lowering.nsproj", error);
    tests.expect(ok, "Stage 49 positive sample lowers through OOP MIR");
    tests.expect(error.empty(), "Stage 49 HIR/MIR lowering reports no internal error");
    tests.expect(compilation.oopHirModel() != nullptr && compilation.oopMirModel() != nullptr &&
            compilation.methodBodyModel() != nullptr && compilation.nameLookup() != nullptr,
        "Stage 49 retains HIR, MIR, ownership model, and name lookup for verification");
    if (!ok || compilation.oopHirModel() == nullptr || compilation.oopMirModel() == nullptr ||
        compilation.methodBodyModel() == nullptr || compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto& hir = *compilation.oopHirModel();
    const auto& mir = *compilation.oopMirModel();
    const auto& body = *compilation.methodBodyModel();

    std::vector<const ProjectOopHirNode*> hirNodes;
    for (const auto& function : hir.functions()) collectHir(function.body.get(), hirNodes);
    std::vector<const ProjectOopMirNode*> mirNodes;
    for (const auto& function : mir.functions()) collectMir(function.body.get(), mirNodes);

    tests.expect(countKind(hirNodes, ProjectOopHirNodeKind::MoveValue) == 7,
        "HIR contains one MoveValue node for each explicit ownership transfer");
    tests.expect(countKind(mirNodes, ProjectOopMirNodeKind::MoveValue) == 7,
        "MIR preserves every ownership transfer as MoveValue");
    tests.expect(std::ranges::count_if(hirNodes, [](const ProjectOopHirNode* node) {
        return node != nullptr && node->kind == ProjectOopHirNodeKind::LocalDeclaration &&
            node->cleanupRegionOrdinal.has_value();
    }) == 11, "HIR RAII local declarations carry stable cleanup-region ordinals");
    tests.expect(std::ranges::count_if(mirNodes, [](const ProjectOopMirNode* node) {
        return node != nullptr && node->kind == ProjectOopMirNodeKind::LocalDeclaration &&
            node->cleanupRegionOrdinal.has_value();
    }) == 11, "MIR RAII local declarations preserve cleanup-region identity");

    const ProjectOopHirNode* throwNode = findNode(
        hirNodes, ProjectOopHirNodeKind::Throw, [](const ProjectOopHirNode&) { return true; });
    tests.expect(throwNode != nullptr, "explicit throw survives into Stage 49 HIR");
    tests.expect(throwNode != nullptr && cleanupPathContainsKind(
            throwNode->cleanupRegionOrdinals, body, MethodCleanupRegionKind::RaiiDestroy),
        "explicit throw HIR now unwinds lexical RAII owners before propagation");

    const ProjectOopMirNode* mirThrow = findNode(
        mirNodes, ProjectOopMirNodeKind::Throw, [](const ProjectOopMirNode&) { return true; });
    tests.expect(mirThrow != nullptr && cleanupPathContainsKind(
            mirThrow->cleanupRegionOrdinals, body, MethodCleanupRegionKind::RaiiDestroy),
        "MIR preserves the explicit-throw RAII cleanup edge");

    const ProjectOopHirNode* moveAssignment = findNode(
        hirNodes, ProjectOopHirNodeKind::Assignment, [](const ProjectOopHirNode& node) {
            return node.booleanValue && node.localOrdinal.has_value();
        });
    tests.expect(moveAssignment != nullptr,
        "HIR marks move-assignment so the old destination owner is destroyed before overwrite");
    const ProjectOopMirNode* mirMoveAssignment = findNode(
        mirNodes, ProjectOopMirNodeKind::Store, [](const ProjectOopMirNode& node) {
            return node.booleanValue && node.localOrdinal.has_value();
        });
    tests.expect(mirMoveAssignment != nullptr,
        "MIR preserves the destroy-before-overwrite marker for move assignment");

    const std::string hirDump = nativesharp::frontend::ProjectOopHirModelDumper::dump(hir);
    const std::string mirDump = nativesharp::frontend::ProjectOopMirModelDumper::dump(mir);
    tests.expect(hirDump.find("move-value") != std::string::npos && hirDump.find("cleanup=#") != std::string::npos,
        "HIR dump makes move values and RAII cleanup identity inspectable");
    tests.expect(mirDump.find("move-value") != std::string::npos && mirDump.find("cleanup=#") != std::string::npos,
        "MIR dump makes move values and RAII cleanup identity inspectable");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), mir);
    tests.expect(module != nullptr, "Stage 49 MIR lowers to LLVM IR");
    tests.expect(lowerer.error().empty(), "Stage 49 LLVM lowering has no internal error");
    if (module == nullptr) return;

    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 49 LLVM IR verifies successfully");
    tests.expect(verifyError.empty(), "verified Stage 49 LLVM IR has no verifier diagnostics");
    const std::string ir = module->text();
    tests.expect(ir.find("%move.load") != std::string::npos && ir.find("store ptr null") != std::string::npos,
        "LLVM move lowering loads the source pointer then nulls moved-from storage");
    tests.expect(ir.find("raii.owner.cleanup") != std::string::npos && ir.find("raii.owner.isnull") != std::string::npos,
        "LLVM RAII cleanup is idempotent and null-aware");
    tests.expect(ir.find("raii.entry") != std::string::npos && ir.find("destruction") == std::string::npos,
        "LLVM RAII cleanup dispatches through the class descriptor destruction entry without host reflection");
    tests.expect(ir.find("personality ptr @__C_specific_handler") != std::string::npos,
        "functions with RAII cleanup participate in the existing Win64 native EH personality");
    tests.expect(ir.find("cleanuppad within none") != std::string::npos,
        "potentially throwing operations receive Win64 cleanup funclets for pending RAII owners");
}

void testPackagedExample(TestContext& tests)
{
    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage49/Stage49RaiiMove.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 49 RAII/move example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    tests.expect(bodies.compileProjectMethodBodies(project, error),
        "packaged Stage 49 example passes method-body ownership analysis");
    tests.expect(error.empty(),
        "packaged Stage 49 method-body analysis has no loader/lowering error");

    error.clear();
    ProjectCompilation hir;
    tests.expect(hir.compileProjectOopHir(project, error),
        "packaged Stage 49 example lowers to OOP HIR");
    tests.expect(error.empty(),
        "packaged Stage 49 HIR lowering has no loader/lowering error");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(project, error),
        "packaged Stage 49 example lowers to OOP MIR");
    tests.expect(error.empty(),
        "packaged Stage 49 MIR lowering has no loader/lowering error");
}

void testInvalidOwnership(TestContext& tests, const std::filesystem::path& root)
{
    const std::string prefix = R"NS(
class Resource
{
    public Resource() { }
    public int Read() { return 1; }
    ~Resource() { }
}
)NS";

    // Stage 51.x note: the diagnostic text grew "or native string" when native strings gained
    // RAII support (`raii string`) -- this Stage 49 test's expected substring was never updated
    // to match, so it had been failing on a stale expected string, not a real regression. Root
    // cause confirmed against src/Frontend/ProjectMethodBodyModel.cpp before touching this.
    expectInvalid(tests, root, "RaiiNeedsReference",
        "class H { public static void F() { raii int value = 1; } }",
        "raii local requires a NativeSharp class/interface reference type or native string");

    expectInvalid(tests, root, "RaiiNeedsInitializer", prefix +
        "class H { public static void F() { raii Resource value; } }",
        "raii local requires an initializer");

    expectInvalid(tests, root, "CopyConstruction", prefix +
        "class H { public static void F() { raii Resource a = new Resource(); raii Resource b = a; } }",
        "RAII ownership cannot be copied or borrowed");

    expectInvalid(tests, root, "CopyIntoNonOwningLocal", prefix +
        "class H { public static void F() { raii Resource a = new Resource(); Resource b = a; } }",
        "cannot be copied into a non-owning local");

    expectInvalid(tests, root, "CopyAssignment", prefix +
        "class H { public static void F() { raii Resource a = new Resource(); Resource b = new Resource(); b = a; } }",
        "cannot be copied by assignment");

    expectInvalid(tests, root, "MoveNeedsOwnerDestination", prefix +
        "class H { public static void F() { raii Resource a = new Resource(); Resource b = move(a); } }",
        "move construction requires a raii destination local");

    expectInvalid(tests, root, "MoveNonOwner", prefix +
        "class H { public static void F() { Resource a = new Resource(); raii Resource b = move(a); } }",
        "move(...) source must be a RAII local");

    expectInvalid(tests, root, "MoveNeedsDirectLocal", prefix +
        "class H { public static void F() { raii Resource a = move(new Resource()); } }",
        "move(...) is only valid for a direct RAII local");

    expectInvalid(tests, root, "UseAfterMove", prefix +
        "class H { public static int F() { raii Resource a = new Resource(); raii Resource b = move(a); return a.Read(); } }",
        "use of moved-from RAII local 'a'");

    expectInvalid(tests, root, "DoubleMove", prefix +
        "class H { public static void F() { raii Resource a = new Resource(); raii Resource b = move(a); raii Resource c = move(a); } }",
        "cannot move from already moved-from RAII local 'a'");

    expectInvalid(tests, root, "ManualDelete", prefix +
        "class H { public static void F() { raii Resource a = new Resource(); delete a; } }",
        "explicit delete of a RAII local is forbidden");

    expectInvalid(tests, root, "SelfMove", prefix +
        "class H { public static void F() { raii Resource a = new Resource(); a = move(a); } }",
        "self move-assignment of a RAII local is not allowed");

    expectInvalid(tests, root, "ReturnRequiresMove", prefix +
        "class H { public static Resource F() { raii Resource a = new Resource(); return a; } }",
        "requires return move(owner) to transfer ownership");

    expectInvalid(tests, root, "BranchJoinUseAfterMove", prefix +
        "class H { public static int F(bool choose) { raii Resource a = new Resource(); if (choose) { raii Resource b = move(a); } else { int ok = a.Read(); } return a.Read(); } }",
        "use of moved-from RAII local 'a'");

    expectInvalid(tests, root, "WhileBackEdge", prefix +
        "class H { public static void F(bool run) { raii Resource a = new Resource(); while (run) { raii Resource b = move(a); } } }",
        "may remain moved across a while-loop back-edge");

    expectInvalid(tests, root, "ForBackEdge", prefix +
        "class H { public static void F(bool run) { raii Resource a = new Resource(); for (int i = 0; run; i = i + 1) { raii Resource b = move(a); } } }",
        "may remain moved across a for-loop back-edge");

    expectInvalid(tests, root, "RaiiConst", prefix +
        "class H { public static void F() { raii const Resource a = new Resource(); } }",
        "raii locals cannot also be const");
}
}

int main()
{
    TestContext tests;
    tests.expect(!nativesharp::kCompilerVersion.empty() && !nativesharp::kCompilerStage.empty(),
        "Stage 49 regression runs against the current NativeSharp compiler identity");

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage49_raii_move";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testSemanticModel(tests, root);
    testHirMirAndLlvm(tests, root);
    testInvalidOwnership(tests, root);
    testPackagedExample(tests);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
