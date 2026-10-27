#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace
{
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::frontend::ClassObjectLayout;
using nativesharp::frontend::ClassVirtualTable;
using nativesharp::frontend::MethodNameBindingKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectConversionKind;
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
    std::string_view references = {})
{
    std::string project = "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />";
    project += references;
    project += "</ItemGroup></Project>";
    writeFile(directory / (std::string(name) + ".nsproj"), project);
}

std::size_t diagnosticCount(const ProjectCompilation& compilation, DiagnosticId id)
{
    std::size_t count = 0;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        count += diagnostic.id() == id ? 1u : 0u;
    }
    return count;
}

std::optional<std::size_t> findTypeOrdinal(const ProjectCompilation& compilation, std::string_view suffix)
{
    if (compilation.nameLookup() == nullptr)
    {
        return std::nullopt;
    }
    for (std::size_t ordinal = 0; ordinal < compilation.nameLookup()->namedTypes().size(); ++ordinal)
    {
        if (compilation.nameLookup()->namedTypes()[ordinal].identity.fullyQualifiedName().ends_with(suffix))
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> findMemberOrdinal(
    const ProjectCompilation& compilation,
    std::size_t typeOrdinal,
    std::string_view name)
{
    if (compilation.memberModel() == nullptr)
    {
        return std::nullopt;
    }
    for (const std::size_t ordinal : compilation.memberModel()->membersOfType(typeOrdinal))
    {
        if (ordinal < compilation.memberModel()->members().size() &&
            compilation.memberModel()->members()[ordinal].name == name)
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

const ClassObjectLayout* layoutFor(const ProjectCompilation& compilation, std::size_t typeOrdinal)
{
    return compilation.objectLayoutModel() != nullptr
        ? compilation.objectLayoutModel()->layoutForType(typeOrdinal)
        : nullptr;
}

std::size_t countMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr)
    {
        return 0;
    }
    std::size_t count = node->kind == kind ? 1u : 0u;
    for (const auto& child : node->children)
    {
        count += countMirKind(child.get(), kind);
    }
    return count;
}

const ProjectOopMirNode* findMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    if (node->kind == kind)
    {
        return node;
    }
    for (const auto& child : node->children)
    {
        if (const ProjectOopMirNode* result = findMirKind(child.get(), kind); result != nullptr)
        {
            return result;
        }
    }
    return nullptr;
}

std::size_t countHirKind(const ProjectOopHirNode* node, ProjectOopHirNodeKind kind)
{
    if (node == nullptr)
    {
        return 0;
    }
    std::size_t count = node->kind == kind ? 1u : 0u;
    for (const auto& child : node->children)
    {
        count += countHirKind(child.get(), kind);
    }
    return count;
}

const ProjectOopHirNode* findHirKind(const ProjectOopHirNode* node, ProjectOopHirNodeKind kind)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    if (node->kind == kind)
    {
        return node;
    }
    for (const auto& child : node->children)
    {
        if (const ProjectOopHirNode* result = findHirKind(child.get(), kind); result != nullptr)
        {
            return result;
        }
    }
    return nullptr;
}

void testGeneralizedReceivers(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Receivers";
    writeProject(directory, "MacroAReceivers");
    writeFile(directory / "Main.ns",
        "public class Counter { public int Value; public Counter(int value) { Value = value; } "
        "public int Add(int delta) { Value = Value + delta; return Value; } } "
        "public class Harness { "
        "public owned Counter Make(int value) { return new Counter(value); } "
        "public int Read(Counter counter) { return counter.Value; } "
        "public int Write(Counter counter) { counter.Value = 11; return counter.Value; } "
        "public int Call(Counter counter) { return counter.Add(2); } "
        "public int Local() { Counter counter = new Counter(3); counter.Value = 9; return counter.Add(4); } "
        "public int Temporary() { raii Counter counter = new Counter(5); return counter.Add(6); } "
        "public int Returned() { return Make(7).Add(8); } "
        "public int WriteReturned() { Make(9).Value = 12; return 0; } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "MacroAReceivers.nsproj", error);
    tests.expect(ok && error.empty(),
        "Stage 31 generalized receivers compile through OOP MIR for parameters, locals, temporaries and returned receivers");
    tests.expect(compilation.methodBodyModel() != nullptr && compilation.oopHirModel() != nullptr &&
            compilation.oopMirModel() != nullptr,
        "Stage 31 retains method-body receiver bindings plus explicit HIR/MIR receivers");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.oopHirModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
    {
        return;
    }

    bool sawObjectField = false;
    bool sawObjectMethod = false;
    for (const auto& binding : compilation.methodBodyModel()->nameBindings())
    {
        sawObjectField = sawObjectField || binding.kind == MethodNameBindingKind::ExplicitObjectField;
        sawObjectMethod = sawObjectMethod || binding.kind == MethodNameBindingKind::ExplicitObjectMethodGroup;
    }
    tests.expect(sawObjectField && sawObjectMethod,
        "semantic binding distinguishes external object fields and method groups from implicit/explicit this");

    std::size_t fieldLoads = 0;
    std::size_t directCalls = 0;
    bool allFieldsHaveReceivers = true;
    bool allCallsHaveReceivers = true;
    for (const auto& function : compilation.oopMirModel()->functions())
    {
        fieldLoads += countMirKind(function.body.get(), ProjectOopMirNodeKind::FieldLoad);
        directCalls += countMirKind(function.body.get(), ProjectOopMirNodeKind::DirectInstanceCall);
        const ProjectOopMirNode* field = findMirKind(function.body.get(), ProjectOopMirNodeKind::FieldLoad);
        if (field != nullptr)
        {
            allFieldsHaveReceivers = allFieldsHaveReceivers && field->children.size() == 1;
        }
        const ProjectOopMirNode* call = findMirKind(function.body.get(), ProjectOopMirNodeKind::DirectInstanceCall);
        if (call != nullptr)
        {
            allCallsHaveReceivers = allCallsHaveReceivers && !call->children.empty();
        }
    }
    tests.expect(fieldLoads > 0 && directCalls >= 5 && allFieldsHaveReceivers && allCallsHaveReceivers,
        "MIR carries an explicit receiver operand for every generalized field/call operation");

    std::size_t fieldAddresses = 0;
    bool writeReturnedEvaluatesReceiverOnce = false;
    for (const auto& function : compilation.oopHirModel()->functions())
    {
        fieldAddresses += countHirKind(function.body.get(), ProjectOopHirNodeKind::FieldAddress);
        if (function.memberOrdinal < compilation.memberModel()->members().size() &&
            compilation.memberModel()->members()[function.memberOrdinal].name == "WriteReturned")
        {
            const ProjectOopHirNode* fieldAddress = findHirKind(
                function.body.get(), ProjectOopHirNodeKind::FieldAddress);
            writeReturnedEvaluatesReceiverOnce = fieldAddress != nullptr &&
                fieldAddress->children.size() == 1 &&
                countHirKind(fieldAddress, ProjectOopHirNodeKind::InstanceCall) == 1;
        }
    }
    tests.expect(fieldAddresses >= 3,
        "generalized object receivers support field writes through parameters, locals and call-result receivers");
    tests.expect(writeReturnedEvaluatesReceiverOnce,
        "a call-result receiver used as a field lvalue is represented exactly once in HIR and is not duplicated");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "generalized receivers lower to LLVM without falling back to implicit %this");
    if (module != nullptr)
    {
        std::string verifyError;
        tests.expect(module->verify(verifyError) && verifyError.empty(),
            "LLVM verifier accepts generalized receiver control flow");
        const std::string llvm = module->text();
        tests.expect(llvm.find("receiver.nonnull") != std::string::npos &&
                llvm.find("receiver.null") != std::string::npos &&
                llvm.find("i32 7") != std::string::npos,
            "every dereferenced object receiver uses the deterministic NullReference trap reason 7 contract");
    }
}

void testInheritanceLayoutAndConstruction(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Inheritance";
    writeProject(directory, "MacroAInheritance");
    writeFile(directory / "Main.ns",
        "public class Animal { protected int Age; public Animal(int age) { Age = age; } "
        "public int GetAge() { return Age; } } "
        "public class Dog : Animal { private int Weight; public Dog(int age, int weight) : base(age) { Weight = weight; } "
        "public Animal AsAnimal() { return this; } public int Sum() { return Age + Weight; } "
        "public int BaseAge() { return base.GetAge(); } } "
        "public class Kennel { public int Use(Dog dog) { Animal animal = dog; return animal.GetAge(); } "
        "public int Construct() { Dog dog = new Dog(5, 20); return dog.Sum(); } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "MacroAInheritance.nsproj", error);
    tests.expect(ok && error.empty(),
        "Stages 32-33 compile inheritance, protected access, upcasts, base calls and constructor chaining through MIR");
    if (!ok || compilation.inheritanceModel() == nullptr || compilation.objectLayoutModel() == nullptr ||
        compilation.oopHirModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const auto animal = findTypeOrdinal(compilation, "Animal");
    const auto dog = findTypeOrdinal(compilation, "Dog");
    tests.expect(animal.has_value() && dog.has_value() &&
            compilation.inheritanceModel()->baseTypeOrdinal(*dog) == animal &&
            compilation.inheritanceModel()->isDerivedFrom(*dog, *animal) &&
            compilation.inheritanceModel()->inheritanceDistance(*dog, *animal) == std::optional<std::size_t>(1),
        "inheritance graph records Dog -> Animal and a one-edge derived-to-base relationship");
    if (!animal.has_value() || !dog.has_value())
    {
        return;
    }

    const ClassObjectLayout* animalLayout = layoutFor(compilation, *animal);
    const ClassObjectLayout* dogLayout = layoutFor(compilation, *dog);
    tests.expect(animalLayout != nullptr && animalLayout->headerSize == 8 && animalLayout->size == 16 &&
            animalLayout->fields.size() == 1 && animalLayout->fields[0].offset == 8,
        "root Animal owns one descriptor header and places Age at byte 8");
    tests.expect(dogLayout != nullptr && dogLayout->headerSize == 8 && dogLayout->baseTypeOrdinal == animal &&
            dogLayout->baseSubobjectSize == 16 && dogLayout->size == 24 &&
            dogLayout->fields.size() == 1 && dogLayout->fields[0].offset == 16,
        "Dog embeds the complete Animal subobject at offset zero, reuses its header, then places Weight at byte 16");

    bool sawUpcast = false;
    bool sawConstructorInitializerCall = false;
    for (const auto& function : compilation.oopHirModel()->functions())
    {
        const ProjectOopHirNode* conversion = findHirKind(function.body.get(), ProjectOopHirNodeKind::Conversion);
        if (conversion != nullptr && conversion->conversion.has_value() &&
            conversion->conversion->kind == ProjectConversionKind::ReferenceUpcast)
        {
            sawUpcast = true;
        }
        sawConstructorInitializerCall = sawConstructorInitializerCall ||
            findHirKind(function.body.get(), ProjectOopHirNodeKind::ConstructorInitializerCall) != nullptr;
    }
    tests.expect(sawUpcast, "HIR records Derived -> Base as an explicit ReferenceUpcast semantic conversion");
    tests.expect(sawConstructorInitializerCall, "derived constructor HIR begins with an explicit base-constructor call");

    bool sawDirectBaseCall = false;
    for (const auto& function : compilation.oopMirModel()->functions())
    {
        const ProjectOopMirNode* call = findMirKind(function.body.get(), ProjectOopMirNodeKind::DirectInstanceCall);
        if (call != nullptr && call->baseReceiver)
        {
            sawDirectBaseCall = true;
        }
    }
    tests.expect(sawDirectBaseCall, "base.Method() remains a statically bound direct instance call in MIR");
}

void testVirtualDispatch(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Virtual";
    writeProject(directory, "MacroAVirtual");
    writeFile(directory / "Main.ns",
        "public class Animal { protected int Age; public Animal(int age) { Age = age; } "
        "public virtual int Speak() { return Age; } public int NonVirtual() { return 1; } } "
        "public class Dog : Animal { private int Weight; public Dog(int age, int weight) : base(age) { Weight = weight; } "
        "public override int Speak() { return Age + Weight; } public int CallBase() { return base.Speak(); } } "
        "public class Harness { public int Dispatch(Animal value) { return value.Speak(); } "
        "public int DispatchDog(Dog value) { return value.Speak(); } "
        "public int Direct(Animal value) { return value.NonVirtual(); } "
        "public int Run() { Animal value = new Dog(5, 20); return value.Speak(); } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "MacroAVirtual.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 34 virtual/override program compiles through physical OOP MIR");
    if (!ok || compilation.virtualDispatchModel() == nullptr || compilation.oopMirModel() == nullptr ||
        compilation.memberModel() == nullptr)
    {
        return;
    }

    const auto animal = findTypeOrdinal(compilation, "Animal");
    const auto dog = findTypeOrdinal(compilation, "Dog");
    tests.expect(animal.has_value() && dog.has_value(), "virtual fixture resolves Animal and Dog type identities");
    if (!animal.has_value() || !dog.has_value())
    {
        return;
    }
    const auto animalSpeak = findMemberOrdinal(compilation, *animal, "Speak");
    const auto dogSpeak = findMemberOrdinal(compilation, *dog, "Speak");
    tests.expect(animalSpeak.has_value() && dogSpeak.has_value(), "both virtual declaration and override are member symbols");
    if (!animalSpeak.has_value() || !dogSpeak.has_value())
    {
        return;
    }

    const auto animalSlot = compilation.virtualDispatchModel()->slotForMember(*animalSpeak);
    const auto dogSlot = compilation.virtualDispatchModel()->slotForMember(*dogSpeak);
    const ClassVirtualTable* dogTable = compilation.virtualDispatchModel()->classTable(*dog);
    tests.expect(animalSlot == std::optional<std::size_t>(0) && dogSlot == animalSlot,
        "override preserves the root virtual slot instead of allocating a new slot");
    tests.expect(dogTable != nullptr && dogTable->slotMemberOrdinals.size() == 1 &&
            dogTable->slotMemberOrdinals[0] == *dogSpeak,
        "Dog vtable replaces Animal.Speak slot zero with Dog.Speak");

    std::size_t virtualCalls = 0;
    std::size_t directCalls = 0;
    bool sawBaseDirect = false;
    for (const auto& function : compilation.oopMirModel()->functions())
    {
        virtualCalls += countMirKind(function.body.get(), ProjectOopMirNodeKind::VirtualInstanceCall);
        directCalls += countMirKind(function.body.get(), ProjectOopMirNodeKind::DirectInstanceCall);
        const ProjectOopMirNode* direct = findMirKind(function.body.get(), ProjectOopMirNodeKind::DirectInstanceCall);
        sawBaseDirect = sawBaseDirect || (direct != nullptr && direct->baseReceiver);
    }
    tests.expect(virtualCalls >= 3,
        "calls through Animal and Dog static types both use virtual dispatch when the selected method owns a slot");
    tests.expect(directCalls >= 3 && sawBaseDirect,
        "constructors/non-virtual/base-qualified calls remain direct and base.Speak never dispatches dynamically");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Stage 34 MIR lowers to LLVM with dispatch metadata");
    if (module == nullptr)
    {
        return;
    }
    std::string verifyError;
    tests.expect(module->verify(verifyError) && verifyError.empty(), "LLVM verifier accepts Stage 34 virtual dispatch");
    const std::string llvm = module->text();
    tests.expect(llvm.find("__nsc_vtable_") != std::string::npos &&
            llvm.find("__nsc_classdesc_") != std::string::npos,
        "LLVM module emits NativeSharp vtables and class descriptors as explicit globals");
    tests.expect(llvm.find("call i32 %vtarget") != std::string::npos,
        "virtual call lowers to an indirect LLVM call through the loaded slot target");

    std::string targetError;
    auto targetMachine = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(targetMachine != nullptr && targetError.empty(), "Windows x64 target machine exists for Macro A virtual codegen");
    if (targetMachine != nullptr)
    {
        tests.expect(targetMachine->configureModuleForTarget(*module, targetError),
            "Stage 34 LLVM receives the Windows x64 target data layout");
        nativesharp::llvm_backend::LlvmOptimizer optimizer;
        tests.expect(optimizer.optimize(
                *module, *targetMachine, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
            "LLVM O2 accepts and optimizes the dynamic-dispatch module");
        const auto objectPath = directory / "macro_a_virtual.obj";
        tests.expect(targetMachine->emit(
                *module, objectPath, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
            "Stage 34 optimized module emits Windows x64 COFF object code");
        nativesharp::llvm_backend::CoffObjectVerifier verifier;
        tests.expect(verifier.verify(objectPath) && verifier.info().machine == 0x8664,
            "Macro A virtual-dispatch COFF passes structural AMD64 verification");
    }
}

void testReceiverAccessibilityAndOverloads(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "ReceiverOverload";
        writeProject(directory, "MacroAReceiverOverload");
        writeFile(directory / "Main.ns",
            "public class Overloaded { public int F(int value) { return 1; } "
            "public int F(long value) { return 2; } } "
            "public class Harness { public int Use(Overloaded value) { return value.F(1); } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopHir(directory / "MacroAReceiverOverload.nsproj", error);
        tests.expect(ok && error.empty(),
            "explicit-object method calls reuse normal overload resolution instead of a receiver-specific shortcut");
        if (ok && compilation.oopHirModel() != nullptr && compilation.memberModel() != nullptr)
        {
            const auto harness = findTypeOrdinal(compilation, "Harness");
            const auto use = harness.has_value() ? findMemberOrdinal(compilation, *harness, "Use") : std::nullopt;
            const ProjectOopHirNode* selectedCall = nullptr;
            if (use.has_value())
            {
                if (const auto* function = compilation.oopHirModel()->functionForMember(*use); function != nullptr)
                {
                    selectedCall = findHirKind(function->body.get(), ProjectOopHirNodeKind::InstanceCall);
                }
            }
            bool selectedExactInt = false;
            if (selectedCall != nullptr && selectedCall->memberOrdinal.has_value() &&
                *selectedCall->memberOrdinal < compilation.memberModel()->members().size())
            {
                const auto& member = compilation.memberModel()->members()[*selectedCall->memberOrdinal];
                selectedExactInt = member.parameters.size() == 1 &&
                    member.parameters[0].typeKey == "builtin:int";
            }
            tests.expect(selectedExactInt,
                "generalized receiver overload ranking selects exact int over widening int-to-long");
        }
    }

    {
        const auto directory = root / "SameProjectInternalReceiver";
        writeProject(directory, "MacroASameProjectInternalReceiver");
        writeFile(directory / "Main.ns",
            "public class Api { internal int Value; internal int Read() { return Value; } } "
            "public class Probe { public int Use(Api api) { api.Value = 7; return api.Read(); } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "MacroASameProjectInternalReceiver.nsproj", error);
        tests.expect(ok && error.empty(),
            "internal fields and methods are accessible through generalized receivers inside the same project");
    }

    {
        const auto directory = root / "CrossProjectPublicReceiver";
        const auto core = directory / "Core";
        const auto app = directory / "App";
        writeProject(core, "Core");
        writeFile(core / "Api.ns",
            "public class Api { public int Value; public int Read() { return Value; } }");
        writeProject(app, "App", "<ProjectReference Include=\"../Core/Core.nsproj\" />");
        writeFile(app / "Probe.ns",
            "public class Probe { public int Use(Api api) { api.Value = 9; return api.Read(); } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(app / "App.nsproj", error);
        tests.expect(ok && error.empty(),
            "public members remain accessible through object receivers across project-reference boundaries");
    }

    {
        const auto directory = root / "CrossProjectInternalReceiver";
        const auto core = directory / "Core";
        const auto app = directory / "App";
        writeProject(core, "Core");
        writeFile(core / "Api.ns",
            "public class Api { internal int Hidden; public int Visible; }");
        writeProject(app, "App", "<ProjectReference Include=\"../Core/Core.nsproj\" />");
        writeFile(app / "Probe.ns",
            "public class Probe { public int Bad(Api api) { return api.Hidden; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(app / "App.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InaccessibleMember) == 1,
            "internal receiver access is rejected across project boundaries even when the containing class is public");
    }

    {
        const auto directory = root / "PartialPrivateReceiver";
        writeProject(directory, "MacroAPartialPrivateReceiver");
        writeFile(directory / "Main.ns",
            "partial class Box { private int Value; } "
            "partial class Box { int Copy(Box other) { Value = other.Value; return Value; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "MacroAPartialPrivateReceiver.nsproj", error);
        tests.expect(ok && error.empty(),
            "partial declarations share one class identity, so same-class private receiver access crosses declaration parts");
    }
}

void testVirtualSlotStability(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "VirtualSlots";
    writeProject(directory, "MacroAVirtualSlots");
    writeFile(directory / "Main.ns",
        "public class A { public virtual int F() { return 1; } public virtual int G() { return 2; } } "
        "public class B : A { public override int G() { return 20; } } "
        "public class Final : B { public override int F() { return 10; } } "
        "public class Plain { public int Value() { return 3; } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "MacroAVirtualSlots.nsproj", error);
    tests.expect(ok && error.empty(),
        "multi-level inheritance with independent virtual families compiles through Stage 34 MIR");
    if (!ok || compilation.virtualDispatchModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const auto a = findTypeOrdinal(compilation, "A");
    const auto b = findTypeOrdinal(compilation, "B");
    const auto c = findTypeOrdinal(compilation, "Final");
    const auto plain = findTypeOrdinal(compilation, "Plain");
    tests.expect(a.has_value() && b.has_value() && c.has_value() && plain.has_value(),
        "virtual-slot fixture resolves all hierarchy and non-polymorphic class identities");
    if (!a.has_value() || !b.has_value() || !c.has_value() || !plain.has_value()) return;

    const auto aF = findMemberOrdinal(compilation, *a, "F");
    const auto aG = findMemberOrdinal(compilation, *a, "G");
    const auto bG = findMemberOrdinal(compilation, *b, "G");
    const auto cF = findMemberOrdinal(compilation, *c, "F");
    tests.expect(aF.has_value() && aG.has_value() && bG.has_value() && cF.has_value(),
        "all virtual roots and overrides have concrete member ordinals");
    if (!aF.has_value() || !aG.has_value() || !bG.has_value() || !cF.has_value()) return;

    const auto slotAF = compilation.virtualDispatchModel()->slotForMember(*aF);
    const auto slotAG = compilation.virtualDispatchModel()->slotForMember(*aG);
    const auto slotBG = compilation.virtualDispatchModel()->slotForMember(*bG);
    const auto slotCF = compilation.virtualDispatchModel()->slotForMember(*cF);
    tests.expect(slotAF == std::optional<std::size_t>(0) &&
            slotAG == std::optional<std::size_t>(1) && slotBG == slotAG && slotCF == slotAF,
        "virtual slots are assigned once at roots and remain stable across independent multi-level overrides");

    const ClassVirtualTable* bTable = compilation.virtualDispatchModel()->classTable(*b);
    const ClassVirtualTable* cTable = compilation.virtualDispatchModel()->classTable(*c);
    tests.expect(bTable != nullptr && bTable->slotMemberOrdinals.size() == 2 &&
            bTable->slotMemberOrdinals[0] == *aF && bTable->slotMemberOrdinals[1] == *bG,
        "B inherits slot F unchanged and replaces only slot G");
    tests.expect(cTable != nullptr && cTable->slotMemberOrdinals.size() == 2 &&
            cTable->slotMemberOrdinals[0] == *cF && cTable->slotMemberOrdinals[1] == *bG,
        "Final replaces inherited slot F while preserving B's replacement of G");

    const auto* plainDispatch = compilation.oopMirModel()->dispatchTableForType(*plain);
    tests.expect(plainDispatch != nullptr && plainDispatch->slotMemberOrdinals.empty(),
        "even a class with no virtual methods has descriptor metadata, preserving the uniform class-object header ABI");
}

void testNegativeSemantics(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "Cycle";
        writeProject(directory, "MacroACycle");
        writeFile(directory / "Main.ns", "class A : B { } class B : A { }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectInheritance(directory / "MacroACycle.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InheritanceCycle) >= 1,
            "single-inheritance graph rejects cycles before layout or lowering");
    }

    {
        const auto directory = root / "PrimitiveReceiver";
        writeProject(directory, "MacroAPrimitiveReceiver");
        writeFile(directory / "Main.ns", "class Box { int Read(int value) { return value.Nope; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "MacroAPrimitiveReceiver.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidMemberAccess) == 1,
            "member access rejects non-class receivers instead of inventing boxing semantics");
    }

    {
        const auto directory = root / "ProtectedReceiver";
        writeProject(directory, "MacroAProtectedReceiver");
        writeFile(directory / "Main.ns",
            "class Base { protected int Value; } class Derived : Base { "
            "int Bad(Base other) { return other.Value; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "MacroAProtectedReceiver.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InaccessibleMember) == 1,
            "protected member cannot be selected through an arbitrary base-typed receiver from a derived class");
    }

    {
        const auto directory = root / "OverrideNonVirtual";
        writeProject(directory, "MacroAOverrideNonVirtual");
        writeFile(directory / "Main.ns",
            "class Base { public int F() { return 1; } } "
            "class Derived : Base { public override int F() { return 2; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectVirtualDispatch(directory / "MacroAOverrideNonVirtual.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOverride) == 1,
            "override targeting a non-virtual inherited method is rejected deterministically");
    }

    {
        const auto directory = root / "OverrideReturn";
        writeProject(directory, "MacroAOverrideReturn");
        writeFile(directory / "Main.ns",
            "class Base { public virtual int F() { return 1; } } "
            "class Derived : Base { public override long F() { return 2; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectVirtualDispatch(directory / "MacroAOverrideReturn.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOverride) == 1,
            "Macro A override requires exact return-type preservation");
    }
    {
        const auto directory = root / "PartialBaseConflict";
        writeProject(directory, "MacroAPartialBaseConflict");
        writeFile(directory / "Main.ns",
            "class A { } class B { } partial class D : A { } partial class D : B { }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectInheritance(
            directory / "MacroAPartialBaseConflict.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::PartialTypeConflict) == 1,
            "partial class declarations cannot disagree about the direct base class");
    }

    {
        const auto directory = root / "PartialSingleBase";
        writeProject(directory, "MacroAPartialSingleBase");
        writeFile(directory / "Main.ns",
            "class A { } partial class D : A { } partial class D { }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectInheritance(directory / "MacroAPartialSingleBase.nsproj", error);
        tests.expect(ok && compilation.inheritanceModel() != nullptr,
            "a partial class may specify its base class on exactly one declaration part");
        if (ok && compilation.inheritanceModel() != nullptr)
        {
            const auto a = findTypeOrdinal(compilation, "A");
            const auto d = findTypeOrdinal(compilation, "D");
            tests.expect(a.has_value() && d.has_value() &&
                    compilation.inheritanceModel()->baseTypeOrdinal(*d) == a,
                "the single partial base specification becomes the canonical direct base relation");
        }
    }

    {
        const auto directory = root / "StructBase";
        writeProject(directory, "MacroAStructBase");
        writeFile(directory / "Main.ns", "struct S { int Value; } class D : S { }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectInheritance(directory / "MacroAStructBase.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidBaseType) == 1,
            "single inheritance accepts class bases only and rejects struct bases");
    }

    {
        const auto directory = root / "ProtectedDerivedReceiver";
        writeProject(directory, "MacroAProtectedDerivedReceiver");
        writeFile(directory / "Main.ns",
            "class Base { protected int Value; } class Derived : Base { "
            "int Good(Derived other) { other.Value = 3; return other.Value + Value; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "MacroAProtectedDerivedReceiver.nsproj", error);
        tests.expect(ok,
            "protected access remains legal through this/implicit-this and derived-typed receivers");
    }

    {
        const auto directory = root / "SameClassPrivateReceiver";
        writeProject(directory, "MacroASameClassPrivateReceiver");
        writeFile(directory / "Main.ns",
            "class Box { private int Value; int Copy(Box other) { Value = other.Value; return Value; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "MacroASameClassPrivateReceiver.nsproj", error);
        tests.expect(ok,
            "private members remain accessible through another receiver of the same declaring class");
    }

    {
        const auto directory = root / "CrossClassPrivateReceiver";
        writeProject(directory, "MacroACrossClassPrivateReceiver");
        writeFile(directory / "Main.ns",
            "class Secret { private int Value; } class Probe { int Read(Secret value) { return value.Value; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "MacroACrossClassPrivateReceiver.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InaccessibleMember) == 1,
            "private receiver access is rejected from a different class");
    }

    {
        const auto directory = root / "CrossProjectInternalOverride";
        const auto core = directory / "Core";
        const auto app = directory / "App";
        writeProject(core, "Core");
        writeFile(core / "Base.ns",
            "public class Base { internal virtual int Hidden() { return 1; } "
            "public virtual int Visible() { return 2; } }");
        writeProject(app, "App", "<ProjectReference Include=\"../Core/Core.nsproj\" />");
        writeFile(app / "Derived.ns",
            "public class Derived : Base { internal override int Hidden() { return 3; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectVirtualDispatch(app / "App.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOverride) == 1,
            "an override cannot target a cross-project internal virtual method that is inaccessible to the derived class");
    }

    {
        const auto directory = root / "HiddenCrossProjectBase";
        const auto core = directory / "Core";
        const auto app = directory / "App";
        writeProject(core, "Core");
        writeFile(core / "Base.ns", "class HiddenBase { }");
        writeProject(app, "App", "<ProjectReference Include=\"../Core/Core.nsproj\" />");
        writeFile(app / "Derived.ns", "public class Derived : HiddenBase { }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectInheritance(app / "App.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::UnknownTypeName) >= 1,
            "an internal class from another project cannot be named as a base type");
    }

    {
        const auto directory = root / "PrivateVirtual";
        writeProject(directory, "MacroAPrivateVirtual");
        writeFile(directory / "Main.ns", "class Base { private virtual int F() { return 1; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectVirtualDispatch(directory / "MacroAPrivateVirtual.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOverride) == 1,
            "a private method cannot introduce a virtual dispatch family");
    }

    {
        const auto directory = root / "OverrideParameters";
        writeProject(directory, "MacroAOverrideParameters");
        writeFile(directory / "Main.ns",
            "class Base { public virtual int F(int value) { return value; } } "
            "class Derived : Base { public override int F(long value) { return 2; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectVirtualDispatch(directory / "MacroAOverrideParameters.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOverride) == 1,
            "override requires the exact inherited parameter signature and cannot create a new overload family");
    }

    {
        const auto directory = root / "OverrideAccessibility";
        writeProject(directory, "MacroAOverrideAccessibility");
        writeFile(directory / "Main.ns",
            "class Base { public virtual int F() { return 1; } } "
            "class Derived : Base { protected override int F() { return 2; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectVirtualDispatch(directory / "MacroAOverrideAccessibility.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOverride) == 1,
            "Macro A override preserves accessibility exactly instead of silently narrowing the virtual contract");
    }

    {
        const auto directory = root / "VirtualHiding";
        writeProject(directory, "MacroAVirtualHiding");
        writeFile(directory / "Main.ns",
            "class Base { public virtual int F() { return 1; } } "
            "class Derived : Base { public int F() { return 2; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectVirtualDispatch(directory / "MacroAVirtualHiding.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOverride) == 1,
            "the Macro A subset rejects silent hiding of an inherited virtual signature and requires override");
    }

}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_macro_a_oop_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testGeneralizedReceivers(tests, root);
    testReceiverAccessibilityAndOverloads(tests, root);
    testInheritanceLayoutAndConstruction(tests, root);
    testVirtualDispatch(tests, root);
    testVirtualSlotStability(tests, root);
    testNegativeSemantics(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
