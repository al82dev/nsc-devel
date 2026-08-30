#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::frontend::MethodCleanupContinuationKind;
using nativesharp::frontend::MethodCleanupRegionKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectOopMirNode;
using nativesharp::frontend::ProjectOopMirNodeKind;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name, std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

void collectNodes(const ProjectOopMirNode* node, std::vector<const ProjectOopMirNode*>& output)
{
    if (node == nullptr) return;
    output.push_back(node);
    for (const auto& child : node->children) collectNodes(child.get(), output);
}

std::size_t countKind(
    const std::vector<const ProjectOopMirNode*>& nodes,
    ProjectOopMirNodeKind kind)
{
    return static_cast<std::size_t>(std::ranges::count_if(nodes, [kind](const ProjectOopMirNode* node) {
        return node != nullptr && node->kind == kind;
    }));
}

const ProjectOopMirNode* findNode(
    const std::vector<const ProjectOopMirNode*>& nodes,
    ProjectOopMirNodeKind kind,
    const auto& predicate)
{
    const auto found = std::ranges::find_if(nodes, [&](const ProjectOopMirNode* node) {
        return node != nullptr && node->kind == kind && predicate(*node);
    });
    return found == nodes.end() ? nullptr : *found;
}

bool cleanupPathKinds(
    const ProjectOopMirNode& node,
    const nativesharp::frontend::ProjectMethodBodyModel& methodBody,
    std::initializer_list<MethodCleanupRegionKind> expected)
{
    if (node.cleanupRegionOrdinals.size() != expected.size()) return false;
    std::size_t index = 0;
    for (const MethodCleanupRegionKind kind : expected)
    {
        const std::size_t ordinal = node.cleanupRegionOrdinals[index++];
        if (ordinal >= methodBody.cleanupRegions().size() ||
            methodBody.cleanupRegions()[ordinal].kind != kind)
        {
            return false;
        }
    }
    return true;
}

void testExceptionalMir(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "ExceptionalMir";
    writeProject(directory, "ExceptionalMir", R"NS(
namespace NativeSharp.Core
{
    public class Exception { }
    public interface IDisposable { void Dispose(); }
}
using NativeSharp.Core;

class Problem : Exception { }
class Resource : IDisposable
{
    public void Dispose() { }
}

class Runner
{
    public static int Run(Resource input, bool fail)
    {
        try
        {
            using (Resource resource = input)
            {
                if (fail)
                {
                    throw new Problem();
                }
                return 1;
            }
        }
        catch (Problem problem)
        {
            return 2;
        }
        catch
        {
            return 3;
        }
        finally
        {
            int marker = 0;
        }
    }

    public static void Rethrow(Resource input)
    {
        try
        {
            throw new Problem();
        }
        catch (Problem problem)
        {
            using (Resource resource = input)
            {
                throw;
            }
        }
        finally
        {
            int marker = 0;
        }
    }

    public static void Loop(Resource input, bool leave)
    {
        while (true)
        {
            using (Resource resource = input)
            {
                if (leave)
                {
                    break;
                }
                continue;
            }
        }
    }

    public static void InterfaceCleanup(IDisposable input)
    {
        using (IDisposable resource = input)
        {
            return;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "ExceptionalMir.nsproj", error);
    tests.expect(ok, "Stage 45.7 exception/cleanup sample lowers through project OOP MIR");
    tests.expect(error.empty(), "Stage 45.7 OOP MIR reports no internal lowering error");
    tests.expect(compilation.oopMirModel() != nullptr && compilation.methodBodyModel() != nullptr,
        "Stage 45.7 retains semantic cleanup bindings and the lowered OOP MIR");
    if (!ok || compilation.oopMirModel() == nullptr || compilation.methodBodyModel() == nullptr) return;

    const auto& mir = *compilation.oopMirModel();
    const auto& methodBody = *compilation.methodBodyModel();
    std::vector<const ProjectOopMirNode*> nodes;
    for (const auto& function : mir.functions()) collectNodes(function.body.get(), nodes);

    tests.expect(countKind(nodes, ProjectOopMirNodeKind::TryRegion) == 2,
        "MIR retains two explicit protected try regions");
    tests.expect(countKind(nodes, ProjectOopMirNodeKind::CatchRegion) == 3,
        "MIR retains typed and catch-all handler regions");
    tests.expect(countKind(nodes, ProjectOopMirNodeKind::FinallyRegion) == 2,
        "MIR retains two explicit finally cleanup regions");
    tests.expect(countKind(nodes, ProjectOopMirNodeKind::UsingRegion) == 4,
        "MIR retains every deterministic using cleanup region");
    tests.expect(countKind(nodes, ProjectOopMirNodeKind::Throw) == 2,
        "MIR keeps throw expressions as exceptional transfers");
    tests.expect(countKind(nodes, ProjectOopMirNodeKind::Rethrow) == 1,
        "MIR keeps rethrow distinct from throw-with-value");
    tests.expect(countKind(nodes, ProjectOopMirNodeKind::CleanupContinuation) == 6,
        "MIR carries normal continuations for every finally/using region");

    const ProjectOopMirNode* tryWithHandlers = findNode(
        nodes, ProjectOopMirNodeKind::TryRegion, [](const ProjectOopMirNode& node) {
            return node.cleanupRegionOrdinal.has_value() && node.mirRegionOrdinal.has_value();
        });
    tests.expect(tryWithHandlers != nullptr,
        "try/finally MIR freezes handler identity and cleanup-region identity");

    const ProjectOopMirNode* typedCatch = findNode(
        nodes, ProjectOopMirNodeKind::CatchRegion, [](const ProjectOopMirNode& node) {
            return !node.catchAll && node.exceptionTypeOrdinal.has_value() && node.localOrdinal.has_value();
        });
    tests.expect(typedCatch != nullptr,
        "typed catch MIR freezes RTTI ordinal and catch-local ordinal");
    const ProjectOopMirNode* catchAll = findNode(
        nodes, ProjectOopMirNodeKind::CatchRegion, [](const ProjectOopMirNode& node) {
            return node.catchAll && !node.exceptionTypeOrdinal.has_value();
        });
    tests.expect(catchAll != nullptr,
        "catch-all MIR stays distinct from a fabricated typed catch");

    const ProjectOopMirNode* directUsing = findNode(
        nodes, ProjectOopMirNodeKind::UsingRegion, [](const ProjectOopMirNode& node) {
            return node.cleanupRegionOrdinal.has_value() && node.localOrdinal.has_value() &&
                node.disposeContractMemberOrdinal.has_value() &&
                node.disposeImplementationMemberOrdinal.has_value() &&
                !node.interfaceCleanupDispatch;
        });
    tests.expect(directUsing != nullptr,
        "class-typed using MIR freezes concrete Dispose implementation and cleanup region");
    const ProjectOopMirNode* interfaceUsing = findNode(
        nodes, ProjectOopMirNodeKind::UsingRegion, [](const ProjectOopMirNode& node) {
            return node.interfaceCleanupDispatch && node.disposeContractMemberOrdinal.has_value() &&
                !node.disposeImplementationMemberOrdinal.has_value();
        });
    tests.expect(interfaceUsing != nullptr,
        "interface-typed using MIR preserves interface cleanup dispatch without reflection");

    const ProjectOopMirNode* throwToCatch = findNode(
        nodes, ProjectOopMirNodeKind::Throw, [&](const ProjectOopMirNode& node) {
            return node.exceptionTargetRegionOrdinal.has_value() &&
                cleanupPathKinds(node, methodBody, {MethodCleanupRegionKind::UsingDispose});
        });
    tests.expect(throwToCatch != nullptr,
        "throw inside using MIR routes Dispose before the enclosing catch dispatch");
    tests.expect(throwToCatch != nullptr &&
            throwToCatch->cleanupContinuationKind == MethodCleanupContinuationKind::Exception,
        "throw MIR explicitly labels its cleanup continuation as exceptional");

    const ProjectOopMirNode* directThrowToCatch = findNode(
        nodes, ProjectOopMirNodeKind::Throw, [](const ProjectOopMirNode& node) {
            return node.exceptionTargetRegionOrdinal.has_value() && node.cleanupRegionOrdinals.empty();
        });
    tests.expect(directThrowToCatch != nullptr,
        "direct throw MIR targets catch before the sibling finally cleanup");

    const ProjectOopMirNode* rethrow = findNode(
        nodes, ProjectOopMirNodeKind::Rethrow, [&](const ProjectOopMirNode& node) {
            return !node.exceptionTargetRegionOrdinal.has_value() &&
                cleanupPathKinds(node, methodBody,
                    {MethodCleanupRegionKind::UsingDispose, MethodCleanupRegionKind::Finally});
        });
    tests.expect(rethrow != nullptr,
        "rethrow MIR unwinds nested using then enclosing finally toward caller");

    const ProjectOopMirNode* returnThroughUsing = findNode(
        nodes, ProjectOopMirNodeKind::Return, [&](const ProjectOopMirNode& node) {
            return cleanupPathKinds(node, methodBody,
                {MethodCleanupRegionKind::UsingDispose, MethodCleanupRegionKind::Finally});
        });
    tests.expect(returnThroughUsing != nullptr,
        "return MIR carries its inner-to-outer cleanup path");
    const ProjectOopMirNode* catchReturn = findNode(
        nodes, ProjectOopMirNodeKind::Return, [&](const ProjectOopMirNode& node) {
            return cleanupPathKinds(node, methodBody, {MethodCleanupRegionKind::Finally});
        });
    tests.expect(catchReturn != nullptr,
        "return from catch MIR still traverses the enclosing finally");

    const ProjectOopMirNode* breakThroughUsing = findNode(
        nodes, ProjectOopMirNodeKind::Break, [&](const ProjectOopMirNode& node) {
            return cleanupPathKinds(node, methodBody, {MethodCleanupRegionKind::UsingDispose}) &&
                node.cleanupContinuationKind == MethodCleanupContinuationKind::Break &&
                node.targetScopeOrdinal.has_value();
        });
    tests.expect(breakThroughUsing != nullptr,
        "break MIR freezes its using cleanup edge and loop target scope");
    const ProjectOopMirNode* continueThroughUsing = findNode(
        nodes, ProjectOopMirNodeKind::Continue, [&](const ProjectOopMirNode& node) {
            return cleanupPathKinds(node, methodBody, {MethodCleanupRegionKind::UsingDispose}) &&
                node.cleanupContinuationKind == MethodCleanupContinuationKind::Continue &&
                node.targetScopeOrdinal.has_value();
        });
    tests.expect(continueThroughUsing != nullptr,
        "continue MIR freezes its using cleanup edge and loop target scope");

    const std::string dump = nativesharp::frontend::ProjectOopMirModelDumper::dump(mir);
    tests.expect(dump.find("try-region") != std::string::npos &&
            dump.find("catch-region") != std::string::npos &&
            dump.find("finally-region") != std::string::npos,
        "Stage 45.7 MIR dump exposes protected, handler, and finally regions");
    tests.expect(dump.find("using-region") != std::string::npos &&
            dump.find("dispose-contract=#") != std::string::npos &&
            dump.find("cleanup-dispatch=interface") != std::string::npos,
        "Stage 45.7 MIR dump exposes deterministic direct/interface Dispose cleanup metadata");
    tests.expect(dump.find("mir-region=#") != std::string::npos &&
            dump.find("exception-target=#") != std::string::npos &&
            dump.find("cleanup-path=#") != std::string::npos,
        "Stage 45.7 MIR dump makes handler and cleanup CFG edges inspectable");
    tests.expect(dump.find("continuation=exception") != std::string::npos &&
            dump.find("continuation=return") != std::string::npos &&
            dump.find("continuation=break") != std::string::npos &&
            dump.find("continuation=continue") != std::string::npos,
        "Stage 45.7 MIR dump preserves exceptional and structured-exit continuation reasons");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), mir);
    tests.expect(module != nullptr,
        "Stage 45.11 lowers the full inherited exceptional MIR sample through typed-catch RTTI");
    tests.expect(lowerer.error().empty(),
        "Stage 45.11 removes the final MIR-to-LLVM typed-catch boundary from the Stage 45.7 sample");
    if (module != nullptr)
    {
        std::string verifyError;
        tests.expect(module->verify(verifyError),
            "full inherited Stage 45.7 exceptional MIR verifies after Stage 45.11 LLVM lowering");
    }
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage45_7_mir_exceptional_cfg";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testExceptionalMir(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
