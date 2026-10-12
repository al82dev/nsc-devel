#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace
{
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::frontend::ClassMemberKind;
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

std::optional<std::size_t> findMember(
    const ProjectCompilation& compilation,
    std::size_t typeOrdinal,
    std::string_view name)
{
    if (compilation.memberModel() == nullptr) return std::nullopt;
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

const ClassMemberSymbol* member(
    const ProjectCompilation& compilation,
    std::size_t typeOrdinal,
    std::string_view name)
{
    const auto ordinal = findMember(compilation, typeOrdinal, name);
    return ordinal.has_value() && compilation.memberModel() != nullptr
        ? &compilation.memberModel()->members()[*ordinal]
        : nullptr;
}

const nativesharp::frontend::ProjectOopMirStaticField* staticField(
    const ProjectCompilation& compilation,
    std::string_view name)
{
    if (compilation.oopMirModel() == nullptr) return nullptr;
    for (const auto& field : compilation.oopMirModel()->staticFields())
    {
        if (field.name == name) return &field;
    }
    return nullptr;
}

std::int64_t signed32(std::uint64_t bits) noexcept
{
    return static_cast<std::int64_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(bits)));
}

std::size_t countKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr) return 0;
    std::size_t result = node->kind == kind ? 1U : 0U;
    for (const auto& child : node->children) result += countKind(child.get(), kind);
    return result;
}

const ProjectOopMirNode* findKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr) return nullptr;
    if (node->kind == kind) return node;
    for (const auto& child : node->children)
    {
        if (const ProjectOopMirNode* found = findKind(child.get(), kind); found != nullptr) return found;
    }
    return nullptr;
}

void testMemberFlagsAndStaticAbi(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "StaticAbi";
    writeProject(directory, "Stage37StaticAbi");
    writeFile(directory / "Main.ns",
        "public class Counter { "
        "public const int Seed = 5; public static int Count = Seed; public static readonly int Limit = 20; "
        "public int Value = 4; public readonly int Id; "
        "public Counter(int id) { Id = id; } "
        "public static int Add(int value) { Count = Count + value; return Count; } "
        "public int Read() { return Value + Id + Counter.Count + Counter.Limit + Counter.Seed; } } "
        "public class Harness { public static int Run() { Counter c = new Counter(7); Counter.Add(3); return c.Read(); } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage37StaticAbi.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 37 static/const/readonly fixture compiles through OOP MIR");
    tests.expect(compilation.memberModel() != nullptr && compilation.objectLayoutModel() != nullptr &&
            compilation.oopHirModel() != nullptr && compilation.oopMirModel() != nullptr,
        "Stage 37 retains semantic, layout, HIR and MIR models");
    if (!ok || compilation.memberModel() == nullptr || compilation.objectLayoutModel() == nullptr ||
        compilation.oopHirModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const auto counter = findType(compilation, "Counter");
    tests.expect(counter.has_value(), "Counter type resolves in Stage 37 member model");
    if (!counter.has_value()) return;

    const ClassMemberSymbol* seed = member(compilation, *counter, "Seed");
    const ClassMemberSymbol* count = member(compilation, *counter, "Count");
    const ClassMemberSymbol* limit = member(compilation, *counter, "Limit");
    const ClassMemberSymbol* value = member(compilation, *counter, "Value");
    const ClassMemberSymbol* id = member(compilation, *counter, "Id");
    const ClassMemberSymbol* add = member(compilation, *counter, "Add");
    tests.expect(seed != nullptr && seed->isConst && seed->isStatic && !seed->isReadonly,
        "const field is represented as immutable static storage");
    tests.expect(count != nullptr && count->isStatic && !count->isConst && !count->isReadonly,
        "mutable static field has its own Stage 37 member flags");
    tests.expect(limit != nullptr && limit->isStatic && limit->isReadonly && !limit->isConst,
        "static readonly remains distinct from const");
    tests.expect(value != nullptr && !value->isStatic && !value->isReadonly && !value->isConst,
        "ordinary instance field remains an instance-layout field");
    tests.expect(id != nullptr && !id->isStatic && id->isReadonly,
        "instance readonly field remains physically part of the object");
    tests.expect(add != nullptr && add->kind == ClassMemberKind::Method && add->isStatic && !add->hasImplicitThis,
        "static method has no implicit this semantic ABI");

    const auto* layout = compilation.objectLayoutModel()->layoutForType(*counter);
    tests.expect(layout != nullptr && layout->fields.size() == 2,
        "static/const/static-readonly fields are excluded from instance object layout");

    const auto addOrdinal = findMember(compilation, *counter, "Add");
    bool staticFunctionAbi = false;
    if (addOrdinal.has_value())
    {
        for (const auto& function : compilation.oopMirModel()->functions())
        {
            if (function.memberOrdinal == *addOrdinal)
            {
                staticFunctionAbi = function.isStatic && function.parameterTypeKeys.size() == 1;
            }
        }
    }
    tests.expect(staticFunctionAbi, "static method MIR carries only source parameters and no hidden this parameter");

    const std::string hirDump = nativesharp::frontend::ProjectOopHirModelDumper::dump(*compilation.oopHirModel());
    const std::string mirDump = nativesharp::frontend::ProjectOopMirModelDumper::dump(*compilation.oopMirModel());
    tests.expect(hirDump.find("abi=static") != std::string::npos,
        "HIR dumper exposes the static callable ABI instead of claiming this:first");
    tests.expect(mirDump.find("abi=(static,p0:builtin:int[win64-direct@0]) -> builtin:int[win64-direct]") !=
            std::string::npos,
        "MIR dumper exposes the static method ABI explicitly");
    tests.expect(mirDump.find("readonly=true volatile=false const=false") != std::string::npos &&
            mirDump.find("const=true constant=true value=5") != std::string::npos,
        "MIR dump exposes readonly, volatile, and compile-time const storage facts");
}

void testConstEvaluation(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "ConstEvaluation";
    writeProject(directory, "Stage37ConstEvaluation");
    writeFile(directory / "Main.ns",
        "class Constants { "
        "public const int Seed = 2 + 3; "
        "public const int Twice = Seed * 2; "
        "public const bool Enabled = (Seed < Twice) && true; "
        "public const int NegDiv = (-5) / 2; "
        "public const int NegMod = (-5) % 2; "
        "public const int WrapDiv = (2147483647 + 1) / 2; "
        "public const int Shift = (-8) >> 1; "
        "public const double Ratio = 5.0 / 2.0; "
        "public const string Product = \"NativeSharp\"; "
        "public const string Alias = Product; }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage37ConstEvaluation.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 37 compile-time constant program materializes in native MIR");
    if (!ok || compilation.oopMirModel() == nullptr) return;

    const auto* seed = staticField(compilation, "Seed");
    const auto* twice = staticField(compilation, "Twice");
    const auto* enabled = staticField(compilation, "Enabled");
    const auto* negDiv = staticField(compilation, "NegDiv");
    const auto* negMod = staticField(compilation, "NegMod");
    const auto* wrapDiv = staticField(compilation, "WrapDiv");
    const auto* shift = staticField(compilation, "Shift");
    const auto* ratio = staticField(compilation, "Ratio");
    const auto* product = staticField(compilation, "Product");
    const auto* alias = staticField(compilation, "Alias");
    tests.expect(seed != nullptr && seed->hasConstantInitializer && seed->constantIntegerValue == 5,
        "integer const arithmetic is evaluated at compile time");
    tests.expect(twice != nullptr && twice->constantIntegerValue == 10,
        "backward const dependency is resolved deterministically");
    tests.expect(enabled != nullptr && enabled->constantBooleanValue,
        "boolean const expression preserves true literal and comparison semantics");
    tests.expect(negDiv != nullptr && signed32(negDiv->constantIntegerValue) == -2,
        "signed const division uses signed 32-bit semantics");
    tests.expect(negMod != nullptr && signed32(negMod->constantIntegerValue) == -1,
        "signed const remainder follows signed native semantics");
    tests.expect(wrapDiv != nullptr && signed32(wrapDiv->constantIntegerValue) == -1073741824,
        "intermediate int const arithmetic wraps to native width before later division");
    tests.expect(shift != nullptr && signed32(shift->constantIntegerValue) == -4,
        "signed right shift in const evaluation preserves arithmetic sign extension");
    tests.expect(ratio != nullptr && ratio->hasConstantInitializer && ratio->constantFloatingValue == 2.5,
        "floating const arithmetic is materialized without a runtime initializer");
    tests.expect(product != nullptr && product->hasConstantInitializer &&
            product->constantStringValue == "NativeSharp",
        "string literal const is preserved as a compile-time native string value");
    tests.expect(alias != nullptr && alias->hasConstantInitializer &&
            alias->constantStringValue == "NativeSharp",
        "string const reference resolves to the same compile-time value");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "const MIR lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError) && verifyError.empty(), "const LLVM module passes verifier");
    const std::string llvm = module->text();
    tests.expect(llvm.find("constant i32 5") != std::string::npos &&
            llvm.find("constant i1 true") != std::string::npos,
        "LLVM emits native constant globals rather than runtime stores for const fields");
    tests.expect(llvm.find("constant ptr @__nsc_str_") != std::string::npos &&
            llvm.find("c\"NativeSharp\\00\"") != std::string::npos,
        "LLVM const string fields point at immutable length-prefixed literal storage");
}

void testReadonlyRules(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "ReadonlyPositive";
        writeProject(directory, "ReadonlyPositive");
        writeFile(directory / "Main.ns",
            "class Box { readonly int X; static readonly int Limit = 5; "
            "public Box(int value) { X = value; this.X = value; } public int Read() { return X + Limit; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(compilation.compileProjectMethodBodies(directory / "ReadonlyPositive.nsproj", error),
            "declaring constructor may assign its own readonly instance field via implicit or explicit this");
    }
    {
        const auto directory = root / "ReadonlyMethod";
        writeProject(directory, "ReadonlyMethod");
        writeFile(directory / "Main.ns", "class Box { readonly int X; public void Change() { X = 1; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ReadonlyMethod.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidAssignmentTarget) >= 1,
            "ordinary method cannot assign an instance readonly field");
    }
    {
        const auto directory = root / "ReadonlyOtherReceiver";
        writeProject(directory, "ReadonlyOtherReceiver");
        writeFile(directory / "Main.ns",
            "class Box { public readonly int X; public Box(Box other) { other.X = 1; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ReadonlyOtherReceiver.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidAssignmentTarget) >= 1,
            "constructor permission does not allow writing readonly fields through another object receiver");
    }
    {
        const auto directory = root / "ReadonlyDerived";
        writeProject(directory, "ReadonlyDerived");
        writeFile(directory / "Main.ns",
            "class Base { protected readonly int X; public Base() { X = 1; } } "
            "class Derived : Base { public Derived() { X = 2; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ReadonlyDerived.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidAssignmentTarget) >= 1,
            "derived constructor cannot assign a readonly field declared by its base type");
    }
    {
        const auto directory = root / "ReadonlyStatic";
        writeProject(directory, "ReadonlyStatic");
        writeFile(directory / "Main.ns",
            "class Box { public static readonly int X = 1; public static void Change() { X = 2; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ReadonlyStatic.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidAssignmentTarget) >= 1,
            "static readonly is immutable after its declaration initializer in the Stage 37 subset");
    }
}

void testStaticAccessAndConstNegatives(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "StaticPositive";
        writeProject(directory, "StaticPositive");
        writeFile(directory / "Main.ns",
            "class Counter { public static int Count = 1; public static int Get() { return Count; } "
            "public int Read() { return Counter.Count + Counter.Get(); } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(compilation.compileProjectOopMir(directory / "StaticPositive.nsproj", error),
            "type-qualified static fields and methods are valid from instance/static contexts");
    }
    {
        const auto directory = root / "StaticThroughObject";
        writeProject(directory, "StaticThroughObject");
        writeFile(directory / "Main.ns",
            "class Counter { public static int Count; public int Bad(Counter other) { return other.Count; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "StaticThroughObject.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::UnknownMember) >= 1,
            "Stage 37 rejects accessing a static field through an object receiver");
    }
    {
        const auto directory = root / "InstanceFromStatic";
        writeProject(directory, "InstanceFromStatic");
        writeFile(directory / "Main.ns",
            "class Counter { int Value; public static int Bad() { return Value; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "InstanceFromStatic.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::UnknownIdentifier) >= 1,
            "static method has no implicit-this access to instance fields");
    }
    {
        const auto directory = root / "ConstCall";
        writeProject(directory, "ConstCall");
        writeFile(directory / "Main.ns",
            "class Constants { static int Make() { return 1; } const int Bad = Make(); }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ConstCall.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOperand) >= 1,
            "const initializer rejects runtime method calls");
    }
    {
        const auto directory = root / "ConstForward";
        writeProject(directory, "ConstForward");
        writeFile(directory / "Main.ns",
            "class Constants { const int Later = Seed + 1; const int Seed = 5; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ConstForward.nsproj", error);
        tests.expect(ok && error.empty(),
            "NSC-LIMIT-019 const dependencies may point forward when the dependency graph is acyclic");
    }
    {
        const auto directory = root / "ConstCycle";
        writeProject(directory, "ConstCycle");
        writeFile(directory / "Main.ns",
            "class Constants { const int A = B + 1; const int B = A + 1; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ConstCycle.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOperand) >= 1,
            "NSC-LIMIT-019 detects real const dependency cycles independently of declaration order");
    }
    {
        const auto directory = root / "ConstAssignment";
        writeProject(directory, "ConstAssignment");
        writeFile(directory / "Main.ns",
            "class Constants { const int Seed = 5; public void Bad() { Seed = 6; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ConstAssignment.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidAssignmentTarget) >= 1,
            "const field is never assignable after declaration");
    }
    {
        const auto directory = root / "ConstDivideByZero";
        writeProject(directory, "ConstDivideByZero");
        writeFile(directory / "Main.ns", "class Constants { const int X = 1 / 0; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ConstDivideByZero.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOperand) >= 1,
            "integer const division by zero is rejected during semantic analysis");
    }
    {
        const auto directory = root / "ConstShiftOutOfRange";
        writeProject(directory, "ConstShiftOutOfRange");
        writeFile(directory / "Main.ns", "class Constants { const int X = 1 << 40; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ConstShiftOutOfRange.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOperand) >= 1,
            "integer const shift count outside the native width is rejected during semantic analysis");
    }
    {
        const auto directory = root / "ConstFloatToIntCast";
        writeProject(directory, "ConstFloatToIntCast");
        writeFile(directory / "Main.ns", "class Constants { const int X = (int)3.5; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "ConstFloatToIntCast.nsproj", error);
        tests.expect(!ok && diagnosticCount(compilation, DiagnosticId::InvalidOperand) >= 1,
            "Stage 37 const evaluation rejects floating-to-integral casts before MIR");
    }
}

void testInitializerOrderingAndTypeInit(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Ordering";
    writeProject(directory, "Stage37Ordering");
    writeFile(directory / "Main.ns",
        "class Root { public int A = 1; } "
        "class Mid : Root { public int B = 2; } "
        "class Leaf : Mid { public int CValue = 3; } "
        "class Delegating { public int X = 4; public Delegating() : this(5) { } "
        "public Delegating(int value) { X = X + value; } } "
        "class Statics { public static int A = 2; public static int B = Init(); "
        "static Statics() { B = B + 1; } public static int Init() { return A * 3; } "
        "public Statics() { } public int Read() { return B; } } "
        "class Harness { public static Leaf MakeLeaf() { return new Leaf(); } "
        "public static Delegating MakeDelegating() { return new Delegating(); } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage37Ordering.nsproj", error);
    tests.expect(ok && error.empty(),
        "inheritance/delegation/static initialization fixture compiles through MIR: " + error);
    if (!ok || compilation.oopMirModel() == nullptr || compilation.memberModel() == nullptr) return;

    const auto leaf = findType(compilation, "Leaf");
    const auto harness = findType(compilation, "Harness");
    const auto delegating = findType(compilation, "Delegating");
    tests.expect(leaf.has_value() && harness.has_value() && delegating.has_value(),
        "ordering fixture resolves leaf, harness and delegating types");

    if (harness.has_value())
    {
        const auto makeLeaf = findMember(compilation, *harness, "MakeLeaf");
        if (makeLeaf.has_value())
        {
            const auto* function = compilation.oopMirModel()->functionForMember(*makeLeaf);
            const ProjectOopMirNode* allocation = function != nullptr
                ? findKind(function->body.get(), ProjectOopMirNodeKind::AllocateAndConstruct)
                : nullptr;
            tests.expect(allocation != nullptr && allocation->argumentCount == 0 && allocation->children.size() == 3,
                "implicit Leaf construction carries exactly Root/Mid/Leaf initializer work after source arguments");
            if (allocation != nullptr && allocation->children.size() == 3)
            {
                bool ordered = true;
                const std::size_t expectedOffsets[] = {8, 16, 24};
                for (std::size_t index = 0; index < 3; ++index)
                {
                    const ProjectOopMirNode* store = allocation->children[index].get();
                    const ProjectOopMirNode* address = store != nullptr && !store->children.empty()
                        ? store->children.front().get()
                        : nullptr;
                    ordered = ordered && store != nullptr && store->kind == ProjectOopMirNodeKind::Store &&
                        address != nullptr && address->immediate == expectedOffsets[index];
                }
                tests.expect(ordered, "implicit constructor initializers execute in base-to-derived physical layout order");
            }
        }
    }

    if (delegating.has_value())
    {
        std::size_t ctorWithInitializer = 0;
        std::size_t ctorWithoutInitializer = 0;
        for (const std::size_t ordinal : compilation.memberModel()->membersOfType(*delegating))
        {
            const ClassMemberSymbol& symbol = compilation.memberModel()->members()[ordinal];
            if (symbol.kind != ClassMemberKind::Constructor) continue;
            const auto* function = compilation.oopMirModel()->functionForMember(ordinal);
            const std::size_t fieldStores = function != nullptr
                ? countKind(function->body.get(), ProjectOopMirNodeKind::FieldAddress)
                : 0;
            if (symbol.parameters.empty()) ctorWithoutInitializer = fieldStores;
            else ctorWithInitializer = fieldStores;
        }
        tests.expect(ctorWithoutInitializer == 0,
            "this(...) delegating constructor does not execute field initializers a second time");
        tests.expect(ctorWithInitializer >= 2,
            "terminal constructor executes the declaration initializer before its body field assignment");
    }

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Stage 37 initializer ordering lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 37 initializer LLVM passes verifier before optimization");
    const std::string llvm = module->text();
    const std::size_t stateClaim = llvm.find("cmpxchg ptr @__nsc_cctor_state_");
    const std::size_t bodyCall = llvm.find("call void @\"__nsc_cctor_body_");
    const std::size_t stateDone = llvm.find("store atomic i8 2", bodyCall);
    tests.expect(llvm.find("private global i8") != std::string::npos &&
            llvm.find("private global i32") != std::string::npos &&
            llvm.find("define private void") != std::string::npos &&
            llvm.find("__nsc_cctor_") != std::string::npos,
        "runtime static initializers keep per-type state and cctor functions private to their module");
    // Stage 54.3 hardened this guard against concurrent first-touch races: the 0 (not
    // started) -> 1 (in progress) claim is now an atomic compare-and-swap rather than a
    // plain store (only one racing thread can ever win the claim), and the final 1/-> 2
    // (done) publication and every guard load remain explicit atomics with sequentially
    // consistent ordering. The observable ordering property this test cares about still
    // holds and is what we check here: the type is atomically claimed for initialization
    // before it is atomically published as done, with the initializer's own field
    // stores/calls happening in between (see the cctor.initialize block above).
    tests.expect(stateClaim != std::string::npos && bodyCall != std::string::npos &&
            stateDone != std::string::npos && stateClaim < bodyCall && bodyCall < stateDone,
        "type initializer atomically claims initialization, executes the explicit body once, then publishes state=2");
    tests.expect(llvm.find("cctor.spin_wait") != std::string::npos &&
            llvm.find("cctor.is_self") != std::string::npos &&
            llvm.find("define void @\"__nsc_cctor_body_") != std::string::npos,
        "losing threads wait while same-thread static-constructor reentrancy bypasses the wait");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 37 obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 37 module accepts Windows x64 triple/data layout");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(
            *module, *target, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "Stage 37 static/initializer module survives LLVM -O2");
    tests.expect(module->verify(verifyError), "optimized Stage 37 LLVM remains verifier-clean");
    const auto object = directory / "stage37.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "optimized Stage 37 module emits a Windows x64 object");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object) && verifier.info().machine == 0x8664,
        "Stage 37 object is structurally valid AMD64 COFF");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage37_static_initialization_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testMemberFlagsAndStaticAbi(tests, root);
    testConstEvaluation(tests, root);
    testReadonlyRules(tests, root);
    testStaticAccessAndConstNegatives(tests, root);
    testInitializerOrderingAndTypeInit(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
