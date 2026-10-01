#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectConstructionModelDumper.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::frontend::ConstructionResolutionKind;
using nativesharp::frontend::ProjectCompilation;
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
    std::string text = "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />";
    text += references;
    text += "</ItemGroup></Project>";
    writeFile(directory / (std::string(name) + ".nsproj"), text);
}

std::size_t diagnosticCount(const ProjectCompilation& compilation, DiagnosticId id)
{
    std::size_t count = 0;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.id() == id)
        {
            ++count;
        }
    }
    return count;
}

void testPrimary(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Primary";
    writeProject(directory, "Primary");
    writeFile(directory / "A.ns",
        "namespace Demo; public partial class Widget { int Value; public Widget() { this.Value = 0; } "
        "public Widget(int value) { this.Value = value; } public int Get() { return this.Value; } } "
        "public class Empty {}\n");
    writeFile(directory / "B.ns",
        "namespace Demo; public partial class Widget { public Widget(float value) { this.Value = 0; } "
        "public Widget MakeInt() { return new Widget(1); } "
        "public Widget MakeFloat() { return new Widget(1.0f); } "
        "public Empty MakeEmpty() { return new Empty(); } }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectConstructions(directory / "Primary.nsproj", error);
    tests.expect(ok, "cross-file constructors and object creation bind successfully");
    tests.expect(error.empty(), "primary construction fixture has no loader error");
    tests.expect(compilation.constructionModel() != nullptr && compilation.memberModel() != nullptr &&
            compilation.methodBodyModel() != nullptr,
        "construction compilation preserves all prerequisite semantic models");
    if (compilation.constructionModel() == nullptr || compilation.memberModel() == nullptr ||
        compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto& model = *compilation.constructionModel();
    tests.expect(model.constructions().size() == 3, "three new C(args) sites are recorded deterministically");
    if (model.constructions().size() == 3)
    {
        tests.expect(model.constructions()[0].resolution == ConstructionResolutionKind::ResolvedExplicit &&
                model.constructions()[0].selectedConstructorMemberOrdinal.has_value(),
            "integer construction resolves to an explicit constructor");
        tests.expect(model.constructions()[0].selectedConversions.size() == 1 &&
                model.constructions()[0].selectedConversions[0].rank == 0,
            "Widget(1) selects identity int conversion over float widening");
        tests.expect(model.constructions()[1].resolution == ConstructionResolutionKind::ResolvedExplicit &&
                model.constructions()[1].selectedConversions.size() == 1 &&
                model.constructions()[1].selectedConversions[0].rank == 0,
            "Widget(1.0f) selects the float constructor by identity");
        tests.expect(model.constructions()[2].resolution == ConstructionResolutionKind::ResolvedImplicitDefault &&
                model.constructions()[2].selectedImplicitDefault,
            "class without explicit constructors receives an implicit parameterless constructor");
    }

    std::size_t constructors = 0;
    for (const auto& member : compilation.memberModel()->members())
    {
        constructors += member.kind == nativesharp::frontend::ClassMemberKind::Constructor ? 1u : 0u;
    }
    tests.expect(constructors == 3, "partial class contributes one merged constructor overload set with three declarations");
    std::size_t constructorScopes = 0;
    if (compilation.methodBodyModel() != nullptr)
    {
        for (const auto& scope : compilation.methodBodyModel()->scopes())
        {
            constructorScopes += scope.kind == nativesharp::frontend::MethodScopeKind::Constructor ? 1u : 0u;
        }
    }
    tests.expect(constructorScopes == 3,
        "constructor bodies receive dedicated callable scopes and reuse Stage 27 parameter/local binding");

    std::ostringstream dump;
    nativesharp::frontend::ProjectConstructionModelDumper().dump(
        model, *compilation.memberModel(), *compilation.nameLookup(), dump);
    tests.expect(dump.str().find("selected=Widget(builtin:int)") != std::string::npos &&
            dump.str().find("selected=implicit-default") != std::string::npos,
        "construction dump exposes explicit and synthesized selections");
}

void testDuplicateAndDefaultSuppression(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "DuplicateCtor";
        writeProject(directory, "DuplicateCtor");
        writeFile(directory / "A.ns", "partial class Box { Box(int x) { } }\n");
        writeFile(directory / "B.ns", "partial class Box { public Box(int y) { } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMembers(directory / "DuplicateCtor.nsproj", error);
        tests.expect(!ok, "duplicate constructor signature across partial parts is rejected");
        tests.expect(diagnosticCount(compilation, DiagnosticId::DuplicateSymbol) == 1 &&
                diagnosticCount(compilation, DiagnosticId::PreviousDeclaration) == 1,
            "duplicate constructor reports duplicate and previous declaration diagnostics");
    }

    {
        const auto directory = root / "SuppressDefault";
        writeProject(directory, "SuppressDefault");
        writeFile(directory / "Main.ns",
            "class Box { public Box(int x) { } Box Make() { return new Box(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(directory / "SuppressDefault.nsproj", error);
        tests.expect(!ok, "any explicit constructor suppresses implicit parameterless synthesis");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidArgumentCount) == 1,
            "new Box() reports arity failure when only Box(int) exists");
    }
}

void testAccessibility(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "PrivateSameType";
        writeProject(directory, "PrivateSameType");
        writeFile(directory / "Main.ns",
            "class Secret { private Secret() { } Secret Clone() { return new Secret(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(compilation.compileProjectConstructions(directory / "PrivateSameType.nsproj", error),
            "private constructor is accessible from another instance callable of the same class");
    }

    {
        const auto directory = root / "PrivateOtherType";
        writeProject(directory, "PrivateOtherType");
        writeFile(directory / "Main.ns",
            "class Secret { private Secret() { } } class Maker { Secret Make() { return new Secret(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(directory / "PrivateOtherType.nsproj", error);
        tests.expect(!ok, "private constructor is inaccessible from a different class");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidCall) == 1,
            "inaccessible private constructor emits InvalidCall");
    }

    {
        const auto core = root / "Core";
        const auto app = root / "App";
        writeProject(core, "Core");
        writeFile(core / "Thing.ns", "namespace Shared; public class Thing { internal Thing() { } }\n");
        writeProject(app, "App", "<ProjectReference Include=\"../Core/Core.nsproj\" />");
        writeFile(app / "Maker.ns",
            "using Shared; class Maker { Thing Make() { return new Thing(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(app / "App.nsproj", error);
        tests.expect(!ok, "internal constructor does not cross a ProjectReference boundary");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidCall) == 1,
            "cross-module inaccessible constructor is diagnosed separately from type visibility");
    }

    {
        const auto core = root / "DefaultCore";
        const auto app = root / "DefaultApp";
        writeProject(core, "DefaultCore");
        writeFile(core / "Thing.ns", "namespace Shared; public class Thing { }\n");
        writeProject(app, "DefaultApp", "<ProjectReference Include=\"../DefaultCore/DefaultCore.nsproj\" />");
        writeFile(app / "Maker.ns",
            "using Shared; class Maker { Thing Make() { return new Thing(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(app / "DefaultApp.nsproj", error);
        tests.expect(ok && compilation.constructionModel() != nullptr &&
                compilation.constructionModel()->constructions().size() == 1 &&
                compilation.constructionModel()->constructions()[0].selectedImplicitDefault,
            "implicit default constructor of a public class is public across ProjectReference boundaries");
    }

    {
        const auto core = root / "PublicAliasCore";
        const auto app = root / "PublicAliasApp";
        writeProject(core, "PublicAliasCore");
        writeFile(core / "Thing.ns",
            "namespace Shared; public class Thing { public Thing(int value) { } }\n");
        writeProject(app, "PublicAliasApp", "<ProjectReference Include=\"../PublicAliasCore/PublicAliasCore.nsproj\" />");
        writeFile(app / "Maker.ns",
            "using Alias = Shared.Thing; class Maker { Alias Make() { return new Alias(1); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(app / "PublicAliasApp.nsproj", error);
        tests.expect(ok && compilation.constructionModel() != nullptr &&
                compilation.constructionModel()->constructions().size() == 1,
            "public explicit constructor is callable across ProjectReference through a type alias");
        if (compilation.constructionModel() != nullptr &&
            compilation.constructionModel()->constructions().size() == 1)
        {
            const auto& binding = compilation.constructionModel()->constructions()[0];
            tests.expect(binding.resolution == ConstructionResolutionKind::ResolvedExplicit &&
                    binding.selectedConstructorMemberOrdinal.has_value(),
                "alias target resolves to the underlying class constructor declaration space");
        }
    }

    {
        const auto directory = root / "HiddenExactArity";
        writeProject(directory, "HiddenExactArity");
        writeFile(directory / "Main.ns",
            "class Secret { private Secret() { } public Secret(int x) { } } "
            "class Maker { Secret Make() { return new Secret(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(directory / "HiddenExactArity.nsproj", error);
        tests.expect(!ok, "inaccessible exact-arity constructor is not misreported as missing arity");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidCall) == 1 &&
                diagnosticCount(compilation, DiagnosticId::InvalidArgumentCount) == 0,
            "accessibility wins over arity diagnostics when a hidden exact signature exists");
    }
}

void testInvalidTargetsAndBodies(TestContext& tests, const std::filesystem::path& root)
{
    {
        // C6 "struct members" flipped this case deliberately: `new Value()` on a STRUCT is now
        // legal and means "initialize in place", not "allocate". What stays rejected is a target
        // that is neither a class nor a struct -- an enum has no constructor to run and no
        // storage shape `new` could mean anything for.
        const auto directory = root / "ValueTarget";
        writeProject(directory, "ValueTarget");
        writeFile(directory / "Main.ns",
            "struct Value { int X; } class Maker { Value Make() { return new Value(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            compilation.compileProjectConstructions(directory / "ValueTarget.nsproj", error),
            "new T(args) accepts a struct target and constructs it in place");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidAllocationType) == 0,
            "struct object creation emits no InvalidAllocationType");
    }

    {
        const auto directory = root / "InvalidTarget";
        writeProject(directory, "InvalidTarget");
        writeFile(directory / "Main.ns",
            "enum Mode { A } class Maker { Mode Make() { return new Mode(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(directory / "InvalidTarget.nsproj", error);
        tests.expect(!ok, "new T(args) rejects a target that is neither a class nor a struct");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidAllocationType) == 1,
            "non-constructible object creation emits InvalidAllocationType");
    }

    {
        const auto directory = root / "LegacyClassNew";
        writeProject(directory, "LegacyClassNew");
        writeFile(directory / "Main.ns",
            "class Box { Box Make() { return new Box; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "LegacyClassNew.nsproj", error);
        tests.expect(!ok, "procedural new C form is rejected for class references in Stage 28");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidAllocationType) == 1,
            "class allocation without constructor syntax emits InvalidAllocationType");
    }

    {
        const auto directory = root / "GlobalConstruction";
        writeProject(directory, "GlobalConstruction");
        writeFile(directory / "Main.ns",
            "namespace Demo { class Box { } Box Make() { return new Box(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(
            directory / "GlobalConstruction.nsproj", error);
        tests.expect(ok && compilation.constructionModel() != nullptr,
            "NativeSharp 3.0 binds new C(args) in a namespace free function");
        tests.expect(compilation.constructionModel() != nullptr &&
                compilation.constructionModel()->constructions().size() == 1,
            "namespace-function construction records the selected constructor plan");
    }

    {
        const auto directory = root / "DirectCtorCall";
        writeProject(directory, "DirectCtorCall");
        writeFile(directory / "Main.ns",
            "class Box { Box() { } int Run() { Box(); return 0; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "DirectCtorCall.nsproj", error);
        tests.expect(!ok,
            "constructor declarations do not participate in ordinary implicit method lookup");
        tests.expect(diagnosticCount(compilation, DiagnosticId::UnknownIdentifier) == 1,
            "constructor-shaped Box() expression is parsed as a call and rejected as an unknown callable; construction requires new Box()");
    }

    {
        const auto directory = root / "CtorReturn";
        writeProject(directory, "CtorReturn");
        writeFile(directory / "Main.ns", "class Box { Box() { return 1; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "CtorReturn.nsproj", error);
        tests.expect(!ok, "constructor body cannot return a value");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidReturn) == 1,
            "constructor return-value violation emits InvalidReturn");
    }
}

void testRanking(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "ConstantRanking";
        writeProject(directory, "ConstantRanking");
        writeFile(directory / "Main.ns",
            "class Box { Box(byte x) { } Box(long x) { } Box Make() { return new Box(1); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(directory / "ConstantRanking.nsproj", error);
        tests.expect(ok && compilation.constructionModel() != nullptr,
            "constructor overloads reuse Stage 27 constant-integer conversion ranking");
        if (compilation.constructionModel() != nullptr && !compilation.constructionModel()->constructions().empty())
        {
            const auto& binding = compilation.constructionModel()->constructions()[0];
            tests.expect(binding.selectedConversions.size() == 1 && binding.selectedConversions[0].rank == 2,
                "new Box(1) prefers constant narrowing to byte over numeric widening to long");
        }
    }

    {
        const auto directory = root / "NullReferenceArgument";
        writeProject(directory, "NullReferenceArgument");
        writeFile(directory / "Main.ns",
            "class Node { } class Box { Box(Node value) { } Box Make() { return new Box(null); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(directory / "NullReferenceArgument.nsproj", error);
        tests.expect(ok && compilation.constructionModel() != nullptr,
            "constructor parameter binding accepts null for a class-reference parameter");
        if (compilation.constructionModel() != nullptr && !compilation.constructionModel()->constructions().empty())
        {
            const auto& binding = compilation.constructionModel()->constructions()[0];
            tests.expect(binding.selectedConversions.size() == 1 &&
                    binding.selectedConversions[0].kind == nativesharp::frontend::ProjectConversionKind::NullReference,
                "constructor selection records a contextual null-reference conversion");
        }
    }

    {
        const auto directory = root / "AmbiguousRanking";
        writeProject(directory, "AmbiguousRanking");
        writeFile(directory / "Main.ns",
            "class Box { Box(int a, long b) { } Box(long a, int b) { } "
            "Box Make(int a, int b) { return new Box(a, b); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(directory / "AmbiguousRanking.nsproj", error);
        tests.expect(!ok, "crossed multiargument conversion wins remain ambiguous for constructors");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidCall) == 1,
            "ambiguous constructor overload emits InvalidCall without source-order tiebreak");
    }
}
void testImplicitBaseConstructorChains(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "ImplicitChain";
        writeProject(directory, "ImplicitChain");
        writeFile(directory / "Main.ns",
            "public class A { public A() { } } public class B : A { } public class Leaf : B { "
            "public Leaf Make() { return new Leaf(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(directory / "ImplicitChain.nsproj", error);
        tests.expect(ok && compilation.constructionModel() != nullptr && compilation.memberModel() != nullptr,
            "implicit default constructors form a valid transitive base-constructor chain");
        if (compilation.constructionModel() != nullptr && compilation.memberModel() != nullptr)
        {
            const auto& model = *compilation.constructionModel();
            tests.expect(model.implicitDefaultConstructorBindings().size() == 2,
                "B and Leaf receive semantic-only implicit constructor obligations");
            tests.expect(model.constructions().size() == 1 &&
                    model.constructions()[0].selectedImplicitDefault &&
                    model.constructions()[0].implicitDefaultBaseConstructorMemberOrdinal.has_value(),
                "new Leaf() resolves its implicit constructor to an executable ancestor entry point");
            if (!model.constructions().empty() &&
                model.constructions()[0].implicitDefaultBaseConstructorMemberOrdinal.has_value())
            {
                const auto ordinal = *model.constructions()[0].implicitDefaultBaseConstructorMemberOrdinal;
                tests.expect(ordinal < compilation.memberModel()->members().size() &&
                        compilation.memberModel()->members()[ordinal].kind ==
                            nativesharp::frontend::ClassMemberKind::Constructor &&
                        compilation.memberModel()->members()[ordinal].name == "A",
                    "transitive implicit chain physically enters the nearest explicit ancestor A()");
            }
        }
    }

    {
        const auto directory = root / "MissingParameterlessBase";
        writeProject(directory, "MissingParameterlessBase");
        writeFile(directory / "Main.ns",
            "class A { public A(int value) { } } class B : A { }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(
            directory / "MissingParameterlessBase.nsproj", error);
        tests.expect(!ok,
            "implicit derived constructor is rejected at declaration level when base has no parameterless constructor");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidArgumentCount) == 1,
            "missing base parameterless constructor produces one primary arity diagnostic even without new B()");
    }

    {
        const auto directory = root / "PrivateParameterlessBase";
        writeProject(directory, "PrivateParameterlessBase");
        writeFile(directory / "Main.ns",
            "class A { private A() { } } class B : A { }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(
            directory / "PrivateParameterlessBase.nsproj", error);
        tests.expect(!ok,
            "implicit derived constructor is rejected when the base parameterless constructor is private");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InaccessibleMember) == 1,
            "private base constructor produces one declaration-level accessibility diagnostic");
    }

    {
        const auto directory = root / "ExplicitThroughImplicitBase";
        writeProject(directory, "ExplicitThroughImplicitBase");
        writeFile(directory / "Main.ns",
            "class A { protected A() { } } class B : A { } class Leaf : B { public Leaf() { } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(
            directory / "ExplicitThroughImplicitBase.nsproj", error);
        tests.expect(ok && compilation.constructionModel() != nullptr,
            "explicit Leaf() can chain through B's implicit constructor to protected A()");
        if (compilation.constructionModel() != nullptr)
        {
            const auto& bindings = compilation.constructionModel()->constructorInitializerBindings();
            tests.expect(bindings.size() == 2,
                "A() and Leaf() explicit declarations each receive a base-constructor binding");
            bool sawC = false;
            for (const auto& binding : bindings)
            {
                if (binding.targetTypeOrdinal.has_value() && binding.selectedImplicitDefault)
                {
                    sawC = true;
                    tests.expect(binding.resolution ==
                            nativesharp::frontend::ConstructorInitializerResolutionKind::ResolvedImplicitDefault &&
                            binding.selectedEntryConstructorMemberOrdinal.has_value(),
                        "Leaf() records B implicit-default selection plus the executable A() entry point");
                }
            }
            tests.expect(sawC, "explicit constructor binding exposes the elided implicit-base chain");
        }
    }
}

void testThisConstructorDelegation(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "ThisDelegation";
        writeProject(directory, "ThisDelegation");
        writeFile(directory / "Main.ns",
            "class Box { private int Value; public Box() : this(5) { } "
            "private Box(int value) { Value = value; } "
            "public Box(int left, int right) : this(left + right) { } "
            "public int Read() { return Value; } "
            "public int Make() { Box box = new Box(); return box.Read(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopHir(directory / "ThisDelegation.nsproj", error);
        tests.expect(ok && compilation.constructionModel() != nullptr && compilation.oopHirModel() != nullptr,
            "this(...) constructor delegation resolves and lowers through OOP HIR");
        if (compilation.constructionModel() != nullptr && compilation.oopHirModel() != nullptr)
        {
            std::size_t thisBindings = 0;
            for (const auto& binding : compilation.constructionModel()->constructorInitializerBindings())
            {
                if (binding.kind == nativesharp::frontend::ConstructorInitializerKind::ExplicitThis)
                {
                    ++thisBindings;
                    tests.expect(binding.resolution ==
                            nativesharp::frontend::ConstructorInitializerResolutionKind::ResolvedExplicit &&
                            binding.selectedConstructorMemberOrdinal.has_value() &&
                            binding.targetTypeOrdinal.has_value() &&
                            *binding.targetTypeOrdinal == binding.containingTypeOrdinal,
                        "this(...) selects an explicit constructor in the same containing class");
                }
            }
            tests.expect(thisBindings == 2,
                "both delegating Box constructors receive explicit-this initializer bindings");

            std::size_t initializerCalls = 0;
            bool allThisCallsAreDirectReceivers = true;
            for (const auto& function : compilation.oopHirModel()->functions())
            {
                if (!function.constructor || function.body == nullptr || function.body->children.empty())
                {
                    continue;
                }
                const auto* first = function.body->children.front().get();
                if (first->kind == nativesharp::frontend::ProjectOopHirNodeKind::ConstructorInitializerCall)
                {
                    ++initializerCalls;
                    allThisCallsAreDirectReceivers = allThisCallsAreDirectReceivers && !first->baseReceiver;
                }
            }
            tests.expect(initializerCalls == 2 && allThisCallsAreDirectReceivers,
                "delegating constructors begin with a direct same-object constructor-initializer call");
        }
    }

    {
        const auto directory = root / "ThisDelegationCycle";
        writeProject(directory, "ThisDelegationCycle");
        writeFile(directory / "Main.ns",
            "class Loop { public Loop() : this(1) { } public Loop(int value) : this() { } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(directory / "ThisDelegationCycle.nsproj", error);
        tests.expect(!ok, "constructor delegation cycles through this(...) are rejected");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidCall) == 1,
            "a this(...) constructor cycle emits one primary InvalidCall diagnostic");
    }

    {
        const auto directory = root / "ThisDelegationNoApplicable";
        writeProject(directory, "ThisDelegationNoApplicable");
        writeFile(directory / "Main.ns",
            "class Box { public Box() : this(true) { } private Box(int value) { } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectConstructions(directory / "ThisDelegationNoApplicable.nsproj", error);
        tests.expect(!ok, "this(...) rejects same-arity constructor candidates with invalid argument conversions");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidCall) == 1,
            "no-applicable this(...) reports InvalidCall rather than an ambiguity");
    }
}

}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage28_project_construction_tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root);

    testPrimary(tests, root);
    testDuplicateAndDefaultSuppression(tests, root);
    testAccessibility(tests, root);
    testInvalidTargetsAndBodies(tests, root);
    testRanking(tests, root);
    testImplicitBaseConstructorChains(tests, root);
    testThisConstructorDelegation(tests, root);

    std::filesystem::remove_all(root, error);
    return tests.result();
}
