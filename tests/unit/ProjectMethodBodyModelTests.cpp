#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModelDumper.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::frontend::MethodCallResolutionKind;
using nativesharp::frontend::MethodNameBindingKind;
using nativesharp::frontend::MethodScopeKind;
using nativesharp::frontend::MethodShadowingKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name)
{
    const std::string text =
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>";
    writeFile(directory / (std::string(name) + ".nsproj"), text);
}

std::size_t diagnosticCount(const ProjectCompilation& compilation, DiagnosticId id)
{
    std::size_t count = 0;
    for (const nativesharp::diagnostics::Diagnostic& diagnostic :
         compilation.diagnostics().diagnostics())
    {
        if (diagnostic.id() == id)
        {
            ++count;
        }
    }
    return count;
}

bool hasBindingKind(
    const nativesharp::frontend::ProjectMethodBodyModel& model,
    MethodNameBindingKind kind,
    std::string_view name = {})
{
    return std::ranges::any_of(
        model.nameBindings(),
        [kind, name](const nativesharp::frontend::MethodNameBinding& binding)
        {
            return binding.kind == kind && (name.empty() || binding.name == name);
        });
}

bool hasShadowingKind(
    const nativesharp::frontend::ProjectMethodBodyModel& model,
    MethodShadowingKind kind)
{
    return std::ranges::any_of(
        model.shadowingEvents(),
        [kind](const nativesharp::frontend::MethodShadowingEvent& event)
        {
            return event.kind == kind;
        });
}

void testPrimaryModel(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "PrimaryModel";
    writeProject(directory, "PrimaryModel");
    writeFile(
        directory / "A.ns",
        "namespace Demo;\n"
        "public partial class Counter {\n"
        "  int Value;\n"
        "  public int Add(int delta) { return this.Value + delta; }\n"
        "  public float Add(float delta) { return 0.0f; }\n"
        "}\n");
    writeFile(
        directory / "B.ns",
        "namespace Demo;\n"
        "public partial class Counter {\n"
        "  public int Run(int Value) {\n"
        "    int total = Add(Value);\n"
        "    { int Value = 1; total = this.Add(Value); }\n"
        "    int answer = Add(total);\n"
        "    return answer;\n"
        "  }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(directory / "PrimaryModel.nsproj", error);
    tests.expect(ok, "primary cross-file method-body model binds successfully"); // 1
    tests.expect(error.empty(), "primary fixture has no project loader error"); // 2
    tests.expect(compilation.nameLookup() != nullptr && compilation.memberModel() != nullptr &&
            compilation.methodBodyModel() != nullptr,
        "method-body compilation preserves prerequisite models"); // 3
    if (compilation.methodBodyModel() == nullptr || compilation.memberModel() == nullptr ||
        compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto& model = *compilation.methodBodyModel();
    tests.expect(model.scopes().size() == 4, "three method roots plus one nested block create four scopes"); // 4
    tests.expect(model.locals().size() == 3, "primary fixture contains three stable local symbols"); // 5
    tests.expect(model.nameBindings().size() == 12, "primary fixture produces deterministic twelve name bindings"); // 6
    tests.expect(model.calls().size() == 3, "primary fixture contains three instance method calls"); // 7
    tests.expect(model.shadowingEvents().size() == 2, "primary fixture records two shadowing events"); // 8
    tests.expect(hasShadowingKind(model, MethodShadowingKind::ParameterShadowsMember),
        "parameter Value shadows the field Value"); // 9
    tests.expect(hasShadowingKind(model, MethodShadowingKind::LocalShadowsParameter),
        "nested local Value shadows parameter Value"); // 10
    tests.expect(std::ranges::all_of(
            model.calls(),
            [](const nativesharp::frontend::MethodCallBinding& call)
            {
                return call.resolution == MethodCallResolutionKind::Resolved &&
                    call.selectedMemberOrdinal.has_value();
            }),
        "all three calls resolve to a concrete overload"); // 11
    tests.expect(hasBindingKind(model, MethodNameBindingKind::ImplicitMethodGroup, "Add"),
        "unqualified Add binds as implicit instance method group"); // 12
    tests.expect(hasBindingKind(model, MethodNameBindingKind::ExplicitThisMethodGroup, "Add"),
        "this.Add binds as explicit-this method group"); // 13

    std::ostringstream dump;
    nativesharp::frontend::ProjectMethodBodyModelDumper().dump(
        model, *compilation.memberModel(), *compilation.nameLookup(), dump);
    tests.expect(dump.str().find("conversion=identity rank=0") != std::string::npos,
        "method-body dump exposes selected argument conversion facts"); // 14

    tests.expect(!model.expressionTypes().empty(),
        "primary fixture records expression type bindings"); // 15
    if (!model.expressionTypes().empty())
    {
        const auto& latest = model.expressionTypes().back();
        tests.expect(model.expressionTypeBinding(latest.syntax, latest.methodMemberOrdinal) == &latest,
            "expression type lookup returns the newest exact callable binding"); // 16
    }
    const auto& latestCall = model.calls().back();
    tests.expect(model.callBinding(latestCall.syntax, latestCall.callerMethodMemberOrdinal) == &latestCall,
        "call lookup returns the newest exact callable binding"); // 17
    const auto& latestName = model.nameBindings().back();
    tests.expect(model.nameBinding(latestName.syntax, latestName.methodMemberOrdinal) == &latestName,
        "name lookup returns the newest exact callable binding"); // 18
}

void testScopeRules(TestContext& tests, const std::filesystem::path& root)
{
    {
        const std::filesystem::path directory = root / "NestedShadow";
        writeProject(directory, "NestedShadow");
        writeFile(directory / "Main.ns",
            "class ScopeBox { int Run(int value) { { int value = 1; return value; } } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "NestedShadow.nsproj", error);
        tests.expect(ok, "nested local may shadow a parameter in a child scope"); // 15
        tests.expect(compilation.methodBodyModel() != nullptr &&
                hasShadowingKind(*compilation.methodBodyModel(), MethodShadowingKind::LocalShadowsParameter),
            "nested parameter shadow is recorded explicitly"); // 16
    }

    {
        const std::filesystem::path directory = root / "RootRedeclaration";
        writeProject(directory, "RootRedeclaration");
        writeFile(directory / "Main.ns",
            "class ScopeBox { int Run(int value) { int value = 1; return value; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "RootRedeclaration.nsproj", error);
        tests.expect(!ok, "outer method block shares root scope with parameters"); // 17
        tests.expect(diagnosticCount(compilation, DiagnosticId::DuplicateSymbol) == 1,
            "parameter/local same-scope redeclaration emits DuplicateSymbol"); // 18
        tests.expect(diagnosticCount(compilation, DiagnosticId::PreviousDeclaration) == 1,
            "same-scope redeclaration points to previous parameter"); // 19
    }

    {
        const std::filesystem::path directory = root / "DuplicateLocals";
        writeProject(directory, "DuplicateLocals");
        writeFile(directory / "Main.ns",
            "class ScopeBox { int Run() { int value = 1; int value = 2; return value; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "DuplicateLocals.nsproj", error);
        tests.expect(!ok, "duplicate locals in one scope are rejected"); // 20
        tests.expect(diagnosticCount(compilation, DiagnosticId::DuplicateSymbol) == 1,
            "duplicate locals emit one DuplicateSymbol"); // 21
    }

    {
        const std::filesystem::path directory = root / "ForScope";
        writeProject(directory, "ForScope");
        writeFile(directory / "Main.ns",
            "class ScopeBox { int Run() { for (int i = 0; i < 1; i = i + 1) { int x = i; } return 0; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ForScope.nsproj", error);
        tests.expect(ok, "for initializer and body bind inside dedicated scopes"); // 22
        tests.expect(compilation.methodBodyModel() != nullptr && std::ranges::any_of(
                compilation.methodBodyModel()->scopes(),
                [](const nativesharp::frontend::MethodScope& scope)
                {
                    return scope.kind == MethodScopeKind::For;
                }),
            "for statement creates a MethodScopeKind::For scope"); // 23
    }

    {
        const std::filesystem::path directory = root / "ForLifetime";
        writeProject(directory, "ForLifetime");
        writeFile(directory / "Main.ns",
            "class ScopeBox { int Run() { for (int i = 0; i < 1; i = i + 1) { } return i; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ForLifetime.nsproj", error);
        tests.expect(!ok, "for initializer local is not visible after the for statement"); // 24
        tests.expect(diagnosticCount(compilation, DiagnosticId::UnknownIdentifier) == 1,
            "use of for local after loop emits UnknownIdentifier"); // 25
    }
}

void testOverloadResolution(TestContext& tests, const std::filesystem::path& root)
{
    {
        const std::filesystem::path directory = root / "PromotionRanking";
        writeProject(directory, "PromotionRanking");
        writeFile(directory / "Main.ns",
            "class Picker { int Pick(int x) { return x; } int Pick(long x) { return 0; } "
            "int Run(char c) { return Pick(c); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "PromotionRanking.nsproj", error);
        tests.expect(ok, "integral promotion overload fixture binds"); // 26
        tests.expect(compilation.methodBodyModel() != nullptr && compilation.methodBodyModel()->calls().size() == 1 &&
                compilation.methodBodyModel()->calls()[0].selectedConversions.size() == 1 &&
                compilation.methodBodyModel()->calls()[0].selectedConversions[0].kind ==
                    nativesharp::frontend::ProjectConversionKind::IntegralPromotion,
            "char chooses Pick(int) by integral-promotion rank"); // 27
    }

    {
        const std::filesystem::path directory = root / "ConstantRanking";
        writeProject(directory, "ConstantRanking");
        writeFile(directory / "Main.ns",
            "class Picker { int Pick(byte x) { return x; } int Pick(long x) { return 0; } "
            "int Run() { return Pick(1); } int RunNegative() { return Pick(-1); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ConstantRanking.nsproj", error);
        tests.expect(ok && compilation.methodBodyModel() != nullptr &&
                compilation.methodBodyModel()->calls().size() == 2,
            "constant-integer overload fixture binds positive and negative calls"); // 28
        tests.expect(compilation.methodBodyModel() != nullptr &&
                compilation.methodBodyModel()->calls().size() == 2 &&
                compilation.methodBodyModel()->calls()[0].selectedConversions.size() == 1 &&
                compilation.methodBodyModel()->calls()[0].selectedConversions[0].kind ==
                    nativesharp::frontend::ProjectConversionKind::ConstantInteger &&
                compilation.methodBodyModel()->calls()[1].selectedConversions.size() == 1 &&
                compilation.methodBodyModel()->calls()[1].selectedConversions[0].kind ==
                    nativesharp::frontend::ProjectConversionKind::Numeric,
            "positive literal narrows when representable while unary-minus call widens to long"); // 29
    }

    {
        const std::filesystem::path directory = root / "SingleAmbiguous";
        writeProject(directory, "SingleAmbiguous");
        writeFile(directory / "Main.ns",
            "class Picker { int Pick(long x) { return 1; } int Pick(ulong x) { return 2; } "
            "int Run(uint x) { return Pick(x); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "SingleAmbiguous.nsproj", error);
        tests.expect(!ok, "equally ranked single-argument overloads are ambiguous"); // 30
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidCall) == 1,
            "ambiguous overload emits InvalidCall"); // 31
        tests.expect(compilation.methodBodyModel() != nullptr && compilation.methodBodyModel()->calls().size() == 1 &&
                compilation.methodBodyModel()->calls()[0].resolution == MethodCallResolutionKind::Ambiguous,
            "ambiguous call remains represented in semantic model"); // 32
    }

    {
        const std::filesystem::path directory = root / "Dominance";
        writeProject(directory, "Dominance");
        writeFile(directory / "Main.ns",
            "class Picker { int Pick(int a, int b) { return 1; } int Pick(long a, long b) { return 2; } "
            "int Run(int a, int b) { return Pick(a, b); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "Dominance.nsproj", error);
        tests.expect(ok, "multiargument strict-dominance fixture binds"); // 33
        tests.expect(compilation.methodBodyModel() != nullptr && compilation.methodBodyModel()->calls().size() == 1 &&
                compilation.methodBodyModel()->calls()[0].selectedConversions.size() == 2 &&
                compilation.methodBodyModel()->calls()[0].selectedConversions[0].rank == 0 &&
                compilation.methodBodyModel()->calls()[0].selectedConversions[1].rank == 0,
            "identity/identity candidate dominates numeric/numeric candidate"); // 34
    }

    {
        const std::filesystem::path directory = root / "TradeoffAmbiguous";
        writeProject(directory, "TradeoffAmbiguous");
        writeFile(directory / "Main.ns",
            "class Picker { int Pick(int a, long b) { return 1; } int Pick(long a, int b) { return 2; } "
            "int Run(int a, int b) { return Pick(a, b); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "TradeoffAmbiguous.nsproj", error);
        tests.expect(!ok, "crossed per-argument conversion tradeoff remains ambiguous"); // 35
        tests.expect(compilation.methodBodyModel() != nullptr && compilation.methodBodyModel()->calls().size() == 1 &&
                compilation.methodBodyModel()->calls()[0].resolution == MethodCallResolutionKind::Ambiguous,
            "dominance algorithm does not sum conversion ranks"); // 36
    }

    {
        const std::filesystem::path directory = root / "NoViable";
        writeProject(directory, "NoViable");
        writeFile(directory / "Main.ns",
            "class Picker { int Pick(int x) { return x; } int Run() { return Pick(true); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "NoViable.nsproj", error);
        tests.expect(!ok, "call with incompatible argument type is rejected"); // 37
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidConversion) == 1,
            "matching arity with no viable conversion emits InvalidConversion"); // 38
    }

    {
        const std::filesystem::path directory = root / "InvalidArity";
        writeProject(directory, "InvalidArity");
        writeFile(directory / "Main.ns",
            "class Picker { int Pick(int x) { return x; } int Run() { return Pick(1, 2); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "InvalidArity.nsproj", error);
        tests.expect(!ok, "call with unsupported arity is rejected"); // 39
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidArgumentCount) == 1,
            "no matching arity emits InvalidArgumentCount"); // 40
    }
}

void testDiagnosticsAndContexts(TestContext& tests, const std::filesystem::path& root)
{
    {
        const std::filesystem::path directory = root / "ShadowedMethod";
        writeProject(directory, "ShadowedMethod");
        writeFile(directory / "Main.ns",
            "class Picker { int Pick(int x) { return x; } int Run() { int Pick = 0; return Pick(1); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ShadowedMethod.nsproj", error);
        tests.expect(!ok, "local name shadows method group before call resolution"); // 41
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidCall) == 1,
            "calling shadowing local emits InvalidCall"); // 42
    }

    {
        const std::filesystem::path directory = root / "InvalidArgumentCascade";
        writeProject(directory, "InvalidArgumentCascade");
        writeFile(directory / "Main.ns",
            "class Picker { int Pick(int x) { return x; } int Run() { return Pick(Missing); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "InvalidArgumentCascade.nsproj", error);
        tests.expect(!ok, "unknown call argument is rejected"); // 43
        tests.expect(diagnosticCount(compilation, DiagnosticId::UnknownIdentifier) == 1 &&
                diagnosticCount(compilation, DiagnosticId::InvalidConversion) == 0,
            "invalid argument suppresses derived overload conversion diagnostic"); // 44
    }

    {
        const std::filesystem::path directory = root / "UnknownCalleeCascade";
        writeProject(directory, "UnknownCalleeCascade");
        writeFile(directory / "Main.ns",
            "class Picker { int Run() { return Missing(1); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "UnknownCalleeCascade.nsproj", error);
        tests.expect(!ok, "unknown callee is rejected"); // 45
        tests.expect(diagnosticCount(compilation, DiagnosticId::UnknownIdentifier) == 1 &&
                diagnosticCount(compilation, DiagnosticId::InvalidCall) == 0,
            "unknown callee suppresses generic secondary InvalidCall"); // 46
    }

    {
        const std::filesystem::path directory = root / "ValidWidening";
        writeProject(directory, "ValidWidening");
        writeFile(directory / "Main.ns",
            "class Box { double Run() { int x = 1; double d = x; return d; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ValidWidening.nsproj", error);
        tests.expect(ok, "local initializer and return allow valid numeric widening"); // 47
    }

    {
        const std::filesystem::path directory = root / "InvalidInitializer";
        writeProject(directory, "InvalidInitializer");
        writeFile(directory / "Main.ns",
            "class Box { float Run() { float x = 1.0; return x; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "InvalidInitializer.nsproj", error);
        tests.expect(!ok, "double literal cannot initialize float implicitly"); // 48
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidConversion) == 1,
            "invalid local initializer emits InvalidConversion"); // 49
    }

    {
        const std::filesystem::path directory = root / "InvalidCondition";
        writeProject(directory, "InvalidCondition");
        writeFile(directory / "Main.ns",
            "class Box { int Run() { if (1) { return 1; } return 0; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "InvalidCondition.nsproj", error);
        tests.expect(!ok, "integer condition is rejected in instance method body"); // 50
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidCondition) == 1,
            "non-bool condition emits InvalidCondition"); // 51
    }

    {
        const std::filesystem::path directory = root / "InvalidReturn";
        writeProject(directory, "InvalidReturn");
        writeFile(directory / "Main.ns",
            "class Box { float Run() { return 1.0; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "InvalidReturn.nsproj", error);
        tests.expect(!ok, "double return expression cannot implicitly narrow to float"); // 52
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidConversion) == 1,
            "invalid return conversion emits InvalidConversion"); // 53
    }

    {
        const std::filesystem::path directory = root / "ThisBypassesShadowing";
        writeProject(directory, "ThisBypassesShadowing");
        writeFile(directory / "Main.ns",
            "class Box { int Value; int Run(int Value) { return this.Value + Value; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ThisBypassesShadowing.nsproj", error);
        tests.expect(ok && compilation.methodBodyModel() != nullptr &&
                hasBindingKind(*compilation.methodBodyModel(), MethodNameBindingKind::ExplicitThisField, "Value") &&
                hasBindingKind(*compilation.methodBodyModel(), MethodNameBindingKind::Parameter, "Value"),
            "explicit this.Value bypasses lexical parameter shadowing"); // 54
    }

    {
        const std::filesystem::path directory = root / "NullClassReference";
        writeProject(directory, "NullClassReference");
        writeFile(directory / "Main.ns",
            "class Node { int Use(Node value) { return 0; } Node Maybe() { Node local = null; "
            "int ignored = Use(null); return null; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "NullClassReference.nsproj", error);
        tests.expect(ok,
            "null converts to class references for local initialization, method arguments, and returns"); // 55
    }
}

void testLongLogicalChains(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "LongLogicalChains";
    writeProject(directory, "LongLogicalChains");

    std::string conjunction = "value";
    std::string disjunction = "value";
    for (std::size_t index = 0; index < 128; ++index)
    {
        conjunction += " && value";
        disjunction += " || value";
    }
    writeFile(
        directory / "Main.ns",
        "class Chain { bool All(bool value) { return " + conjunction +
            "; } bool Any(bool value) { return " + disjunction + "; } }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "LongLogicalChains.nsproj", error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "long logical chains bind without exhausting the host stack");
}

void testDeepNestedValueExpression(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "DeepNestedValueExpression";
    writeProject(directory, "DeepNestedValueExpression");

    std::string expression = "value";
    for (std::size_t index = 0; index < 40; ++index)
    {
        expression = "Permute(" + expression + " + 1.0f)";
    }
    writeFile(
        directory / "Main.ns",
        "class NestedValue {\n"
        "  static float Permute(float value) { return value; }\n"
        "  static float Evaluate(float value) { return " + expression + "; }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "DeepNestedValueExpression.nsproj", error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "deep nested call and binary expressions bind through the heap-backed worklist");
}

void testQualifiedStaticTypeRoots(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path alpha = root / "QualifiedAlpha";
    const std::filesystem::path beta = root / "QualifiedBeta";
    const std::filesystem::path app = root / "QualifiedApp";
    writeProject(alpha, "QualifiedAlpha");
    writeProject(beta, "QualifiedBeta");
    writeFile(
        alpha / "Api.ns",
        "namespace SharedAlpha {\n"
        "  public struct Color {\n"
        "    public static int Number = 7;\n"
        "    public static int Value() { return 5; }\n"
        "    public static int Code { get { return 8; } }\n"
        "  }\n"
        "  public enum Mode { C = 6 }\n"
        "}\n");
    writeFile(
        beta / "Api.ns",
        "namespace SharedBeta {\n"
        "  public class Color { public static int Value() { return 6; } }\n"
        "}\n");
    writeFile(
        app / "QualifiedApp.nsproj",
        "<Project><PropertyGroup><Name>QualifiedApp</Name></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" />"
        "<ProjectReference Include=\"../QualifiedAlpha/QualifiedAlpha.nsproj\" />"
        "<ProjectReference Include=\"../QualifiedBeta/QualifiedBeta.nsproj\" />"
        "</ItemGroup></Project>");
    writeFile(
        app / "Main.ns",
        "using AlphaColor = SharedAlpha.Color;\n"
        "using A = SharedAlpha;\n"
        "namespace QualifiedApp {\n"
        "  public class Child { public int Value() { return 1; } }\n"
        "  public class Holder { public Child Child; }\n"
        "  public class Transform { public int Position; }\n"
        "  public class Camera {\n"
        "    public Transform Transform;\n"
        "    public int Read() { return Transform.Position; }\n"
        "  }\n"
        "  public class Program {\n"
        "    public static int Instance(Holder object) { return object.Child.Value(); }\n"
        "    public static int Main() {\n"
        "      return SharedAlpha.Color.Value() + SharedBeta.Color.Value() +\n"
        "        AlphaColor.Value() + A.Color.Value() + SharedAlpha.Color.Number +\n"
        "        SharedAlpha.Color.Code + (int)SharedAlpha.Mode.C;\n"
        "    }\n"
        "  }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(app / "QualifiedApp.nsproj", error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "qualified types, namespace aliases, and type aliases bind as static receivers");
    tests.expect(compilation.methodBodyModel() != nullptr &&
            hasBindingKind(*compilation.methodBodyModel(), MethodNameBindingKind::TypeReference,
                "SharedAlpha.Color") &&
            hasBindingKind(*compilation.methodBodyModel(), MethodNameBindingKind::TypeReference,
                "AlphaColor") &&
            hasBindingKind(*compilation.methodBodyModel(), MethodNameBindingKind::TypeReference,
                "A.Color") &&
            hasBindingKind(*compilation.methodBodyModel(), MethodNameBindingKind::ExplicitObjectMethodGroup,
                "Value"),
        "qualified static roots do not change an ordinary object.Child.Value() access");

    const std::filesystem::path ambiguous = root / "QualifiedAmbiguous";
    writeFile(
        ambiguous / "QualifiedAmbiguous.nsproj",
        "<Project><PropertyGroup><Name>QualifiedAmbiguous</Name></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" />"
        "<ProjectReference Include=\"../QualifiedAlpha/QualifiedAlpha.nsproj\" />"
        "<ProjectReference Include=\"../QualifiedBeta/QualifiedBeta.nsproj\" />"
        "</ItemGroup></Project>");
    writeFile(
        ambiguous / "Main.ns",
        "using SharedAlpha; using SharedBeta; namespace QualifiedAmbiguous;\n"
        "public class Program { public static int Main() { return Color.Value(); } }\n");

    ProjectCompilation ambiguousCompilation;
    error.clear();
    const bool ambiguousOk = ambiguousCompilation.compileProjectMethodBodies(
        ambiguous / "QualifiedAmbiguous.nsproj", error);
    tests.expect(!ambiguousOk &&
            diagnosticCount(ambiguousCompilation, DiagnosticId::AmbiguousTypeName) == 1 &&
            diagnosticCount(ambiguousCompilation, DiagnosticId::UnknownIdentifier) == 0,
        "an unqualified static receiver from two imported namespaces reports deterministic ambiguity");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage27_project_method_body_tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root);

    testPrimaryModel(tests, root);
    testScopeRules(tests, root);
    testOverloadResolution(tests, root);
    testDiagnosticsAndContexts(tests, root);
    testLongLogicalChains(tests, root);
    testDeepNestedValueExpression(tests, root);
    testQualifiedStaticTypeRoots(tests, root);

    std::filesystem::remove_all(root, error);
    return tests.result();
}
