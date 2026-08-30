#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/Sema/OwnershipKind.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::frontend::MethodCleanupRegionKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectOopHirFunction;
using nativesharp::frontend::ProjectOopHirNode;
using nativesharp::frontend::ProjectOopHirNodeKind;
using nativesharp::frontend::ProjectOopMirFunction;
using nativesharp::frontend::ProjectOopMirNode;
using nativesharp::frontend::ProjectOopMirNodeKind;
using nativesharp::sema::OwnershipKind;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory)
{
    writeFile(
        directory / "Stage50_8.nsproj",
        "<Project><PropertyGroup><Name>Stage50_8</Name><OutputType>StaticLibrary</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(
        directory / "Main.ns",
        R"NS(
class Resource
{
    public int Value;
    public Resource(int value) { Value = value; }
    public int Read() { return Value; }
    ~Resource() { Value = 0; }
}

class Api
{
    public static void Inspect(borrowed Resource value)
    {
        int observed = value.Read();
    }

    public static void Consume(owned Resource value)
    {
        Inspect(value);
    }

    public static owned Resource Forward(owned Resource value)
    {
        Inspect(value);
        return move(value);
    }

    public static borrowed Resource Echo(borrowed Resource value)
    {
        return value;
    }

    public static int Run()
    {
        raii Resource first = new Resource(1);
        borrowed Resource view = first;
        Inspect(view);

        raii Resource second = new Resource(2);
        Consume(move(second));

        raii Resource third = new Resource(3);
        raii Resource fourth = Forward(move(third));
        borrowed Resource finalView = fourth;
        Inspect(finalView);
        return fourth.Read();
    }
}
)NS");
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
    return static_cast<std::size_t>(std::ranges::count_if(nodes, [kind](const Node* node)
    {
        return node != nullptr && node->kind == kind;
    }));
}

const ProjectOopHirFunction* hirFunctionNamed(
    const ProjectCompilation& compilation,
    std::string_view name)
{
    if (compilation.oopHirModel() == nullptr || compilation.memberModel() == nullptr) return nullptr;
    const auto& members = compilation.memberModel()->members();
    for (const ProjectOopHirFunction& function : compilation.oopHirModel()->functions())
    {
        if (function.memberOrdinal < members.size() && members[function.memberOrdinal].name == name)
            return &function;
    }
    return nullptr;
}

const ProjectOopMirFunction* mirFunctionNamed(
    const ProjectCompilation& compilation,
    std::string_view name)
{
    if (compilation.oopMirModel() == nullptr || compilation.memberModel() == nullptr) return nullptr;
    const auto& members = compilation.memberModel()->members();
    for (const ProjectOopMirFunction& function : compilation.oopMirModel()->functions())
    {
        if (function.memberOrdinal < members.size() && members[function.memberOrdinal].name == name)
            return &function;
    }
    return nullptr;
}

void testOwnershipIr(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "OwnershipIr";
    writeProject(directory);

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage50_8.nsproj", error);
    tests.expect(ok, "Stage 50.8 ownership sample lowers through OOP MIR");
    tests.expect(error.empty(), "Stage 50.8 HIR/MIR lowering has no internal error");
    tests.expect(compilation.methodBodyModel() != nullptr && compilation.memberModel() != nullptr &&
            compilation.nameLookup() != nullptr && compilation.oopHirModel() != nullptr &&
            compilation.oopMirModel() != nullptr,
        "Stage 50.8 retains semantic, HIR and MIR models for ownership inspection");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.oopHirModel() == nullptr ||
        compilation.oopMirModel() == nullptr || compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto& body = *compilation.methodBodyModel();
    const std::size_t ownedParameterCleanups = static_cast<std::size_t>(std::ranges::count_if(
        body.cleanupRegions(), [](const auto& region)
        {
            return region.kind == MethodCleanupRegionKind::OwnedParameterDestroy;
        }));
    tests.expect(ownedParameterCleanups == 2,
        "semantic cleanup model creates exactly one cleanup region for each owned parameter");
    tests.expect(std::ranges::all_of(body.cleanupRegions(), [](const auto& region)
        {
            return region.kind != MethodCleanupRegionKind::OwnedParameterDestroy ||
                (region.resourceParameterOrdinal.has_value() && !region.resourceLocalOrdinal.has_value());
        }),
        "owned-parameter cleanup regions identify parameter storage, never local storage");

    std::vector<const ProjectOopHirNode*> hirNodes;
    for (const auto& function : compilation.oopHirModel()->functions()) collectHir(function.body.get(), hirNodes);
    std::vector<const ProjectOopMirNode*> mirNodes;
    for (const auto& function : compilation.oopMirModel()->functions()) collectMir(function.body.get(), mirNodes);

    tests.expect(countKind(hirNodes, ProjectOopHirNodeKind::OwnedParameterCleanup) == 2,
        "HIR materializes one entry cleanup region per owned parameter");
    tests.expect(countKind(mirNodes, ProjectOopMirNodeKind::OwnedParameterCleanup) == 2,
        "MIR preserves owned-parameter cleanup identity exactly once");
    tests.expect(countKind(hirNodes, ProjectOopHirNodeKind::BorrowValue) >= 5,
        "HIR explicitly materializes borrow operations at local, call and return boundaries");
    tests.expect(countKind(mirNodes, ProjectOopMirNodeKind::BorrowValue) >= 5,
        "MIR preserves explicit borrow operations without turning them into owners");
    tests.expect(std::ranges::any_of(hirNodes, [](const ProjectOopHirNode* node)
        {
            return node != nullptr && node->kind == ProjectOopHirNodeKind::MoveValue &&
                node->parameterOrdinal.has_value() && !node->localOrdinal.has_value();
        }),
        "HIR represents move(ownedParameter) with parameter provenance");
    tests.expect(std::ranges::any_of(mirNodes, [](const ProjectOopMirNode* node)
        {
            return node != nullptr && node->kind == ProjectOopMirNodeKind::MoveValue &&
                node->parameterOrdinal.has_value() && !node->localOrdinal.has_value();
        }),
        "MIR preserves move(ownedParameter) as a real ownership transfer");

    const ProjectOopHirFunction* inspectHir = hirFunctionNamed(compilation, "Inspect");
    const ProjectOopHirFunction* consumeHir = hirFunctionNamed(compilation, "Consume");
    const ProjectOopHirFunction* forwardHir = hirFunctionNamed(compilation, "Forward");
    const ProjectOopHirFunction* echoHir = hirFunctionNamed(compilation, "Echo");
    tests.expect(inspectHir != nullptr && consumeHir != nullptr && forwardHir != nullptr && echoHir != nullptr,
        "HIR exposes all Stage 50.8 fixture functions");
    if (inspectHir != nullptr && consumeHir != nullptr && forwardHir != nullptr && echoHir != nullptr)
    {
        tests.expect(inspectHir->parameterTypeKeys == consumeHir->parameterTypeKeys,
            "owned and borrowed parameter contracts retain the same physical ABI type key");
        tests.expect(inspectHir->parameterOwnerships.size() == 1 &&
                inspectHir->parameterOwnerships[0] == OwnershipKind::Borrowed,
            "HIR retains borrowed parameter ownership metadata");
        tests.expect(consumeHir->parameterOwnerships.size() == 1 &&
                consumeHir->parameterOwnerships[0] == OwnershipKind::Owned,
            "HIR retains owned parameter ownership metadata");
        tests.expect(forwardHir->returnOwnership == OwnershipKind::Owned,
            "HIR retains an owned return contract independently from the physical type key");
        tests.expect(echoHir->returnOwnership == OwnershipKind::Borrowed,
            "HIR retains a borrowed return contract independently from the physical type key");
    }

    const ProjectOopMirFunction* forwardMir = mirFunctionNamed(compilation, "Forward");
    const ProjectOopMirFunction* echoMir = mirFunctionNamed(compilation, "Echo");
    tests.expect(forwardMir != nullptr && echoMir != nullptr &&
            forwardMir->returnOwnership == OwnershipKind::Owned &&
            echoMir->returnOwnership == OwnershipKind::Borrowed,
        "MIR preserves ownership-qualified callable contracts");

    const std::string hirDump = nativesharp::frontend::ProjectOopHirModelDumper::dump(*compilation.oopHirModel());
    const std::string mirDump = nativesharp::frontend::ProjectOopMirModelDumper::dump(*compilation.oopMirModel());
    tests.expect(hirDump.find("owned-parameter-cleanup") != std::string::npos &&
            hirDump.find("borrow-value") != std::string::npos &&
            hirDump.find("return-ownership=owned") != std::string::npos,
        "HIR dump makes Stage 50.8 ownership execution semantics inspectable");
    tests.expect(mirDump.find("owned-parameter-cleanup") != std::string::npos &&
            mirDump.find("borrow-value") != std::string::npos &&
            mirDump.find("return-ownership=borrowed") != std::string::npos,
        "MIR dump makes Stage 50.8 ownership execution semantics inspectable");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr, "Stage 50.8 ownership MIR lowers to LLVM IR");
    tests.expect(lowerer.error().empty(), "Stage 50.8 LLVM lowering reports no internal error");
    if (module == nullptr) return;

    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 50.8 LLVM IR verifies successfully");
    tests.expect(verifyError.empty(), "verified Stage 50.8 LLVM module has no verifier diagnostics");

    const std::string ir = module->text();
    tests.expect(ir.find("owned.param.cleanup") != std::string::npos &&
            ir.find("owned.param.isnull") != std::string::npos &&
            ir.find("owned.param.entry") != std::string::npos,
        "LLVM emits null-aware class-descriptor destruction for owned parameters");
    tests.expect(ir.find("move.param.load") != std::string::npos && ir.find("store ptr null") != std::string::npos,
        "LLVM move(ownedParameter) loads the owned pointer and nulls callee storage before cleanup");
    tests.expect(ir.find("borrow.cleanup") == std::string::npos,
        "borrowed values introduce no LLVM destruction path or hidden lifetime runtime");
    tests.expect(ir.find("personality ptr @__C_specific_handler") != std::string::npos &&
            ir.find("cleanuppad within none") != std::string::npos,
        "owned-parameter cleanups reuse the existing Win64 exceptional cleanup machinery");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage50_8_ownership_ir";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testOwnershipIr(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
