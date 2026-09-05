#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectConversionKind;
using nativesharp::frontend::ProjectOopMirNode;
using nativesharp::frontend::ProjectOopMirNodeKind;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

std::size_t diagnosticCount(const ProjectCompilation& compilation, DiagnosticId id)
{
    std::size_t count = 0;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        count += diagnostic.id() == id ? 1U : 0U;
    }
    return count;
}

std::optional<std::size_t> findType(const ProjectCompilation& compilation, std::string_view suffix)
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

std::optional<std::size_t> findMember(
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
        if (const ProjectOopMirNode* found = findMirKind(child.get(), kind); found != nullptr)
        {
            return found;
        }
    }
    return nullptr;
}

std::size_t countMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr)
    {
        return 0;
    }
    std::size_t count = node->kind == kind ? 1U : 0U;
    for (const auto& child : node->children)
    {
        count += countMirKind(child.get(), kind);
    }
    return count;
}

bool containsOrdinal(std::span<const std::size_t> values, std::size_t ordinal)
{
    return std::find(values.begin(), values.end(), ordinal) != values.end();
}

void testSyntaxAndInterfaceDag(TestContext& tests, const std::filesystem::path& root)
{
    {
        nativesharp::diagnostics::DiagnosticEngine diagnostics;
        nativesharp::source::SourceManager sources;
        const nativesharp::FileId fileId = sources.addBuffer("stage36_keywords.ns", "interface");
        const auto* file = sources.getFile(fileId);
        nativesharp::lexer::Lexer lexer(*file, diagnostics);
        const std::vector<nativesharp::lexer::Token> tokens = lexer.lexAll();
        tests.expect(!diagnostics.hasErrors(), "Stage 36 interface keyword corpus lexes without diagnostics");
        tests.expect(tokens.size() == 2 && tokens[0].kind() == nativesharp::lexer::TokenKind::KwInterface,
            "interface is a true reserved token rather than a contextual identifier");
    }

    const auto directory = root / "Dag";
    writeProject(directory, "Stage36Dag");
    writeFile(directory / "InterfacesA.ns",
        "namespace Demo { "
        "public interface IBase { int Read(); } "
        "public interface IStatus { int Status(); } "
        "public interface IAdvanced : IBase, IStatus { int Advanced(); } "
        "public interface ILeft : IBase { } "
        "public interface IRight : IBase { } "
        "public partial interface IDiamond : ILeft { int Final(); } "
        "}");
    writeFile(directory / "InterfacesB.ns",
        "namespace Demo { public partial interface IDiamond : IRight { } }");
    writeFile(directory / "Classes.ns",
        "namespace Demo { "
        "public abstract class BaseDevice : IBase { public abstract int Read(); } "
        "public class Sensor : BaseDevice, IAdvanced, IDiamond { "
        "public override int Read() { return 10; } "
        "public int Status() { return 20; } "
        "public int Advanced() { return 30; } "
        "public int Final() { return 40; } "
        "} "
        "}");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectInterfaces(directory / "Stage36Dag.nsproj", error);
    tests.expect(ok && error.empty(), "multiple/diamond/partial interface inheritance builds through ProjectInterfaceModel");
    tests.expect(compilation.interfaceModel() != nullptr && compilation.interfaceModel()->isBuilt(),
        "Stage 36 has an explicit built ProjectInterfaceModel");
    if (!ok || compilation.interfaceModel() == nullptr)
    {
        return;
    }

    const auto iBase = findType(compilation, "IBase");
    const auto iStatus = findType(compilation, "IStatus");
    const auto iAdvanced = findType(compilation, "IAdvanced");
    const auto iDiamond = findType(compilation, "IDiamond");
    const auto sensor = findType(compilation, "Sensor");
    const auto baseDevice = findType(compilation, "BaseDevice");
    tests.expect(iBase && iStatus && iAdvanced && iDiamond && sensor && baseDevice,
        "interface DAG fixture resolves all interface/class identities");
    if (!iBase || !iStatus || !iAdvanced || !iDiamond || !sensor || !baseDevice)
    {
        return;
    }

    const auto* advancedInfo = compilation.interfaceModel()->interfaceInfo(*iAdvanced);
    const auto* diamondInfo = compilation.interfaceModel()->interfaceInfo(*iDiamond);
    tests.expect(advancedInfo != nullptr && advancedInfo->directBaseInterfaceOrdinals.size() == 2,
        "IAdvanced retains both direct base-interface edges");
    tests.expect(compilation.interfaceModel()->interfaceDerivesFrom(*iAdvanced, *iBase) &&
            compilation.interfaceModel()->interfaceDerivesFrom(*iAdvanced, *iStatus),
        "interface transitive relation recognizes both inherited contracts");
    tests.expect(diamondInfo != nullptr && diamondInfo->effectiveMethodMemberOrdinals.size() == 2,
        "diamond inheritance deduplicates IBase.Read and adds IDiamond.Final exactly once");
    tests.expect(compilation.interfaceModel()->classImplementsInterface(*sensor, *iAdvanced) &&
            compilation.interfaceModel()->classImplementsInterface(*sensor, *iBase) &&
            compilation.interfaceModel()->classImplementsInterface(*sensor, *iStatus) &&
            compilation.interfaceModel()->classImplementsInterface(*sensor, *iDiamond),
        "class implementation closure includes directly and transitively implemented interfaces");
    tests.expect(compilation.interfaceModel()->classImplementsInterface(*baseDevice, *iBase),
        "abstract class may publish an interface contract before a concrete derived type completes it");

    const auto* advancedPlan = compilation.interfaceModel()->tablePlan(*sensor, *iAdvanced);
    const auto* basePlan = compilation.interfaceModel()->tablePlan(*sensor, *iBase);
    const auto* diamondPlan = compilation.interfaceModel()->tablePlan(*sensor, *iDiamond);
    tests.expect(advancedPlan != nullptr && advancedPlan->implementationMemberOrdinals.size() == 3,
        "Sensor/IAdvanced table contains slots for inherited and direct interface methods");
    tests.expect(basePlan != nullptr && basePlan->implementationMemberOrdinals.size() == 1 &&
            basePlan->implementationMemberOrdinals[0].has_value(),
        "Sensor/IBase table resolves the inherited abstract contract to a concrete implementation");
    tests.expect(diamondPlan != nullptr && diamondPlan->implementationMemberOrdinals.size() == 2,
        "Sensor/IDiamond table has one deduplicated base slot plus Final");
    tests.expect(advancedPlan != nullptr && advancedPlan->stableInterfaceTypeId != 0,
        "interface table plan carries a deterministic stable interface type id");
}

void testInheritedImplementationsAndConversions(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Conversions";
    writeProject(directory, "Stage36Conversions");
    writeFile(directory / "Main.ns",
        "interface IBase { int Read(); } "
        "interface IAdvanced : IBase { int Extra(); } "
        "class Root : IBase { public virtual int Read() { return 1; } } "
        "class Derived : Root, IAdvanced { "
        "public override int Read() { return 2; } "
        "public int Extra() { return 3; } "
        "} "
        "class Holder { "
        "private IBase Value; "
        "public Holder(IBase value) { Value = value; } "
        "public IBase Get() { return Value; } "
        "public int Invoke() { return Value.Read(); } "
        "} "
        "class Harness { "
        "public static IBase Make() { return new Derived(); } "
        "public static int Consume(IBase value) { return value.Read(); } "
        "public static int Run() { "
        "IAdvanced advanced = new Derived(); "
        "IBase baseValue = advanced; "
        "Holder holder = new Holder(new Derived()); "
        "IBase nullable = null; "
        "int result = Consume(baseValue) + holder.Invoke(); "
        "delete advanced; delete holder; delete nullable; return result; "
        "} "
        "}");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage36Conversions.nsproj", error);
    tests.expect(ok && error.empty(), "class/interface conversions, interface fields/parameters/returns and calls compile through MIR");
    tests.expect(compilation.interfaceModel() != nullptr && compilation.oopMirModel() != nullptr,
        "conversion fixture reaches interface and MIR models");
    if (!ok || compilation.interfaceModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const auto iBase = findType(compilation, "IBase");
    const auto iAdvanced = findType(compilation, "IAdvanced");
    const auto rootType = findType(compilation, "Root");
    const auto derived = findType(compilation, "Derived");
    const auto harness = findType(compilation, "Harness");
    tests.expect(iBase && iAdvanced && rootType && derived && harness,
        "conversion fixture resolves interface/base/derived/harness types");
    if (!iBase || !iAdvanced || !rootType || !derived || !harness)
    {
        return;
    }

    const auto* rootPlan = compilation.interfaceModel()->tablePlan(*rootType, *iBase);
    const auto* derivedBasePlan = compilation.interfaceModel()->tablePlan(*derived, *iBase);
    tests.expect(rootPlan != nullptr && rootPlan->implementationMemberOrdinals[0].has_value(),
        "base class directly implementing IBase has a concrete interface table");
    tests.expect(derivedBasePlan != nullptr && derivedBasePlan->implementationMemberOrdinals[0].has_value(),
        "derived class inherits IBase and gets its own dynamic interface table");
    if (derivedBasePlan != nullptr && derivedBasePlan->implementationMemberOrdinals[0].has_value())
    {
        const std::size_t implementation = *derivedBasePlan->implementationMemberOrdinals[0];
        tests.expect(compilation.memberModel() != nullptr &&
                compilation.memberModel()->members()[implementation].declaringTypeOrdinal == *derived,
            "derived override replaces the base method in the derived interface table");
    }

    const auto run = findMember(compilation, *harness, "Run");
    const auto consume = findMember(compilation, *harness, "Consume");
    const auto* function = run ? compilation.oopMirModel()->functionForMember(*run) : nullptr;
    const auto* consumeFunction = consume ? compilation.oopMirModel()->functionForMember(*consume) : nullptr;
    const ProjectOopMirNode* interfaceCall = consumeFunction != nullptr
        ? findMirKind(consumeFunction->body.get(), ProjectOopMirNodeKind::InterfaceInstanceCall)
        : nullptr;
    const ProjectOopMirNode* deleteNode = function != nullptr
        ? findMirKind(function->body.get(), ProjectOopMirNodeKind::Deallocate)
        : nullptr;
    const ProjectOopMirNode* convertNode = function != nullptr
        ? findMirKind(function->body.get(), ProjectOopMirNodeKind::Convert)
        : nullptr;
    tests.expect(interfaceCall != nullptr && interfaceCall->interfaceSlot.has_value(),
        "interface receiver call is a dedicated MIR InterfaceInstanceCall with an interface slot");
    tests.expect(deleteNode != nullptr && deleteNode->dynamicDestruction,
        "delete through an interface reference preserves descriptor-driven dynamic destruction");
    tests.expect(convertNode != nullptr,
        "interface assignment/argument flow preserves explicit conversion nodes in MIR");

    const auto interfacesOfDerived = compilation.interfaceModel()->interfacesOfClass(*derived);
    tests.expect(containsOrdinal(interfacesOfDerived, *iBase) && containsOrdinal(interfacesOfDerived, *iAdvanced),
        "derived class interface closure includes inherited IBase and direct IAdvanced");
}

void testCompositionAndSourceOrder(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Composition";
    writeProject(directory, "Stage36Composition");
    writeFile(directory / "A_Derived.ns",
        "namespace Composition { "
        "public class Derived : AbstractBase { public int Second() { return 22; } } "
        "public interface IOver { int Pick(int value); int Pick(float value); } "
        "public class OverImpl : IOver { "
        "public int Pick(int value) { return value; } "
        "public int Pick(float value) { return 2; } } "
        "public class Holder { private IContract Value; public Holder(IContract value) { Value = value; } public IContract Get() { return Value; } } "
        "public class Harness { public static int Run() { "
        "IContract contract = new Derived(); "
        "IOver over = new OverImpl(); "
        "int result = contract.First() + contract.Second() + over.Pick(1) + over.Pick(1.0f); "
        "Holder holder = new Holder(new Derived()); IContract returned = holder.Get(); "
        "delete contract; delete over; delete holder; return result + returned.First(); } } "
        "}");
    writeFile(directory / "Z_Base.ns",
        "namespace Composition { "
        "public interface IContract { int First(); int Second(); } "
        "public abstract class AbstractBase : IContract { public int First() { return 11; } } "
        "}");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage36Composition.nsproj", error);
    tests.expect(ok && error.empty(),
        "interface implementation inherited from a base class is independent of source-file order");
    tests.expect(compilation.interfaceModel() != nullptr && compilation.objectLayoutModel() != nullptr &&
            compilation.oopMirModel() != nullptr,
        "composition fixture reaches interface, object-layout and MIR models");
    if (!ok || compilation.interfaceModel() == nullptr || compilation.objectLayoutModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const auto contract = findType(compilation, "IContract");
    const auto abstractBase = findType(compilation, "AbstractBase");
    const auto derived = findType(compilation, "Derived");
    const auto iOver = findType(compilation, "IOver");
    const auto overImpl = findType(compilation, "OverImpl");
    const auto holder = findType(compilation, "Holder");
    const auto harness = findType(compilation, "Harness");
    tests.expect(contract && abstractBase && derived && iOver && overImpl && holder && harness,
        "composition fixture resolves all participating types despite derived-before-base source order");
    if (!contract || !abstractBase || !derived || !iOver || !overImpl || !holder || !harness)
    {
        return;
    }

    const auto* abstractPlan = compilation.interfaceModel()->tablePlan(*abstractBase, *contract);
    const auto* derivedPlan = compilation.interfaceModel()->tablePlan(*derived, *contract);
    tests.expect(abstractPlan != nullptr && abstractPlan->implementationMemberOrdinals.size() == 2 &&
            abstractPlan->implementationMemberOrdinals[0].has_value() &&
            !abstractPlan->implementationMemberOrdinals[1].has_value(),
        "abstract class may partially implement an interface and leave one table slot abstract");
    tests.expect(derivedPlan != nullptr && derivedPlan->implementationMemberOrdinals.size() == 2 &&
            derivedPlan->implementationMemberOrdinals[0].has_value() &&
            derivedPlan->implementationMemberOrdinals[1].has_value(),
        "concrete derived class completes the inherited interface obligation");
    if (derivedPlan != nullptr && derivedPlan->implementationMemberOrdinals.size() == 2 &&
        derivedPlan->implementationMemberOrdinals[0].has_value() &&
        derivedPlan->implementationMemberOrdinals[1].has_value())
    {
        const auto& first = compilation.memberModel()->members()[*derivedPlan->implementationMemberOrdinals[0]];
        const auto& second = compilation.memberModel()->members()[*derivedPlan->implementationMemberOrdinals[1]];
        tests.expect(first.declaringTypeOrdinal == *abstractBase && second.declaringTypeOrdinal == *derived,
            "concrete interface table can mix inherited base implementation and derived implementation slots");
    }

    const auto* overPlan = compilation.interfaceModel()->tablePlan(*overImpl, *iOver);
    tests.expect(overPlan != nullptr && overPlan->implementationMemberOrdinals.size() == 2,
        "overloaded interface methods occupy distinct slots by full signature");
    if (overPlan != nullptr && overPlan->interfaceMethodMemberOrdinals.size() == 2)
    {
        tests.expect(overPlan->interfaceMethodMemberOrdinals[0] != overPlan->interfaceMethodMemberOrdinals[1],
            "overloaded interface slots retain distinct contract member identities");
    }

    const auto* holderLayout = compilation.objectLayoutModel()->layoutForType(*holder);
    tests.expect(holderLayout != nullptr && holderLayout->fields.size() == 1 &&
            holderLayout->fields[0].size == 8 && holderLayout->fields[0].alignment == 8,
        "interface field is physically one native Windows-x64 pointer (8-byte size/alignment)");
    tests.expect(compilation.objectLayoutModel()->classReferenceLayout().size == 8,
        "class and interface references intentionally share the single-pointer ABI width");

    const auto run = findMember(compilation, *harness, "Run");
    const auto* runFunction = run ? compilation.oopMirModel()->functionForMember(*run) : nullptr;
    tests.expect(runFunction != nullptr &&
            countMirKind(runFunction->body.get(), ProjectOopMirNodeKind::InterfaceInstanceCall) >= 5,
        "MIR preserves interface dispatch for inherited contracts, overloads and returned interface references");
    tests.expect(runFunction != nullptr &&
            countMirKind(runFunction->body.get(), ProjectOopMirNodeKind::Deallocate) == 3,
        "multiple interface/class lifetime paths remain explicit dynamic delete operations");
}

void testSharedContractsAndInterfaceUpcasts(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "SharedContracts";
    writeProject(directory, "Stage36SharedContracts");
    writeFile(directory / "Main.ns",
        "interface IA { int F(); } "
        "interface IB { int F(); } "
        "interface ICombined : IA, IB { int F(); } "
        "class Impl : ICombined, IA, IB { public int F() { return 9; } } "
        "class Harness { public static int Run() { "
        "ICombined combined = new Impl(); IA a = combined; IB b = combined; "
        "int result = a.F() + b.F() + combined.F(); delete combined; return result; } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage36SharedContracts.nsproj", error);
    tests.expect(ok && error.empty(),
        "identical contracts inherited from multiple interfaces and redeclared identically remain coherent");
    if (!ok || compilation.interfaceModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const auto ia = findType(compilation, "IA");
    const auto ib = findType(compilation, "IB");
    const auto combined = findType(compilation, "ICombined");
    const auto impl = findType(compilation, "Impl");
    const auto harness = findType(compilation, "Harness");
    tests.expect(ia && ib && combined && impl && harness,
        "shared-contract fixture resolves all interface/class identities");
    if (!ia || !ib || !combined || !impl || !harness)
    {
        return;
    }

    const auto* combinedInfo = compilation.interfaceModel()->interfaceInfo(*combined);
    tests.expect(combinedInfo != nullptr && combinedInfo->effectiveMethodMemberOrdinals.size() == 1,
        "same-signature/same-return diamond contract is deduplicated to one effective interface slot");
    tests.expect(compilation.interfaceModel()->interfaceDerivesFrom(*combined, *ia) &&
            compilation.interfaceModel()->interfaceDerivesFrom(*combined, *ib),
        "combined interface supports implicit upcast to each direct base interface");

    const auto implemented = compilation.interfaceModel()->interfacesOfClass(*impl);
    tests.expect(implemented.size() == 3 && containsOrdinal(implemented, *ia) &&
            containsOrdinal(implemented, *ib) && containsOrdinal(implemented, *combined),
        "duplicate direct/transitive interface specifications are deduplicated in class implementation closure");
    const auto* planA = compilation.interfaceModel()->tablePlan(*impl, *ia);
    const auto* planB = compilation.interfaceModel()->tablePlan(*impl, *ib);
    const auto* planCombined = compilation.interfaceModel()->tablePlan(*impl, *combined);
    tests.expect(planA != nullptr && planB != nullptr && planCombined != nullptr,
        "class publishes independent ABI lookup entries for IA, IB and ICombined");
    tests.expect(planA != nullptr && planB != nullptr && planCombined != nullptr &&
            planA->implementationMemberOrdinals[0] == planB->implementationMemberOrdinals[0] &&
            planB->implementationMemberOrdinals[0] == planCombined->implementationMemberOrdinals[0],
        "one concrete method may satisfy identical contracts in multiple interface tables");

    const auto run = findMember(compilation, *harness, "Run");
    const auto* runFunction = run ? compilation.oopMirModel()->functionForMember(*run) : nullptr;
    tests.expect(runFunction != nullptr &&
            countMirKind(runFunction->body.get(), ProjectOopMirNodeKind::InterfaceInstanceCall) == 3,
        "calls through IA, IB and ICombined each remain interface-dispatch operations in MIR");
    tests.expect(runFunction != nullptr &&
            countMirKind(runFunction->body.get(), ProjectOopMirNodeKind::Convert) >= 3,
        "class-to-interface plus interface-to-base-interface conversions remain explicit in MIR");
}

void testNegatives(TestContext& tests, const std::filesystem::path& root)
{
    struct NegativeCase final
    {
        const char* name;
        const char* source;
        DiagnosticId expected;
        bool syntaxOnly;
    };

    const NegativeCase cases[] = {
        {"InterfaceCycle", "interface IA : IB { } interface IB : IA { }", DiagnosticId::InheritanceCycle, false},
        {"InterfaceInheritsClass", "class BaseClass { } interface I : BaseClass { int F(); }", DiagnosticId::InvalidBaseType, false},
        {"ClassAfterInterface", "class B { } interface I { int F(); } class D : I, B { public int F() { return 1; } }", DiagnosticId::InvalidBaseType, false},
        {"MissingImplementation", "interface I { int F(); } class Impl : I { }", DiagnosticId::InvalidOverride, false},
        {"WrongReturn", "interface I { int F(); } class Impl : I { public float F() { return 1.0f; } }", DiagnosticId::InvalidOverride, false},
        {"PrivateImplementation", "interface I { int F(); } class Impl : I { private int F() { return 1; } }", DiagnosticId::InvalidOverride, false},
        {"StaticImplementation", "interface I { int F(); } class Impl : I { public static int F() { return 1; } }", DiagnosticId::InvalidOverride, false},
        {"ConflictingDiamond", "interface IA { int F(); } interface IB { float F(); } interface IC : IA, IB { }", DiagnosticId::InvalidOverride, false},
        {"InterfacePointerBase", "interface IA { int F(); } interface IB : IA* { int G(); }", DiagnosticId::InvalidBaseType, false},
        {"InterfaceField", "interface I { int Value; }", DiagnosticId::ExpectedDeclaration, true},
        {"InterfaceMethodBody", "interface I { int F() { return 1; } }", DiagnosticId::ExpectedDeclaration, true},
        {"InterfacePrivateMethod", "interface I { private int F(); }", DiagnosticId::ExpectedDeclaration, true},
        {"InterfaceStaticMethod", "interface I { static int F(); }", DiagnosticId::ExpectedDeclaration, true}
    };

    for (const NegativeCase& negative : cases)
    {
        const auto directory = root / negative.name;
        writeProject(directory, negative.name);
        writeFile(directory / "Main.ns", negative.source);
        ProjectCompilation compilation;
        std::string error;
        const bool ok = negative.syntaxOnly
            ? compilation.compileProjectSyntax(directory / (std::string(negative.name) + ".nsproj"), error)
            : compilation.compileProjectInterfaces(directory / (std::string(negative.name) + ".nsproj"), error);
        tests.expect(!ok, std::string("Stage 36 negative is rejected: ") + negative.name);
        tests.expect(diagnosticCount(compilation, negative.expected) >= 1,
            std::string("Stage 36 negative reports intended diagnostic: ") + negative.name);
    }

    const auto allocationDirectory = root / "NewInterface";
    writeProject(allocationDirectory, "NewInterface");
    writeFile(allocationDirectory / "Main.ns",
        "interface I { int F(); } class H { public I Make() { return new I(); } }");
    ProjectCompilation allocation;
    std::string allocationError;
    const bool allocationOk = allocation.compileProjectOopMir(
        allocationDirectory / "NewInterface.nsproj", allocationError);
    tests.expect(!allocationOk, "interface type cannot be directly allocated with new");
    tests.expect(diagnosticCount(allocation, DiagnosticId::InvalidAllocationType) >= 1,
        "new interface reports InvalidAllocationType");

    const auto downcastDirectory = root / "InvalidImplicitDowncast";
    writeProject(downcastDirectory, "InvalidImplicitDowncast");
    writeFile(downcastDirectory / "Main.ns",
        "interface I { int F(); } class Impl : I { public int F() { return 1; } } "
        "class H { public Impl Bad(I value) { Impl result = value; return result; } }");
    ProjectCompilation downcast;
    std::string downcastError;
    const bool downcastOk = downcast.compileProjectMethodBodies(
        downcastDirectory / "InvalidImplicitDowncast.nsproj", downcastError);
    tests.expect(!downcastOk, "Stage 36 does not silently introduce interface-to-class downcasts");
    tests.expect(diagnosticCount(downcast, DiagnosticId::InvalidConversion) >= 1,
        "interface-to-class implicit downcast reports InvalidConversion");

    const auto siblingDirectory = root / "SiblingInterfaces";
    writeProject(siblingDirectory, "SiblingInterfaces");
    writeFile(siblingDirectory / "Main.ns",
        "interface IA { int A(); } interface IB { int B(); } "
        "class Impl : IA, IB { public int A() { return 1; } public int B() { return 2; } } "
        "class H { public IB Bad(IA value) { return value; } }");
    ProjectCompilation sibling;
    std::string siblingError;
    const bool siblingOk = sibling.compileProjectMethodBodies(
        siblingDirectory / "SiblingInterfaces.nsproj", siblingError);
    tests.expect(!siblingOk, "sibling interfaces are not implicitly interchangeable even if a class can implement both");
    tests.expect(diagnosticCount(sibling, DiagnosticId::InvalidConversion) >= 1,
        "sibling-interface implicit conversion reports InvalidConversion");
}

void testLlvmAndCoff(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Backend";
    writeProject(directory, "Stage36Backend");
    writeFile(directory / "Main.ns",
        "interface IReadable { int Read(); } "
        "interface ITagged { int Tag(); } "
        "class Sensor : IReadable, ITagged { "
        "private int Value; public Sensor(int value) { Value = value; } "
        "public int Read() { return Value; } public int Tag() { return 7; } "
        "~Sensor() { Value = 0; } } "
        "class Harness { public static int Run() { "
        "IReadable readable = new Sensor(42); int result = readable.Read(); delete readable; return result; } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage36Backend.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 36 backend fixture reaches OOP MIR");
    if (!ok || compilation.nameLookup() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const auto sensor = findType(compilation, "Sensor");
    tests.expect(sensor.has_value(), "backend fixture resolves dynamic Sensor class");
    const auto* dispatch = sensor ? compilation.oopMirModel()->dispatchTableForType(*sensor) : nullptr;
    tests.expect(dispatch != nullptr && dispatch->interfaceTables.size() == 2 && !dispatch->interfaceMapName.empty(),
        "MIR class descriptor owns two interface method tables and one interface map");
    if (dispatch != nullptr)
    {
        tests.expect(dispatch->interfaceTables[0].calleeNames.size() == 1 &&
                dispatch->interfaceTables[1].calleeNames.size() == 1,
            "each Stage 36 interface table carries its concrete callable slot");
    }

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Stage 36 MIR lowers to LLVM without backend errors");
    if (module == nullptr)
    {
        return;
    }

    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 36 interface LLVM is verifier-clean before optimization");
    const std::string llvm = module->text();
    tests.expect(llvm.find("%nativesharp.interface_map_entry = type { i64, ptr }") != std::string::npos,
        "LLVM ABI declares the compact interface-map entry layout");
    tests.expect(llvm.find("%nativesharp.interface_map = type { i64, ptr }") != std::string::npos,
        "LLVM ABI declares count+entries interface-map header");
    // NSC-LIMIT-009. LA BUSQUEDA LLEVA `@\"` Y NO `@`, y no es un adorno: todo simbolo que sale
    // de una identidad lleva el `$3A$3A` del `::` del modulo, y LLVM entrecomilla cualquier
    // nombre con un byte que no sea alfanumerico, `-`, `.` o `_`. Lo impreso es
    // `@\"__nsc_...\"`. Escrito a mano sin la comilla encuentra CERO, y en una asercion de
    // ausencia eso seria un verde falso. Ver TypeIdentity.h e `irTextSymbolReference`.
    tests.expect(llvm.find("@\"__nsc_ifmap_") != std::string::npos &&
            llvm.find("@\"__nsc_iftable_") != std::string::npos,
        "LLVM materializes per-class interface maps and method tables");
    tests.expect(llvm.find("interface.search.") != std::string::npos,
        "interface call lowering performs runtime type-id map lookup rather than compile-time class selection");
    tests.expect(llvm.find("interface.missing.") != std::string::npos,
        "interface dispatch has an explicit failure/trap path instead of unchecked UB");
    tests.expect(llvm.find("call void @__nativesharp_free") != std::string::npos,
        "delete through interface still reaches Stage 35 native destruction/free machinery");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 36 obtains Windows x64 LLVM target machine");
    if (target == nullptr)
    {
        return;
    }
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 36 module accepts Windows x64 triple/data layout");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(*module, *target, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "Stage 36 interface-dispatch LLVM survives O2 optimization");
    tests.expect(module->verify(verifyError), "optimized Stage 36 interface LLVM remains verifier-clean");

    const auto object = directory / "stage36-recovery.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "optimized Stage 36 module emits a Windows x64 COFF object");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object) && verifier.info().machine == 0x8664,
        "Stage 36 recovery object is structurally valid AMD64 COFF");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage36_recovery_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testSyntaxAndInterfaceDag(tests, root);
    testInheritedImplementationsAndConversions(tests, root);
    testCompositionAndSourceOrder(tests, root);
    testSharedContractsAndInterfaceUpcasts(tests, root);
    testNegatives(tests, root);
    testLlvmAndCoff(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
