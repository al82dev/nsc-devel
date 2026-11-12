#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirNode.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirNode.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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

std::vector<const ProjectOopMirNode*> nodesByPreorder(const ProjectOopMirNode* root)
{
    std::vector<const ProjectOopMirNode*> result;
    std::vector<const ProjectOopMirNode*> pending;
    if (root != nullptr) pending.push_back(root);
    while (!pending.empty())
    {
        const auto* node = pending.back();
        pending.pop_back();
        result.push_back(node);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            if (*child) pending.push_back(child->get());
    }
    return result;
}

bool reportFacts(const ProjectOopMirModel& model, const ProjectOopHirModel& hir,
    const ProjectMethodBodyModel& bindings, bool expectUnknown)
{
    std::size_t count = 0;
    std::size_t unknown = 0;
    std::size_t genericInt = 0;
    std::size_t genericPayload = 0;
    std::size_t refFacts = 0;
    std::size_t classRef = 0;
    std::size_t namespaceRef = 0;
    std::size_t classMut = 0;
    std::size_t namespaceMut = 0;
    std::size_t unknownMode = 0;
    for (const auto& function : model.functions())
    {
        const auto hirFunctions = hir.functions();
        const auto hirFunction = std::ranges::find_if(hirFunctions, [&](const auto& source) {
            return source.mangledName == function.mangledName;
        });
        if (hirFunction == hirFunctions.end()) return false;
        const auto nodes = nodesByPreorder(function.body.get());
        for (std::size_t i = 0; i < function.asyncYieldFacts.size(); ++i)
        {
            const auto& fact = function.asyncYieldFacts[i];
            if (fact.itemMode != hirFunction->asyncYieldItemMode) return false;
            const auto mode = fact.itemMode;
            const bool isRefMut = function.mangledName.find("GenRefMut") != std::string::npos;
            const bool isRef = !isRefMut && function.mangledName.find("GenRef") != std::string::npos;
            const bool syntheticUnknown = expectUnknown && function.mangledName.find("Wrapped") != std::string::npos;
            const auto expected = syntheticUnknown ? ProjectOopYieldItemMode::Unknown :
                isRefMut ? ProjectOopYieldItemMode::MutableReference :
                isRef ? ProjectOopYieldItemMode::Reference : ProjectOopYieldItemMode::Value;
            if (mode != expected) return false;
            if (mode == ProjectOopYieldItemMode::Unknown) ++unknownMode;
            if (isRef && function.namespaceFunctionOrdinal) ++namespaceRef;
            if (isRef && !function.namespaceFunctionOrdinal) ++classRef;
            if (isRefMut && function.namespaceFunctionOrdinal) ++namespaceMut;
            if (isRefMut && !function.namespaceFunctionOrdinal) ++classMut;
            if (fact.yieldPreorder >= nodes.size() || fact.nodePreorder >= nodes.size()) return false;
            const auto* yield = nodes[fact.yieldPreorder];
            const auto* node = nodes[fact.nodePreorder];
            if (yield->kind != ProjectOopMirNodeKind::AsyncYieldValue || node->kind != fact.nodeKind)
                return false;
            const ProjectOopMirNode* parent = yield;
            if (fact.parentFact.has_value())
            {
                if (*fact.parentFact >= i) return false;
                const auto& parentFact = function.asyncYieldFacts[*fact.parentFact];
                if (parentFact.yieldPreorder != fact.yieldPreorder || parentFact.nodePreorder >= nodes.size())
                    return false;
                parent = nodes[parentFact.nodePreorder];
            }
            if (fact.childIndex >= parent->children.size() || parent->children[fact.childIndex].get() != node)
                return false;
            const auto* binding = node->syntax == nullptr ? nullptr :
                bindings.expressionTypeBinding(node->syntax, function.memberOrdinal);
            if (fact.hasBinding != (binding != nullptr)) return false;
            if (binding != nullptr && (fact.typeKey != binding->typeKey || fact.valueCategory != binding->valueCategory ||
                fact.ownership != binding->ownership || fact.ownershipTransfer != binding->ownershipTransfer ||
                fact.ownershipFreshOwner != binding->ownershipFreshOwner ||
                fact.objectLifetimeOwnership != binding->objectLifetimeOwnership ||
                fact.storageOwnership != binding->storageOwnership)) return false;
            ++count;
            if (function.mangledName.find("GenRef") != std::string::npos) ++refFacts;
            if (function.mangledName.find("Generator") != std::string::npos && fact.typeKey == "builtin:int")
                ++genericInt;
            if (function.mangledName.find("Generator") != std::string::npos &&
                fact.typeKey.find("Payload") != std::string::npos) ++genericPayload;
            std::cout << "fact " << function.mangledName << " yield=" << fact.yieldPreorder
                      << " node=" << fact.nodePreorder << " parent=";
            if (fact.parentFact) std::cout << *fact.parentFact;
            else std::cout << "root";
            std::cout << " child=" << fact.childIndex << " kind=" << static_cast<int>(fact.nodeKind)
                      << " item-mode=" << static_cast<int>(fact.itemMode);
            if (!fact.hasBinding) { ++unknown; std::cout << " UNKNOWN\n"; continue; }
            std::cout << " type=" << fact.typeKey << " category=" << static_cast<int>(fact.valueCategory)
                      << " ownership=" << static_cast<int>(fact.ownership) << " transfer=" << fact.ownershipTransfer
                      << " fresh=" << fact.ownershipFreshOwner << " object="
                      << static_cast<int>(fact.objectLifetimeOwnership) << " storage="
                      << static_cast<int>(fact.storageOwnership) << '\n';
        }
    }
    std::cout << "fact-coverage unknown=" << unknown << " generic-int=" << genericInt
              << " generic-payload=" << genericPayload << " ref=" << refFacts
              << " class-ref=" << classRef << " namespace-ref=" << namespaceRef
              << " class-mut=" << classMut << " namespace-mut=" << namespaceMut
              << " unknown-mode=" << unknownMode << '\n';
    return count > 0 && (!expectUnknown || (unknown > 0 && unknownMode > 0)) &&
        genericInt > 0 && genericPayload > 0 && refFacts > 0 &&
        classRef > 0 && namespaceRef > 0 && classMut > 0 && namespaceMut > 0;
}

bool removeOneYieldSyntax(const ProjectOopHirModel& hir)
{
    for (const auto& function : hir.functions())
    {
        if (function.mangledName.find("Program__Gen__") == std::string::npos || !function.body) continue;
        std::vector<ProjectOopHirNode*> pending{function.body.get()};
        while (!pending.empty())
        {
            auto* node = pending.back();
            pending.pop_back();
            if (node->kind == ProjectOopHirNodeKind::AsyncYieldValue && !node->children.empty() &&
                node->children.front() && node->children.front()->kind != ProjectOopHirNodeKind::Conditional)
            {
                node->children.front()->syntax = nullptr;
                return true;
            }
            for (const auto& child : node->children) if (child) pending.push_back(child.get());
        }
    }
    return false;
}

bool removeOneYieldMode(const ProjectOopHirModel& hir)
{
    for (const auto& function : hir.functions())
    {
        if (!function.isAsyncPoll || function.mangledName.find("Wrapped") == std::string::npos) continue;
        const_cast<ProjectOopHirFunction&>(function).asyncYieldItemMode = ProjectOopYieldItemMode::Unknown;
        return true;
    }
    return false;
}

std::string factSnapshot(const ProjectOopMirModel& model)
{
    std::ostringstream out;
    for (const auto& function : model.functions())
        for (const auto& fact : function.asyncYieldFacts)
        {
            out << function.mangledName.size() << ':' << function.mangledName << ' ' << fact.yieldPreorder << ' '
                << fact.nodePreorder << ' ' << fact.parentFact.has_value() << ' '
                << fact.parentFact.value_or(0) << ' ' << fact.childIndex << ' ' << static_cast<int>(fact.nodeKind)
                << ' ' << static_cast<int>(fact.itemMode) << ' ' << fact.hasBinding << ' '
                << fact.typeKey.size() << ':' << fact.typeKey << ' '
                << static_cast<int>(fact.valueCategory) << ' ' << static_cast<int>(fact.ownership) << ' '
                << fact.ownershipTransfer << ' ' << fact.ownershipFreshOwner << ' '
                << static_cast<int>(fact.objectLifetimeOwnership) << ' '
                << static_cast<int>(fact.storageOwnership) << '\n';
        }
    return out.str();
}

bool writeFixture(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    std::ofstream source(directory / "Main.ns", std::ios::binary);
    source << R"NS(using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
class Payload { ~Payload() {} }
class Generator<T> { public static AsyncIterator<T, Exception> Gen(T value) {
 try { yield return value; } finally { yield return value; } } }
class Program { public static Payload Shared;
 public static owned Payload Make() { return new Payload(); }
 public static bool Choose(bool value) { return value; }
 public static int InspectString(borrowed string value) { return 0; }
 public static AsyncIterator<Payload, Exception> Gen(bool choose) {
  try { yield return Shared; } finally { yield return Choose(choose) ? Make() : Shared; }
 }
 public static AsyncIterator<ref int, Exception> GenRef() { int value = 5; yield return value; }
 public static AsyncIterator<ref mut int, Exception> GenRefMut() { int value = 5; yield return value; }
 public static int Main() { AsyncIterator<int, Exception> ints = Generator<int>.Gen(5);
  owned Payload owner = new Payload(); AsyncIterator<Payload, Exception> objects = Generator<Payload>.Gen(owner);
  return 0; }
}
namespace Helpers {
 public AsyncIterator<ref int, Exception> GenRef() { int value = 5; yield return value; }
 public AsyncIterator<ref mut int, Exception> GenRefMut() { int value = 5; yield return value; }
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
    if (argc != 4) { std::cerr << "usage: test <shared-directory> <cold|warm|unknown> <1|4>\n"; return 2; }
    const std::filesystem::path root = argv[1];
    const std::string phase = argv[2];
    const std::string workersText = argv[3];
    if (workersText != "1" && workersText != "4") return 2;
    const std::size_t workers = workersText == "1" ? 1U : 4U;
    if (phase != "cold" && phase != "warm" && phase != "unknown") return 2;
    if ((phase == "cold" || phase == "unknown") && !writeFixture(root)) return 2;
    ProjectCompilation compilation;
    compilation.setCompilationJobs(workers);
    std::string error;
    if (!compilation.compileProjectOopHir(root / "Probe.nsproj", error) ||
        !compilation.methodBodyModel() || !compilation.oopHirModel() || !compilation.nameLookup() ||
        !compilation.memberModel() || !compilation.objectLayoutModel() || !compilation.virtualDispatchModel() ||
        !compilation.interfaceModel() || !compilation.destructionModel() || !compilation.namespaceFunctionModel())
    { std::cerr << "HIR failed: " << error << '\n'; return 1; }
    if (phase == "unknown" && (!removeOneYieldSyntax(*compilation.oopHirModel()) ||
        !removeOneYieldMode(*compilation.oopHirModel())))
    { std::cerr << "could not create a synthetic missing-binding case\n"; return 1; }
    ProjectOopMirModel model;
    model.setMirCacheRoot(root / "cache");
    if (!model.build(*compilation.nameLookup(), *compilation.memberModel(), *compilation.objectLayoutModel(),
        *compilation.virtualDispatchModel(), *compilation.interfaceModel(), *compilation.destructionModel(),
        *compilation.oopHirModel(), *compilation.namespaceFunctionModel(), error, workers))
    { std::cerr << "MIR failed: " << error << '\n'; return 1; }
    const std::size_t restored = ProjectOopHirModelTestAccess::restoredPolls(model);
    std::cout << "restored-polls=" << restored << '\n';
    if (((phase == "cold" || phase == "unknown") && restored != 0) ||
        (phase == "warm" && restored == 0)) return 1;
    const bool observations = reportProducers(model, *compilation.methodBodyModel()) &&
        reportFacts(model, *compilation.oopHirModel(), *compilation.methodBodyModel(), phase == "unknown");
    const std::string before = factSnapshot(model);
    std::string secondError;
    const bool secondBuild = model.build(*compilation.nameLookup(), *compilation.memberModel(),
        *compilation.objectLayoutModel(), *compilation.virtualDispatchModel(), *compilation.interfaceModel(),
        *compilation.destructionModel(), *compilation.oopHirModel(), *compilation.namespaceFunctionModel(),
        secondError, workers);
    const std::string after = factSnapshot(model);
    const bool rejected = !secondBuild && secondError == "ProjectOopMirModel instances are single-use";
    std::cout << "single-use-rebuild-rejected=" << rejected << " facts-preserved=" << (before == after) << '\n';
    return observations && rejected && before == after ? 0 : 1;
}
