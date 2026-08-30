#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::frontend::MethodCleanupContinuationKind;
using nativesharp::frontend::MethodCleanupRegionKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectOopHirNode;
using nativesharp::frontend::ProjectOopHirNodeKind;
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

void collectNodes(
    const ProjectOopHirNode* node,
    std::vector<const ProjectOopHirNode*>& output)
{
    if (node == nullptr)
    {
        return;
    }
    output.push_back(node);
    for (const auto& child : node->children)
    {
        collectNodes(child.get(), output);
    }
}

std::size_t countKind(
    const std::vector<const ProjectOopHirNode*>& nodes,
    ProjectOopHirNodeKind kind)
{
    return static_cast<std::size_t>(std::ranges::count_if(nodes, [kind](const ProjectOopHirNode* node) {
        return node != nullptr && node->kind == kind;
    }));
}

const ProjectOopHirNode* findNode(
    const std::vector<const ProjectOopHirNode*>& nodes,
    ProjectOopHirNodeKind kind,
    const auto& predicate)
{
    const auto found = std::ranges::find_if(nodes, [&](const ProjectOopHirNode* node) {
        return node != nullptr && node->kind == kind && predicate(*node);
    });
    return found == nodes.end() ? nullptr : *found;
}

bool cleanupPathKinds(
    const ProjectOopHirNode& node,
    const nativesharp::frontend::ProjectMethodBodyModel& methodBody,
    std::initializer_list<MethodCleanupRegionKind> expected)
{
    if (node.cleanupRegionOrdinals.size() != expected.size())
    {
        return false;
    }
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

void testExceptionalHir(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "ExceptionalHir";
    writeProject(directory, "ExceptionalHir", R"NS(
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
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopHir(directory / "ExceptionalHir.nsproj", error);
    tests.expect(ok, "Stage 45.6 exception/cleanup sample lowers through project OOP HIR");
    tests.expect(error.empty(), "Stage 45.6 OOP HIR reports no internal lowering error");
    tests.expect(compilation.oopHirModel() != nullptr && compilation.methodBodyModel() != nullptr,
        "Stage 45.6 retains semantic cleanup bindings and the lowered OOP HIR");
    if (!ok || compilation.oopHirModel() == nullptr || compilation.methodBodyModel() == nullptr)
    {
        return;
    }

    const auto& hir = *compilation.oopHirModel();
    const auto& methodBody = *compilation.methodBodyModel();
    std::vector<const ProjectOopHirNode*> nodes;
    for (const auto& function : hir.functions())
    {
        collectNodes(function.body.get(), nodes);
    }

    tests.expect(countKind(nodes, ProjectOopHirNodeKind::TryRegion) == 2,
        "two try statements become explicit HIR try regions");
    tests.expect(countKind(nodes, ProjectOopHirNodeKind::CatchRegion) == 3,
        "typed and catch-all handlers become explicit HIR catch regions");
    tests.expect(countKind(nodes, ProjectOopHirNodeKind::FinallyRegion) == 2,
        "two finally clauses become explicit HIR cleanup regions");
    tests.expect(countKind(nodes, ProjectOopHirNodeKind::UsingRegion) == 2,
        "two using statements become explicit HIR using cleanup regions");
    tests.expect(countKind(nodes, ProjectOopHirNodeKind::Throw) == 2,
        "throw expressions remain first-class exceptional HIR exits");
    tests.expect(countKind(nodes, ProjectOopHirNodeKind::Rethrow) == 1,
        "throw semicolon becomes an explicit rethrow HIR exit");
    tests.expect(countKind(nodes, ProjectOopHirNodeKind::CleanupContinuation) == 4,
        "each finally/using region carries an explicit normal cleanup continuation");

    const ProjectOopHirNode* tryWithHandlers = findNode(
        nodes, ProjectOopHirNodeKind::TryRegion, [](const ProjectOopHirNode& node) {
            return node.cleanupRegionOrdinal.has_value() && node.hirRegionOrdinal.has_value();
        });
    tests.expect(tryWithHandlers != nullptr,
        "try/finally HIR freezes both handler identity and cleanup-region identity");

    const ProjectOopHirNode* typedCatch = findNode(
        nodes, ProjectOopHirNodeKind::CatchRegion, [](const ProjectOopHirNode& node) {
            return !node.catchAll && node.exceptionTypeOrdinal.has_value() && node.localOrdinal.has_value();
        });
    tests.expect(typedCatch != nullptr,
        "typed catch HIR freezes exception RTTI ordinal and catch-local ordinal");
    const ProjectOopHirNode* catchAll = findNode(
        nodes, ProjectOopHirNodeKind::CatchRegion, [](const ProjectOopHirNode& node) {
            return node.catchAll && !node.exceptionTypeOrdinal.has_value();
        });
    tests.expect(catchAll != nullptr,
        "catch-all HIR is distinct from a fabricated typed catch");

    const ProjectOopHirNode* usingRegion = findNode(
        nodes, ProjectOopHirNodeKind::UsingRegion, [](const ProjectOopHirNode& node) {
            return node.cleanupRegionOrdinal.has_value() && node.localOrdinal.has_value() &&
                node.disposeContractMemberOrdinal.has_value();
        });
    tests.expect(usingRegion != nullptr,
        "using HIR freezes resource local, cleanup region, and compile-time Dispose contract");
    tests.expect(usingRegion != nullptr && usingRegion->disposeImplementationMemberOrdinal.has_value() &&
            !usingRegion->interfaceCleanupDispatch,
        "class-typed using HIR freezes the concrete Dispose implementation without reflection");

    const ProjectOopHirNode* throwToCatch = findNode(
        nodes, ProjectOopHirNodeKind::Throw, [&](const ProjectOopHirNode& node) {
            return node.exceptionTargetRegionOrdinal.has_value() &&
                cleanupPathKinds(node, methodBody, {MethodCleanupRegionKind::UsingDispose});
        });
    tests.expect(throwToCatch != nullptr,
        "throw inside using unwinds Dispose before transferring to the enclosing catch dispatch");
    tests.expect(throwToCatch != nullptr && throwToCatch->cleanupContinuationKind ==
            MethodCleanupContinuationKind::Exception,
        "throw HIR marks its cleanup continuation reason as exceptional");

    const ProjectOopHirNode* directThrowToCatch = findNode(
        nodes, ProjectOopHirNodeKind::Throw, [](const ProjectOopHirNode& node) {
            return node.exceptionTargetRegionOrdinal.has_value() && node.cleanupRegionOrdinals.empty();
        });
    tests.expect(directThrowToCatch != nullptr,
        "throw directly in try transfers to sibling catch before executing that try's finally");

    const ProjectOopHirNode* rethrow = findNode(
        nodes, ProjectOopHirNodeKind::Rethrow, [&](const ProjectOopHirNode& node) {
            return !node.exceptionTargetRegionOrdinal.has_value() &&
                cleanupPathKinds(node, methodBody,
                    {MethodCleanupRegionKind::UsingDispose, MethodCleanupRegionKind::Finally});
        });
    tests.expect(rethrow != nullptr,
        "rethrow from catch unwinds nested using then enclosing finally when no local handler remains");

    const ProjectOopHirNode* returnThroughUsing = findNode(
        nodes, ProjectOopHirNodeKind::Return, [&](const ProjectOopHirNode& node) {
            return cleanupPathKinds(node, methodBody,
                {MethodCleanupRegionKind::UsingDispose, MethodCleanupRegionKind::Finally});
        });
    tests.expect(returnThroughUsing != nullptr,
        "return inside using/try HIR carries the frozen inner-to-outer cleanup path");
    const ProjectOopHirNode* catchReturn = findNode(
        nodes, ProjectOopHirNodeKind::Return, [&](const ProjectOopHirNode& node) {
            return cleanupPathKinds(node, methodBody, {MethodCleanupRegionKind::Finally});
        });
    tests.expect(catchReturn != nullptr,
        "return from catch HIR still traverses the enclosing finally cleanup");

    const std::string dump = nativesharp::frontend::ProjectOopHirModelDumper::dump(hir);
    tests.expect(dump.find("try-region") != std::string::npos &&
            dump.find("catch-region") != std::string::npos &&
            dump.find("finally-region") != std::string::npos,
        "Stage 45.6 HIR dump exposes protected, handler, and finally regions");
    tests.expect(dump.find("using-region") != std::string::npos &&
            dump.find("dispose-contract=#") != std::string::npos,
        "Stage 45.6 HIR dump exposes deterministic using/Dispose metadata");
    tests.expect(dump.find("exception-target=#") != std::string::npos &&
            dump.find("cleanup-path=#") != std::string::npos,
        "Stage 45.6 HIR dump makes exceptional targets and cleanup edges inspectable");
    tests.expect(dump.find("continuation=exception") != std::string::npos &&
            dump.find("continuation=normal") != std::string::npos,
        "Stage 45.6 HIR dump distinguishes exceptional and normal cleanup continuations");

    ProjectCompilation mirBoundary;
    error.clear();
    const bool mirOk = mirBoundary.compileProjectOopMir(directory / "ExceptionalHir.nsproj", error);
    tests.expect(mirOk,
        "Stage 45.7 consumes the frozen Stage 45.6 exceptional HIR without changing its semantics");
    tests.expect(error.empty() && mirBoundary.oopMirModel() != nullptr,
        "Stage 45.6 HIR remains a valid input contract for Stage 45.7 MIR lowering");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage45_6_hir_exceptional_cfg";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testExceptionalHir(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
