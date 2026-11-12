#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirNode.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess final
{
    static std::size_t restoredPolls(const ProjectOopMirModel& model)
    {
        std::size_t result = 0;
        const auto count = std::min(model.functions_.size(), model.mirCachePayloads_.size());
        for (std::size_t i = 0; i < count; ++i)
            if (model.functions_[i].mangledName.find("$moveNext$poll") != std::string::npos &&
                model.mirCachePayloads_[i].has_value()) ++result;
        return result;
    }
};
}

using namespace nativesharp::frontend;

struct Pending final
{
    const ProjectOopMirNode* node;
    std::string path;
    bool inYieldOperand;
};

bool writeFixture(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    std::ofstream source(directory / "Main.ns", std::ios::binary);
    source << R"NS(using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
class Payload { ~Payload() {} }
class Program { public static Payload Shared;
 public static owned Payload Make() { return new Payload(); }
 public static bool Choose(bool value) { return value; }
 public static int InspectString(borrowed string value) { return 0; }
 public static AsyncIterator<Payload, Exception> Gen(bool choose) {
  try { yield return Shared; } finally { yield return Choose(choose) ? Make() : Shared; }
 }
 public static int Main() { return 0; }
}
namespace Helpers {
 public AsyncIterator<Payload, Exception> Gen(bool choose) {
  try { yield return Program.Shared; }
  finally { yield return Program.Choose(choose) ? Program.Make() : Program.Shared; }
 }
 public AsyncIterator<int, Exception> Wrapped() {
  try { yield return 5; } finally { yield return Program.InspectString("a" + "b"); }
 }
}
)NS";
    std::ofstream project(directory / "Probe.nsproj", std::ios::binary);
    project << "<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>"
               "<ItemGroup><Compile Include=\"Main.ns\" /></ItemGroup></Project>";
    return source.good() && project.good();
}

bool reportProducers(const ProjectOopMirModel& model, const ProjectMethodBodyModel& bindings)
{
    std::size_t conditionalCount = 0;
    std::size_t namespaceConditionals = 0;
    std::size_t wrappers = 0;
    std::size_t temporaries = 0;
    for (const auto& function : model.functions())
    {
        if (function.mangledName.find("$moveNext$poll") == std::string::npos) continue;
        std::vector<Pending> pending{{function.body.get(), "root", false}};
        while (!pending.empty())
        {
            Pending item = std::move(pending.back());
            pending.pop_back();
            if (item.node == nullptr) continue;
            for (std::size_t i = item.node->children.size(); i > 0; --i)
                pending.push_back({item.node->children[i - 1].get(), item.path + "/" + std::to_string(i - 1),
                    item.inYieldOperand || (item.node->kind == ProjectOopMirNodeKind::AsyncYieldValue && i == 1)});
            if (item.inYieldOperand && function.namespaceFunctionOrdinal.has_value() &&
                (item.node->kind == ProjectOopMirNodeKind::FullExpressionCleanup ||
                 item.node->kind == ProjectOopMirNodeKind::OwnedTemporaryBorrow))
            {
                if (item.node->kind == ProjectOopMirNodeKind::FullExpressionCleanup) ++wrappers;
                else ++temporaries;
                const auto* binding = item.node->syntax == nullptr ? nullptr :
                    bindings.expressionTypeBinding(item.node->syntax, function.memberOrdinal);
                std::cout << "wrapper " << function.mangledName << ' ' << item.path
                          << " kind=" << static_cast<int>(item.node->kind);
                if (binding == nullptr) std::cout << " UNKNOWN\n";
                else std::cout << " type=" << binding->typeKey << '\n';
            }
            if (item.node->kind != ProjectOopMirNodeKind::AsyncYieldValue || item.node->children.size() != 1 ||
                !item.node->children[0] || item.node->children[0]->kind != ProjectOopMirNodeKind::Conditional)
                continue;
            const auto* conditional = item.node->children[0].get();
            if (conditional->children.size() != 3 || !conditional->children[1] || !conditional->children[2])
                return false;
            ++conditionalCount;
            if (function.namespaceFunctionOrdinal.has_value()) ++namespaceConditionals;
            for (std::size_t branch = 1; branch <= 2; ++branch)
            {
                const auto* producer = conditional->children[branch].get();
                const auto* binding = producer->syntax == nullptr ? nullptr :
                    bindings.expressionTypeBinding(producer->syntax, function.memberOrdinal);
                std::cout << "producer " << function.mangledName << ' ' << item.path << "/0/" << branch;
                if (binding == nullptr) { std::cout << " UNKNOWN\n"; return false; }
                std::cout << " type=" << binding->typeKey << " ownership=" << static_cast<int>(binding->ownership)
                          << " transfer=" << binding->ownershipTransfer << " fresh=" << binding->ownershipFreshOwner
                          << " object=" << static_cast<int>(binding->objectLifetimeOwnership)
                          << " storage=" << static_cast<int>(binding->storageOwnership) << '\n';
            }
        }
    }
    return conditionalCount == 2 && namespaceConditionals == 1 && wrappers > 0 && temporaries > 0;
}

int main(int argc, char** argv)
{
    if (argc != 3) { std::cerr << "usage: test <shared-directory> <cold|warm>\n"; return 2; }
    const std::filesystem::path root = argv[1];
    const std::string phase = argv[2];
    if (phase != "cold" && phase != "warm") return 2;
    if (phase == "cold" && !writeFixture(root)) return 2;
    ProjectCompilation compilation;
    std::string error;
    if (!compilation.compileProjectOopHir(root / "Probe.nsproj", error) ||
        !compilation.methodBodyModel() || !compilation.oopHirModel() || !compilation.nameLookup() ||
        !compilation.memberModel() || !compilation.objectLayoutModel() || !compilation.virtualDispatchModel() ||
        !compilation.interfaceModel() || !compilation.destructionModel() || !compilation.namespaceFunctionModel())
    { std::cerr << "HIR failed: " << error << '\n'; return 1; }
    ProjectOopMirModel model;
    model.setMirCacheRoot(root / "cache");
    if (!model.build(*compilation.nameLookup(), *compilation.memberModel(), *compilation.objectLayoutModel(),
        *compilation.virtualDispatchModel(), *compilation.interfaceModel(), *compilation.destructionModel(),
        *compilation.oopHirModel(), *compilation.namespaceFunctionModel(), error, 1))
    { std::cerr << "MIR failed: " << error << '\n'; return 1; }
    const std::size_t restored = ProjectOopHirModelTestAccess::restoredPolls(model);
    std::cout << "restored-polls=" << restored << '\n';
    if ((phase == "cold" && restored != 0) || (phase == "warm" && restored == 0)) return 1;
    return reportProducers(model, *compilation.methodBodyModel()) ? 0 : 1;
}
