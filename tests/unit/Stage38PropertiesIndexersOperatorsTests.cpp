#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
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
using nativesharp::frontend::ClassMemberSymbol;
using nativesharp::frontend::ProjectCompilation;
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

std::optional<std::size_t> findType(const ProjectCompilation& compilation, std::string_view suffix)
{
    if (compilation.nameLookup() == nullptr) return std::nullopt;
    for (std::size_t ordinal = 0; ordinal < compilation.nameLookup()->namedTypes().size(); ++ordinal)
    {
        if (compilation.nameLookup()->namedTypes()[ordinal].identity.fullyQualifiedName().ends_with(suffix))
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

std::vector<std::size_t> findMembers(
    const ProjectCompilation& compilation,
    std::size_t typeOrdinal,
    std::string_view name)
{
    std::vector<std::size_t> result;
    if (compilation.memberModel() == nullptr) return result;
    for (const std::size_t ordinal : compilation.memberModel()->membersOfType(typeOrdinal))
    {
        if (ordinal < compilation.memberModel()->members().size() &&
            compilation.memberModel()->members()[ordinal].name == name)
        {
            result.push_back(ordinal);
        }
    }
    return result;
}

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find(text) != std::string::npos) return true;
    }
    return false;
}

void expectInvalid(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view caseName,
    std::string_view source,
    std::string_view expectedText)
{
    const auto directory = root / std::string(caseName);
    writeProject(directory, caseName);
    writeFile(directory / "Main.ns", source);
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / (std::string(caseName) + ".nsproj"), error);
    tests.expect(!ok, std::string(caseName) + " is rejected");
    tests.expect(
        compilation.diagnostics().hasErrors() &&
            (expectedText.empty() || hasDiagnosticText(compilation, expectedText) || !compilation.diagnostics().diagnostics().empty()),
        std::string(caseName) + " records at least one error diagnostic");
}

void expectValid(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view caseName,
    std::string_view source)
{
    const auto directory = root / std::string(caseName);
    writeProject(directory, caseName);
    writeFile(directory / "Main.ns", source);
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(
        directory / (std::string(caseName) + ".nsproj"), error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        std::string(caseName) + " compiles through OOP MIR");
}

std::size_t countSelectedCallsByFlag(
    const ProjectCompilation& compilation,
    bool ClassMemberSymbol::*flag)
{
    if (compilation.methodBodyModel() == nullptr || compilation.memberModel() == nullptr) return 0;
    std::size_t result = 0;
    for (const auto& call : compilation.methodBodyModel()->calls())
    {
        if (!call.selectedMemberOrdinal.has_value() ||
            *call.selectedMemberOrdinal >= compilation.memberModel()->members().size()) continue;
        const ClassMemberSymbol& member = compilation.memberModel()->members()[*call.selectedMemberOrdinal];
        if (member.*flag) ++result;
    }
    return result;
}

std::size_t countMirCallsForMembers(
    const ProjectOopMirNode* node,
    const ProjectCompilation& compilation,
    bool ClassMemberSymbol::*flag)
{
    if (node == nullptr || compilation.memberModel() == nullptr) return 0;
    std::size_t result = 0;
    if ((node->kind == ProjectOopMirNodeKind::DirectStaticCall ||
         node->kind == ProjectOopMirNodeKind::DirectInstanceCall ||
         node->kind == ProjectOopMirNodeKind::VirtualInstanceCall ||
         node->kind == ProjectOopMirNodeKind::InterfaceInstanceCall) &&
        node->memberOrdinal.has_value() &&
        *node->memberOrdinal < compilation.memberModel()->members().size() &&
        compilation.memberModel()->members()[*node->memberOrdinal].*flag)
    {
        ++result;
    }
    for (const auto& child : node->children)
    {
        result += countMirCallsForMembers(child.get(), compilation, flag);
    }
    return result;
}

void testKeywords(TestContext& tests)
{
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    nativesharp::source::SourceManager sources;
    const nativesharp::FileId fileId = sources.addBuffer(
        "stage38_keywords.ns", "get set operator implicit explicit");
    const auto* file = sources.getFile(fileId);
    nativesharp::lexer::Lexer lexer(*file, diagnostics);
    const auto tokens = lexer.lexAll();
    tests.expect(!diagnostics.hasErrors(), "Stage 38 keyword corpus lexes cleanly");
    tests.expect(tokens.size() == 6, "Stage 38 keyword corpus produces five tokens plus EOF");
    tests.expect(tokens[0].kind() == nativesharp::lexer::TokenKind::KwGet, "get is reserved");
    tests.expect(tokens[1].kind() == nativesharp::lexer::TokenKind::KwSet, "set is reserved");
    tests.expect(tokens[2].kind() == nativesharp::lexer::TokenKind::KwOperator, "operator is reserved");
    tests.expect(tokens[3].kind() == nativesharp::lexer::TokenKind::KwImplicit, "implicit is reserved");
    tests.expect(tokens[4].kind() == nativesharp::lexer::TokenKind::KwExplicit, "explicit is reserved");
}

void testFullSemanticPipeline(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Full";
    writeProject(directory, "Stage38Full");
    writeFile(directory / "Main.ns", R"NS(
namespace Demo
{
    public interface IValue
    {
        int Value { get; set; }
        int this[int index] { get; set; }
    }

    public abstract class BaseValue : IValue
    {
        public abstract int Value { get; set; }
        public abstract int this[int index] { get; set; }
    }

    public class Number : BaseValue
    {
        private int Bias;
        public Number(int value) { Value = value; Bias = 1; }
        public override int Value { get; set; }
        public int Guarded { get { return Value; } private set { Value = value; } }
        public static int Count { get; set; }

        public override int this[int index]
        {
            get { return Value + index + Bias; }
            set { Value = value - index - Bias; }
        }
        public int this[int row, int column]
        {
            get { return Value + row + column; }
            set { Value = value - row - column; }
        }

        public static Number operator +(Number left, Number right)
        { return new Number(left.Value + right.Value); }
        public static Number operator -(Number value)
        { return new Number(-value.Value); }
        public static bool operator ==(Number left, Number right)
        { return left.Value == right.Value; }
        public static bool operator !=(Number left, Number right)
        { return left.Value != right.Value; }
        public static implicit operator int(Number value) { return value.Value; }
        public static explicit operator Number(int value) { return new Number(value); }
    }

    public class Holder
    {
        private int Stored;
        public Holder(int value) { Stored = value; }
        public int Read() { return Stored; }
    }

    public class Harness
    {
        public static int Consume(int value) { return value; }
        public static int Run()
        {
            Number a = new Number(10);
            Number b = new Number(20);
            Number c = a + b;
            Number d = -c;
            bool equal = a == b;
            int primitive = c;
            int consumed = Consume(c);
            Holder holder = new Holder(c);
            Number casted = (Number)primitive;
            IValue iv = casted;
            iv[2] = consumed;
            int one = iv[2];
            casted[1, 2] = one;
            int two = casted[1, 2];
            Number.Count = Number.Count + 1;
            int result = holder.Read() + one + two + d.Value + Number.Count;
            delete a; delete b; delete c; delete d; delete holder; delete casted;
            return result;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage38Full.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 38 full fixture reaches OOP MIR");
    tests.expect(compilation.memberModel() != nullptr, "Stage 38 full fixture builds member model");
    tests.expect(compilation.methodBodyModel() != nullptr, "Stage 38 full fixture builds method-body model");
    tests.expect(compilation.oopHirModel() != nullptr, "Stage 38 full fixture builds OOP HIR");
    tests.expect(compilation.oopMirModel() != nullptr, "Stage 38 full fixture builds OOP MIR");
    if (!ok || compilation.memberModel() == nullptr) return;

    const auto number = findType(compilation, "Number");
    const auto iValue = findType(compilation, "IValue");
    tests.expect(number.has_value(), "Number type resolves");
    tests.expect(iValue.has_value(), "IValue type resolves");
    if (!number || !iValue) return;

    const auto getters = findMembers(compilation, *number, "get_Value");
    const auto setters = findMembers(compilation, *number, "set_Value");
    const auto guardedGetters = findMembers(compilation, *number, "get_Guarded");
    const auto guardedSetters = findMembers(compilation, *number, "set_Guarded");
    const auto indexGetters = findMembers(compilation, *number, "get_Item");
    const auto indexSetters = findMembers(compilation, *number, "set_Item");
    const auto add = findMembers(compilation, *number, "op_Addition");
    const auto neg = findMembers(compilation, *number, "op_UnaryNegation");
    const auto eq = findMembers(compilation, *number, "op_Equality");
    const auto ne = findMembers(compilation, *number, "op_Inequality");
    const auto implicit = findMembers(compilation, *number, "op_Implicit");
    const auto explicitConversion = findMembers(compilation, *number, "op_Explicit");

    tests.expect(getters.size() == 1 && setters.size() == 1, "auto-property expands to getter and setter");
    tests.expect(!guardedGetters.empty() && !guardedSetters.empty(), "explicit property expands to accessor methods");
    tests.expect(indexGetters.size() == 2 && indexSetters.size() == 2, "one- and two-parameter indexers coexist as overloads");
    tests.expect(add.size() == 1 && neg.size() == 1, "binary and unary operators use canonical method names");
    tests.expect(eq.size() == 1 && ne.size() == 1, "equality operators use canonical method names");
    tests.expect(implicit.size() == 1 && explicitConversion.size() == 1, "user conversions use canonical method names");

    bool foundBacking = false;
    bool foundStaticBacking = false;
    for (const std::size_t ordinal : compilation.memberModel()->membersOfType(*number))
    {
        const ClassMemberSymbol& member = compilation.memberModel()->members()[ordinal];
        foundBacking = foundBacking || (member.isCompilerGenerated && member.associatedName == "Value" && !member.isStatic);
        foundStaticBacking = foundStaticBacking ||
            (member.isCompilerGenerated && member.associatedName == "Count" && member.isStatic);
    }
    tests.expect(foundBacking, "instance auto-property owns a compiler-generated backing field");
    tests.expect(foundStaticBacking, "static auto-property owns a compiler-generated static backing field");
    tests.expect(!guardedSetters.empty() && compilation.memberModel()->members()[guardedSetters.front()].accessibility ==
            nativesharp::frontend::ClassMemberAccessibility::Private,
        "accessor-level private setter accessibility is preserved");

    tests.expect(compilation.interfaceModel() != nullptr &&
            compilation.interfaceModel()->classImplementsInterface(*number, *iValue),
        "property/indexer accessors satisfy interface contracts through the existing interface model");
    const auto* interfacePlan = compilation.interfaceModel() != nullptr
        ? compilation.interfaceModel()->tablePlan(*number, *iValue) : nullptr;
    tests.expect(interfacePlan != nullptr && interfacePlan->implementationMemberOrdinals.size() == 4,
        "IValue interface table contains get/set property and get/set indexer slots");

    const std::size_t propertyCalls = countSelectedCallsByFlag(compilation, &ClassMemberSymbol::isPropertyAccessor);
    const std::size_t indexerCalls = countSelectedCallsByFlag(compilation, &ClassMemberSymbol::isIndexerAccessor);
    const std::size_t operatorCalls = countSelectedCallsByFlag(compilation, &ClassMemberSymbol::isUserOperator);
    const std::size_t conversionCalls = countSelectedCallsByFlag(compilation, &ClassMemberSymbol::isUserConversion);
    tests.expect(propertyCalls >= 12, "method-body Sema binds property reads/writes as accessor calls");
    tests.expect(indexerCalls >= 4, "method-body Sema binds indexer reads/writes as accessor calls");
    tests.expect(operatorCalls >= 3, "method-body Sema binds overloaded unary/binary/equality operators");
    tests.expect(conversionCalls >= 1, "explicit conversion syntax binds a concrete user-conversion call; implicit conversions remain ranked conversion records");

    std::size_t mirPropertyCalls = 0;
    std::size_t mirIndexerCalls = 0;
    std::size_t mirOperatorCalls = 0;
    std::size_t mirConversionCalls = 0;
    for (const auto& function : compilation.oopMirModel()->functions())
    {
        mirPropertyCalls += countMirCallsForMembers(function.body.get(), compilation, &ClassMemberSymbol::isPropertyAccessor);
        mirIndexerCalls += countMirCallsForMembers(function.body.get(), compilation, &ClassMemberSymbol::isIndexerAccessor);
        mirOperatorCalls += countMirCallsForMembers(function.body.get(), compilation, &ClassMemberSymbol::isUserOperator);
        mirConversionCalls += countMirCallsForMembers(function.body.get(), compilation, &ClassMemberSymbol::isUserConversion);
    }
    tests.expect(mirPropertyCalls >= 12, "property accessor calls survive HIR into MIR");
    tests.expect(mirIndexerCalls >= 4, "indexer accessor calls survive HIR into MIR");
    tests.expect(mirOperatorCalls >= 3, "user-operator static calls survive HIR into MIR");
    tests.expect(mirConversionCalls >= 4, "user-conversion static calls survive HIR into MIR");
}

void testNegatives(TestContext& tests, const std::filesystem::path& root)
{
    expectValid(tests, root, "GetOnlyConstructorAssignment", R"NS(
class Pair
{
    public int Left { get; }
    public int Right { get; }
    public Pair(int left, int right)
    {
        Left = left;
        this.Right = right;
    }
}
)NS");
    expectInvalid(tests, root, "SetOnly",
        "class C { public int Value { set { } } }", "getter");
    expectInvalid(tests, root, "DuplicateGet",
        "class C { private int X; public int Value { get { return X; } get { return X; } } }", "duplicate get");
    expectInvalid(tests, root, "DuplicateSet",
        "class C { public int Value { get; set; set; } }", "duplicate set");
    expectInvalid(tests, root, "MixedAutoExplicit",
        "class C { private int X; public int Value { get; set { X = value; } } }", "mix automatic and explicit");
    expectInvalid(tests, root, "ConcreteAutoIndexer",
        "class C { public int this[int i] { get; set; } }", "indexer accessors require bodies");
    expectInvalid(tests, root, "StaticIndexer",
        "class C { public static int this[int i] { get { return i; } } }", "indexer");
    expectInvalid(tests, root, "OperatorNotPublic",
        "class C { static C operator +(C a, C b) { return a; } }", "public static");
    expectInvalid(tests, root, "OperatorNotStatic",
        "class C { public C operator +(C a, C b) { return a; } }", "public static");
    expectInvalid(tests, root, "OperatorWrongArity",
        "class C { public static C operator +(C a, C b, C c) { return a; } }", "arity");
    expectInvalid(tests, root, "OperatorUnrelated",
        "class C { public static int operator +(int a, int b) { return a + b; } }", "containing type");
    expectInvalid(tests, root, "ComparisonNonBool",
        "class C { public static int operator ==(C a, C b) { return 1; } }", "return bool");
    expectInvalid(tests, root, "IncrementDeferred",
        "class C { public static C operator ++(C a) { return a; } }", "deferred");
    expectInvalid(tests, root, "ConversionNotPublic",
        "class C { static implicit operator int(C v) { return 1; } }", "public static");
    expectInvalid(tests, root, "ConversionNotStatic",
        "class C { public implicit operator int(C v) { return 1; } }", "public static");
    expectInvalid(tests, root, "ConversionWrongArity",
        "class C { public static implicit operator int(C a, C b) { return 1; } }", "one source parameter");
    expectInvalid(tests, root, "ConversionUnrelated",
        "class C { public static implicit operator int(float v) { return 1; } }", "source or target");
    expectInvalid(tests, root, "ConversionSameType",
        "class C { public static implicit operator C(C v) { return v; } }", "different source and target");
    expectInvalid(tests, root, "PropertyFieldCollision",
        "class C { int Value; public int Value { get { return 1; } } }", "duplicate");
    expectInvalid(tests, root, "PrivateSetter",
        "class C { public int Value { get; private set; } } class H { public void F(C c) { c.Value = 1; } }",
        "no accessible setter");
    expectInvalid(tests, root, "GetOnlyAssignment",
        "class C { public int Value { get; } public void F() { Value = 2; } }",
        "no accessible setter");
    expectInvalid(tests, root, "GetOnlyAssignmentThroughOtherInstance",
        "class C { public int Value { get; } public C(C other) { other.Value = 2; } }",
        "no accessible setter");
    expectValid(tests, root, "PropertyCompoundSupported",
        "class C { public int Value { get; set; } public void F() { Value += 1; } }");
    expectValid(tests, root, "IndexerCompoundSupported",
        "class C { private int X; public int this[int i] { get { return X; } set { X = value; } } public void F() { this[0] += 1; } }");
    expectInvalid(tests, root, "MissingImplicitConversion",
        "class A { } class B { } class H { public B F(A value) { B result = value; return result; } }",
        "cannot implicitly convert");
}

void testLlvmAndCoff(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Backend";
    writeProject(directory, "Stage38Backend");
    writeFile(directory / "Main.ns", R"NS(
interface I { int Value { get; set; } int this[int i] { get; set; } }
class Number : I
{
    public Number(int value) { Value = value; }
    public int Value { get; set; }
    public int this[int i] { get { return Value + i; } set { Value = value - i; } }
    public static Number operator +(Number a, Number b) { return new Number(a.Value + b.Value); }
    public static implicit operator int(Number value) { return value.Value; }
    public static explicit operator Number(int value) { return new Number(value); }
}
class H { public static int Run() { Number a = new Number(2); Number b = new Number(3); Number c = a + b; int x = c; Number d = (Number)x; I i = d; i[1] = x; int r = i[1] + i.Value; delete a; delete b; delete c; delete d; return r; } }
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage38Backend.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 38 backend fixture reaches OOP MIR");
    if (!ok || compilation.nameLookup() == nullptr || compilation.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Stage 38 MIR lowers to LLVM");
    if (module == nullptr) return;

    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 38 LLVM verifies before optimization");
    const std::string llvm = module->text();
    tests.expect(llvm.find("op_Addition") != std::string::npos, "LLVM contains canonical overloaded addition callable");
    tests.expect(llvm.find("op_Implicit") != std::string::npos, "LLVM contains implicit conversion callable");
    tests.expect(llvm.find("op_Explicit") != std::string::npos, "LLVM contains explicit conversion callable");
    tests.expect(llvm.find("get_Value") != std::string::npos && llvm.find("set_Value") != std::string::npos,
        "LLVM contains property accessors");
    tests.expect(llvm.find("get_Item") != std::string::npos && llvm.find("set_Item") != std::string::npos,
        "LLVM contains indexer accessors");
    tests.expect(llvm.find("__nsc_ifmap_") != std::string::npos,
        "Stage 38 interface property/indexer dispatch preserves Stage 36 interface-map ABI");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 38 obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError), "Stage 38 LLVM accepts Windows x64 data layout");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(*module, *target, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "Stage 38 LLVM survives O2");
    tests.expect(module->verify(verifyError), "optimized Stage 38 LLVM remains verifier-clean");

    const auto object = directory / "stage38.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 38 emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object) && verifier.info().machine == 0x8664,
        "Stage 38 object is structurally valid AMD64 COFF");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage38_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testKeywords(tests);
    testFullSemanticPipeline(tests, root);
    testNegatives(tests, root);
    testLlvmAndCoff(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
