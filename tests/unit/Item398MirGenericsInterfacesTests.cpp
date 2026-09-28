#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

std::optional<std::size_t> findType(
    const nativesharp::frontend::ProjectCompilation& compilation, std::string_view suffix)
{
    for (std::size_t ordinal = 0; ordinal < compilation.nameLookup()->namedTypes().size(); ++ordinal)
    {
        if (compilation.nameLookup()->namedTypes()[ordinal].identity.fullyQualifiedName().ends_with(suffix))
            return ordinal;
    }
    return std::nullopt;
}

const nativesharp::frontend::ProjectOopMirNode* findKind(
    const nativesharp::frontend::ProjectOopMirNode* node,
    nativesharp::frontend::ProjectOopMirNodeKind kind)
{
    if (node == nullptr || node->kind == kind) return node;
    for (const auto& child : node->children)
        if (const auto* found = findKind(child.get(), kind); found != nullptr) return found;
    return nullptr;
}
}

int main()
{
    using namespace nativesharp::frontend;
    nativesharp::tests::TestContext tests;
    const auto root = nativesharp::tests::evidenceRoot(
        "nativesharp_item398_mir_generics_interfaces_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Item398.nsproj",
        "<Project><PropertyGroup><Name>Item398</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
interface IValue { int Get(); }
class Device : IValue { public int Get() { return 7; } }
class Box<T>
{
    public T Value;
    public Box(T value) { Value = value; }
    public T Get() { return Value; }
}
class Probe
{
    public static int Run()
    {
        Box<int> box = new Box<int>(35);
        IValue value = new Device();
        int result = box.Get() + value.Get();
        delete box;
        delete value;
        return result;
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(root / "Item398.nsproj", error);
    tests.expect(built && compilation.oopMirModel() != nullptr,
        "generic/interface fixture reaches project MIR: " + error);
    if (!built || compilation.oopMirModel() == nullptr) return tests.resultKeeping(root);
    const ProjectOopMirModel& model = *compilation.oopMirModel();

    const auto specialization = std::ranges::find_if(model.genericSpecializations(),
        [](const ProjectOopMirGenericSpecialization& value) {
            return value.key.find("Box`1<int>") != std::string::npos;
        });
    tests.expect(specialization != model.genericSpecializations().end() &&
            specialization->functionIndices.size() >= 2 &&
            specialization->dispatchTableIndices.size() == 1,
        "MIR consolidates a closed generic type's functions and runtime table under one key");

    const auto device = findType(compilation, "Device");
    const auto interfaceType = findType(compilation, "IValue");
    const ProjectOopMirInterfaceBinding* binding = device && interfaceType
        ? model.interfaceBindingForTypes(*device, *interfaceType) : nullptr;
    tests.expect(binding != nullptr && binding->stableInterfaceTypeId != 0 &&
            binding->interfaceMemberOrdinals.size() == 1 &&
            binding->implementationMemberOrdinals.size() == 1 &&
            binding->calleeNames.size() == 1,
        "MIR consolidates stable interface identity, slot mapping, and concrete callee");
    std::uint64_t interfaceId = 0;
    std::size_t slot = 99;
    tests.expect(binding != nullptr && model.interfaceSlotForMember(
            binding->interfaceMemberOrdinals[0], interfaceId, slot) &&
            interfaceId == binding->stableInterfaceTypeId && slot == 0,
        "MIR resolves interface cleanup dispatch without consulting semantic models");

    const auto run = std::ranges::find_if(model.functions(),
        [](const ProjectOopMirFunction& function) {
            return function.mangledName.find("Probe__Run") != std::string::npos;
        });
    const ProjectOopMirNode* call = run == model.functions().end() ? nullptr
        : findKind(run->body.get(), ProjectOopMirNodeKind::InterfaceInstanceCall);
    tests.expect(call != nullptr && call->interfaceSlot == 0 &&
            interfaceType.has_value() && call->targetTypeOrdinal == *interfaceType &&
            binding != nullptr && call->immediate == binding->stableInterfaceTypeId,
        "interface call sites consume the same consolidated MIR identity and slot");

    const std::string dump = ProjectOopMirModelDumper::dump(model);
    tests.expect(dump.find("schema=1.14") != std::string::npos &&
            dump.find("generic-specializations=") != std::string::npos &&
            dump.find("interface-bindings=") != std::string::npos,
        "versioned MIR dump publishes generic and interface catalogues");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), model);
    tests.expect(module != nullptr,
        "LLVM consumes consolidated generic/interface MIR: " + lowerer.error());
    return tests.resultKeeping(root);
}
