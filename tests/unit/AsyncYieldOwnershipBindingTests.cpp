#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirNode.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirNode.h"

#include <algorithm>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess final
{
    static std::size_t restoredPolls(const ProjectOopMirModel& model)
    {
        std::size_t result = 0;
        const auto count = std::min(model.functions_.size(), model.mirCachePayloads_.size());
        for (std::size_t index = 0; index < count; ++index)
            if (model.functions_[index].mangledName.find("$moveNext$poll") != std::string::npos &&
                model.mirCachePayloads_[index].has_value()) ++result;
        return result;
    }
};
}

namespace
{
using namespace nativesharp::frontend;

bool inspect(const std::filesystem::path& root, std::string_view name, std::string_view body)
{
    const auto directory = root / name;
    std::filesystem::create_directories(directory);
    const std::string source = std::string(R"NS(using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
class Payload { ~Payload() {} }
struct Box { public owned Payload Value; }
class Program { public static Payload Shared;
 public static owned Payload Make() { return new Payload(); }
)NS") + std::string(body) + "\n public static int Main() { return 0; } }\n";
    std::ofstream(directory / "Main.ns", std::ios::binary) << source;
    std::ofstream(directory / "Probe.nsproj", std::ios::binary)
        << "<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>"
           "<ItemGroup><Compile Include=\"Main.ns\" /></ItemGroup></Project>";
    ProjectCompilation compilation;
    std::string error;
    if (!compilation.compileProjectOopMir(directory / "Probe.nsproj", error) ||
        compilation.methodBodyModel() == nullptr || compilation.oopHirModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
    {
        std::cerr << name << ": HIR compilation failed: " << error << '\n';
        return false;
    }
    const auto& bodyModel = *compilation.methodBodyModel();
    std::size_t yields = 0;
    bool valid = true;
    for (const auto& function : compilation.oopHirModel()->functions())
    {
        std::vector<const ProjectOopHirNode*> pending{function.body.get()};
        while (!pending.empty())
        {
            const auto* node = pending.back();
            pending.pop_back();
            if (node == nullptr) continue;
            for (const auto& child : node->children) pending.push_back(child.get());
            if (node->kind != ProjectOopHirNodeKind::AsyncYieldValue) continue;
            ++yields;
            if (node->syntax == nullptr || node->syntax->first() == nullptr)
            {
                std::cerr << name << ": async yield lacks its operand syntax\n";
                valid = false;
                continue;
            }
            const auto* operand = node->syntax->first();
            std::size_t matches = 0;
            for (const auto& binding : bodyModel.expressionTypes())
            {
                if (binding.syntax != operand || binding.methodMemberOrdinal != function.memberOrdinal) continue;
                ++matches;
                std::cout << name << " syntax=" << static_cast<int>(operand->kind())
                    << " type=" << binding.typeKey
                    << " category=" << static_cast<int>(binding.valueCategory)
                    << " ownership=" << static_cast<int>(binding.ownership)
                    << " transfer=" << binding.ownershipTransfer
                    << " fresh=" << binding.ownershipFreshOwner
                    << " object=" << static_cast<int>(binding.objectLifetimeOwnership)
                    << " storage=" << static_cast<int>(binding.storageOwnership)
                    << " local=" << binding.ownershipSourceLocalOrdinal.has_value()
                    << " parameter=" << binding.ownershipSourceParameterOrdinal.has_value()
                    << '\n';
                using nativesharp::sema::OwnershipKind;
                using nativesharp::sema::ValueCategory;
                bool expected = true;
                if (name == "class_move" || name == "value_move")
                    expected = binding.ownership == OwnershipKind::Owned && binding.ownershipTransfer &&
                        !binding.ownershipFreshOwner && binding.ownershipSourceLocalOrdinal.has_value();
                else if (name == "class_factory")
                    expected = binding.ownership == OwnershipKind::Owned && binding.ownershipTransfer &&
                        binding.ownershipFreshOwner;
                else if (name == "class_fresh")
                    expected = binding.ownership == OwnershipKind::None && !binding.ownershipTransfer &&
                        binding.ownershipFreshOwner && binding.objectLifetimeOwnership == OwnershipKind::Owned &&
                        binding.storageOwnership == OwnershipKind::Owned;
                else if (name == "value_fresh")
                    expected = binding.ownership == OwnershipKind::None && !binding.ownershipTransfer &&
                        !binding.ownershipFreshOwner && binding.valueCategory == ValueCategory::Temporary;
                else if (name == "class_owner_alias")
                    expected = binding.ownership == OwnershipKind::Owned && !binding.ownershipTransfer &&
                        !binding.ownershipFreshOwner && binding.valueCategory == ValueCategory::Place &&
                        binding.ownershipSourceLocalOrdinal.has_value();
                else if (name == "class_local_alias")
                    expected = binding.ownership == OwnershipKind::None && !binding.ownershipTransfer &&
                        !binding.ownershipFreshOwner && binding.valueCategory == ValueCategory::Place &&
                        binding.objectLifetimeOwnership == OwnershipKind::None &&
                        binding.storageOwnership == OwnershipKind::None;
                else
                    expected = binding.ownership == OwnershipKind::None && !binding.ownershipTransfer &&
                        !binding.ownershipFreshOwner && binding.valueCategory == ValueCategory::Place;
                if (!expected)
                {
                    std::cerr << name << ": unexpected ownership binding\n";
                    valid = false;
                }
            }
            if (matches != 1)
            {
                std::cerr << name << ": yield has " << matches << " callable-specific bindings\n";
                valid = false;
            }
            if ((name == "reference" || name == "reference_class" || name == "scalar_copy" ||
                 name == "class_local_alias") &&
                (node->children.size() != 1 || node->children.front() == nullptr ||
                 node->children.front()->kind != (name == "scalar_copy" || name == "class_local_alias"
                                                                         ? ProjectOopHirNodeKind::LocalValue
                                                                         : ProjectOopHirNodeKind::LocalAddress)))
            {
                std::cerr << name << ": reference/value HIR shape changed\n";
                valid = false;
            }
        }
    }
    if (yields != 1) std::cerr << name << ": expected one async yield, found " << yields << '\n';
    std::size_t mirYields = 0;
    for (const auto& function : compilation.oopMirModel()->functions())
    {
        std::vector<const ProjectOopMirNode*> pending{function.body.get()};
        while (!pending.empty())
        {
            const auto* node = pending.back(); pending.pop_back();
            if (node == nullptr) continue;
            for (const auto& child : node->children) pending.push_back(child.get());
            if (node->kind != ProjectOopMirNodeKind::AsyncYieldValue) continue;
            ++mirYields;
            if (node->children.size() != 1 || node->children.front() == nullptr ||
                node->children.front()->syntax == nullptr)
            {
                std::cerr << name << ": MIR producer UNKNOWN: missing syntax\n";
                valid = false; continue;
            }
            const auto* producer = node->children.front().get();
            const auto* binding = bodyModel.expressionTypeBinding(producer->syntax, function.memberOrdinal);
            if (binding == nullptr || binding->typeKey != producer->typeKey)
            {
                std::cerr << name << ": MIR producer UNKNOWN: missing or mismatched binding\n";
                valid = false; continue;
            }
            std::cout << name << " MIR producer=" << producer->typeKey
                << " binding=" << binding->typeKey << '\n';
        }
    }
    if (mirYields != 1) std::cerr << name << ": expected one MIR yield, found " << mirYields << '\n';
    return yields == 1 && mirYields == 1 && valid;
}

bool inspectGeneric(const std::filesystem::path& root)
{
    const auto directory = root / "generic";
    std::filesystem::create_directories(directory);
    std::ofstream(directory / "Main.ns", std::ios::binary) << R"NS(using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
class Payload { public int Id; public Payload() { Id = 37; } }
class Generator<T> { public static AsyncIterator<T, Exception> Gen(T value) {
 try { yield return value; } finally { yield return value; } } }
class Program { public static async int Main() { try {
 AsyncIterator<int, Exception> ints = Generator<int>.Gen(5);
 owned Payload owner = new Payload();
 AsyncIterator<Payload, Exception> objs = Generator<Payload>.Gen(owner);
 bool first = await ints.MoveNextAsync(); bool second = await objs.MoveNextAsync();
 return first && second ? 42 : 20; } catch (Exception error) { return 29; } } }
)NS";
    std::ofstream(directory / "Probe.nsproj", std::ios::binary)
        << "<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>"
           "<ItemGroup><Compile Include=\"Main.ns\" /></ItemGroup></Project>";
    ProjectCompilation compilation;
    std::string error;
    if (!compilation.compileProjectOopMir(directory / "Probe.nsproj", error) ||
        compilation.methodBodyModel() == nullptr || compilation.oopHirModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
    {
        std::cerr << "generic HIR compilation failed: " << error << '\n';
        return false;
    }
    const auto& body = *compilation.methodBodyModel();
    std::size_t count = 0;
    std::size_t intCount = 0;
    std::size_t payloadCount = 0;
    bool valid = true;
    for (const auto& function : compilation.oopHirModel()->functions())
    {
        if (function.mangledName.find("$moveNext$poll") == std::string::npos) continue;
        std::vector<const ProjectOopHirNode*> pending{function.body.get()};
        while (!pending.empty())
        {
            const auto* node = pending.back();
            pending.pop_back();
            if (node == nullptr) continue;
            for (const auto& child : node->children) pending.push_back(child.get());
            if (node->kind != ProjectOopHirNodeKind::AsyncYieldValue) continue;
            ++count;
            if (node->syntax == nullptr || node->syntax->first() == nullptr)
            {
                std::cerr << "generic async yield lacks its operand syntax\n";
                valid = false;
                continue;
            }
            const auto* binding = body.expressionTypeBinding(node->syntax->first(), function.memberOrdinal);
            const std::string operandType = node->children.empty() ? "<none>" : node->children.front()->typeKey;
            if (binding == nullptr || binding->typeKey != operandType) valid = false;
            if (operandType == "builtin:int") ++intCount;
            if (operandType == "type:Probe::Payload") ++payloadCount;
            std::size_t matches = 0;
            for (const auto& candidate : body.expressionTypes())
                if (candidate.syntax == node->syntax->first() && candidate.methodMemberOrdinal == function.memberOrdinal)
                    ++matches;
            if (matches != 1) valid = false;
            std::cout << "generic function=" << function.mangledName << " ordinal=" << function.memberOrdinal
                << " generic=" << function.genericSpecializationKey
                << " operand=" << operandType
                << " binding=" << (binding == nullptr ? "<none>" : binding->typeKey) << '\n';
        }
    }
    std::size_t mirCount = 0;
    for (const auto& function : compilation.oopMirModel()->functions())
    {
        if (function.mangledName.find("$moveNext$poll") == std::string::npos) continue;
        std::vector<const ProjectOopMirNode*> pending{function.body.get()};
        while (!pending.empty())
        {
            const auto* node = pending.back(); pending.pop_back();
            if (node == nullptr) continue;
            for (const auto& child : node->children) pending.push_back(child.get());
            if (node->kind != ProjectOopMirNodeKind::AsyncYieldValue) continue;
            ++mirCount;
            if (node->children.size() != 1 || node->children.front() == nullptr ||
                node->children.front()->syntax == nullptr)
            {
                std::cerr << "generic MIR producer UNKNOWN: missing syntax\n";
                valid = false; continue;
            }
            const auto* producer = node->children.front().get();
            const auto* binding = body.expressionTypeBinding(producer->syntax, function.memberOrdinal);
            if (binding == nullptr || binding->typeKey != producer->typeKey)
            {
                std::cerr << "generic MIR producer UNKNOWN: missing or mismatched binding\n";
                valid = false;
            }
        }
    }
    return count == 4 && intCount == 2 && payloadCount == 2 && mirCount == 4 && valid;
}

bool inspectMixedAndCache(const std::filesystem::path& root)
{
    const auto directory = root / "mixed";
    std::filesystem::create_directories(directory);
    std::ofstream(directory / "Main.ns", std::ios::binary) << R"NS(using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
class Payload { ~Payload() {} }
class Program { public static Payload Shared;
 public static owned Payload Make() { return new Payload(); }
 public static bool Choose(bool value) { return value; }
 public static AsyncIterator<Payload, Exception> Gen(bool choose) {
  try { yield return Shared; } finally { yield return Choose(choose) ? Make() : Shared; }
 }
 public static int Main() { return 0; }
}
)NS";
    std::ofstream(directory / "Probe.nsproj", std::ios::binary)
        << "<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>"
           "<ItemGroup><Compile Include=\"Main.ns\" /></ItemGroup></Project>";
    ProjectCompilation compilation;
    std::string error;
    if (!compilation.compileProjectOopHir(directory / "Probe.nsproj", error) ||
        !compilation.methodBodyModel() || !compilation.oopHirModel() || !compilation.nameLookup() ||
        !compilation.memberModel() || !compilation.objectLayoutModel() || !compilation.virtualDispatchModel() ||
        !compilation.interfaceModel() || !compilation.destructionModel() || !compilation.namespaceFunctionModel())
    {
        std::cerr << "mixed HIR compilation failed: " << error << '\n';
        return false;
    }
    const auto& body = *compilation.methodBodyModel();
    const auto lookup = [&](const ProjectOopMirNode* node, std::size_t ordinal) {
        return node == nullptr || node->syntax == nullptr ? nullptr : body.expressionTypeBinding(node->syntax, ordinal);
    };
    ProjectOopMirNode synthetic;
    if (lookup(&synthetic, 0) != nullptr)
    {
        std::cerr << "synthetic node incorrectly received a binding\n";
        return false;
    }
    std::cout << "synthetic MIR producer UNKNOWN: missing syntax\n";
    const auto inspectModel = [&](const ProjectOopMirModel& model, std::string_view label) {
        std::size_t matches = 0;
        bool valid = true;
        for (const auto& function : model.functions())
        {
            if (function.mangledName.find("$moveNext$poll") == std::string::npos) continue;
            std::vector<const ProjectOopMirNode*> pending{function.body.get()};
            while (!pending.empty())
            {
                const auto* node = pending.back(); pending.pop_back();
                if (!node) continue;
                for (const auto& child : node->children) pending.push_back(child.get());
                if (node->kind != ProjectOopMirNodeKind::AsyncYieldValue || node->children.size() != 1 ||
                    !node->children.front() || node->children.front()->kind != ProjectOopMirNodeKind::Conditional) continue;
                ++matches;
                const auto* conditional = node->children.front().get();
                if (conditional->children.size() != 3 || !conditional->children[1] || !conditional->children[2])
                { valid = false; continue; }
                const auto* owned = lookup(conditional->children[1].get(), function.memberOrdinal);
                const auto* borrowed = lookup(conditional->children[2].get(), function.memberOrdinal);
                if (!owned || !borrowed)
                {
                    std::cerr << label << " conditional branch UNKNOWN: missing syntax or binding\n";
                    valid = false; continue;
                }
                if (owned->ownership != nativesharp::sema::OwnershipKind::Owned || !owned->ownershipTransfer ||
                    borrowed->ownership != nativesharp::sema::OwnershipKind::None || borrowed->ownershipTransfer)
                    valid = false;
            }
        }
        std::cout << label << " conditional-yields=" << matches << '\n';
        return matches == 1 && valid;
    };
    auto build = [&](ProjectOopMirModel& model) {
        model.setMirCacheRoot(directory / "cache");
        return model.build(*compilation.nameLookup(), *compilation.memberModel(), *compilation.objectLayoutModel(),
            *compilation.virtualDispatchModel(), *compilation.interfaceModel(), *compilation.destructionModel(),
            *compilation.oopHirModel(), *compilation.namespaceFunctionModel(), error, 1);
    };
    ProjectOopMirModel cold;
    if (!build(cold)) { std::cerr << "cold MIR failed: " << error << '\n'; return false; }
    const bool coldOk = inspectModel(cold, "cold");
    ProjectOopMirModel warm;
    if (!build(warm)) { std::cerr << "warm MIR failed: " << error << '\n'; return false; }
    const std::size_t restored = ProjectOopHirModelTestAccess::restoredPolls(warm);
    std::cout << "warm restored-polls=" << restored << '\n';
    return coldOk && restored > 0 && inspectModel(warm, "warm");
}

bool inspectFullExpressionWrapper(const std::filesystem::path& root)
{
    const auto directory = root / "full-expression-wrapper";
    std::filesystem::create_directories(directory);
    std::ofstream(directory / "Main.ns", std::ios::binary) << R"NS(using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
class Program {
 public static int InspectString(borrowed string value) { return 0; }
 public static AsyncIterator<int, Exception> Gen() {
  try { yield return 5; }
  finally { yield return InspectString("a" + "b"); }
 }
 public static int Main() { return 0; }
}
)NS";
    std::ofstream(directory / "Probe.nsproj", std::ios::binary)
        << "<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>"
           "<ItemGroup><Compile Include=\"Main.ns\" /></ItemGroup></Project>";
    ProjectCompilation compilation;
    std::string error;
    if (!compilation.compileProjectOopMir(directory / "Probe.nsproj", error) ||
        !compilation.methodBodyModel() || !compilation.oopMirModel())
    {
        std::cerr << "full-expression wrapper compilation failed: " << error << '\n';
        return false;
    }
    std::size_t wrappers = 0;
    std::size_t temporaries = 0;
    std::size_t unknown = 0;
    const auto& body = *compilation.methodBodyModel();
    for (const auto& function : compilation.oopMirModel()->functions())
    {
        if (function.mangledName.find("$moveNext$poll") == std::string::npos) continue;
        std::vector<const ProjectOopMirNode*> pending{function.body.get()};
        while (!pending.empty())
        {
            const auto* node = pending.back(); pending.pop_back();
            if (!node) continue;
            for (const auto& child : node->children) pending.push_back(child.get());
            if (node->kind != ProjectOopMirNodeKind::AsyncYieldValue || node->children.size() != 1 ||
                !node->children.front()) continue;
            std::vector<const ProjectOopMirNode*> operands{node->children.front().get()};
            while (!operands.empty())
            {
                const auto* operand = operands.back(); operands.pop_back();
                if (!operand) continue;
                for (const auto& child : operand->children) operands.push_back(child.get());
                if (operand->kind != ProjectOopMirNodeKind::FullExpressionCleanup &&
                    operand->kind != ProjectOopMirNodeKind::OwnedTemporaryBorrow) continue;
                if (operand->kind == ProjectOopMirNodeKind::FullExpressionCleanup) ++wrappers;
                else ++temporaries;
                const auto* binding = operand->syntax == nullptr ? nullptr :
                    body.expressionTypeBinding(operand->syntax, function.memberOrdinal);
                if (binding == nullptr) ++unknown;
                std::cout << "wrapper kind=" << static_cast<int>(operand->kind)
                    << " binding=" << (binding == nullptr ? "UNKNOWN" : binding->typeKey) << '\n';
            }
        }
    }
    std::cout << "full-expression wrappers=" << wrappers << " temporaries=" << temporaries
        << " unknown=" << unknown << '\n';
    if (wrappers == 0 || temporaries == 0)
        std::cerr << "expected wrapper and owned temporary in async-yield operand\n";
    return wrappers > 0 && temporaries > 0;
}
}

int main()
{
    const auto base = std::filesystem::temp_directory_path();
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path root;
    for (unsigned index = 0; index < 1000; ++index)
    {
        auto candidate = base / ("nsc-async-yield-binding-" + std::to_string(nonce) + "-" + std::to_string(index));
        std::error_code error;
        if (std::filesystem::create_directory(candidate, error)) { root = candidate; break; }
        if (error) { std::cerr << "cannot create test directory: " << error.message() << '\n'; return 1; }
    }
    if (root.empty()) { std::cerr << "no unique test directory available\n"; return 1; }
    bool ok = true;
    ok &= inspect(root, "class_move", R"NS(public static AsyncIterator<Payload, Exception> Gen() {
 owned Payload value = new Payload(); yield return move(value); })NS");
    ok &= inspect(root, "class_fresh", R"NS(public static AsyncIterator<Payload, Exception> Gen() {
 yield return new Payload(); })NS");
    ok &= inspect(root, "class_factory", R"NS(public static AsyncIterator<Payload, Exception> Gen() {
 yield return Make(); })NS");
    ok &= inspect(root, "class_copy", R"NS(public static AsyncIterator<Payload, Exception> Gen() {
 yield return Shared; })NS");
    ok &= inspect(root, "reference", R"NS(public static AsyncIterator<ref int, Exception> Gen() {
 int value = 5; yield return value; })NS");
    ok &= inspect(root, "scalar_copy", R"NS(public static AsyncIterator<int, Exception> Gen() {
 int value = 5; yield return value; })NS");
    ok &= inspect(root, "value_move", R"NS(public static AsyncIterator<Box, Exception> Gen() {
 Box value = new Box(); value.Value = new Payload(); yield return move(value); })NS");
    ok &= inspect(root, "value_fresh", R"NS(public static AsyncIterator<Box, Exception> Gen() {
 yield return new Box(); })NS");
    ok &= inspect(root, "reference_class", R"NS(public static AsyncIterator<ref Payload, Exception> Gen() {
 Payload value = Shared; yield return value; })NS");
    ok &= inspect(root, "class_owner_alias", R"NS(public static AsyncIterator<Payload, Exception> Gen() {
 owned Payload value = new Payload(); yield return value; })NS");
    ok &= inspect(root, "class_local_alias", R"NS(public static AsyncIterator<Payload, Exception> Gen() {
 Payload value = Shared; yield return value; })NS");
    ok &= inspectGeneric(root);
    ok &= inspectMixedAndCache(root);
    ok &= inspectFullExpressionWrapper(root);
    return ok ? 0 : 1;
}
