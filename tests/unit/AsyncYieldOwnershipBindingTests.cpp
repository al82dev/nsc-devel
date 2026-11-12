#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirNode.h"

#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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
    if (!compilation.compileProjectOopHir(directory / "Probe.nsproj", error) ||
        compilation.methodBodyModel() == nullptr || compilation.oopHirModel() == nullptr)
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
            if ((name == "reference" || name == "reference_class" || name == "scalar_copy") &&
                (node->children.size() != 1 || node->children.front() == nullptr ||
                 node->children.front()->kind != (name == "scalar_copy" ? ProjectOopHirNodeKind::LocalValue
                                                                         : ProjectOopHirNodeKind::LocalAddress)))
            {
                std::cerr << name << ": reference/value HIR shape changed\n";
                valid = false;
            }
        }
    }
    if (yields != 1) std::cerr << name << ": expected one async yield, found " << yields << '\n';
    return yields == 1 && valid;
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
    if (!compilation.compileProjectOopHir(directory / "Probe.nsproj", error) ||
        compilation.methodBodyModel() == nullptr || compilation.oopHirModel() == nullptr)
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
    return count == 4 && intCount == 2 && payloadCount == 2 && valid;
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
    ok &= inspectGeneric(root);
    return ok ? 0 : 1;
}
