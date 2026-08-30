#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::MethodLambdaCaptureKind;
using nativesharp::frontend::ProjectCompilation;
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

void writeProject(const std::filesystem::path& directory, std::string_view name)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic) { return diagnostic.message().find(text) != std::string::npos; });
}

std::size_t countHirKind(const ProjectOopHirNode* node, ProjectOopHirNodeKind kind)
{
    if (node == nullptr) return 0;
    std::size_t count = node->kind == kind ? 1U : 0U;
    for (const auto& child : node->children) count += countHirKind(child.get(), kind);
    return count;
}

std::size_t countHirKind(const ProjectCompilation& compilation, ProjectOopHirNodeKind kind)
{
    if (compilation.oopHirModel() == nullptr) return 0;
    std::size_t count = 0;
    for (const auto& function : compilation.oopHirModel()->functions())
        count += countHirKind(function.body.get(), kind);
    for (const auto& initializer : compilation.oopHirModel()->fieldInitializers())
        count += countHirKind(initializer.value.get(), kind);
    return count;
}

std::size_t countMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr) return 0;
    std::size_t count = node->kind == kind ? 1U : 0U;
    for (const auto& child : node->children) count += countMirKind(child.get(), kind);
    return count;
}

std::size_t countMirKind(const ProjectCompilation& compilation, ProjectOopMirNodeKind kind)
{
    if (compilation.oopMirModel() == nullptr) return 0;
    std::size_t count = 0;
    for (const auto& function : compilation.oopMirModel()->functions())
        count += countMirKind(function.body.get(), kind);
    for (const auto& initializer : compilation.oopMirModel()->fieldInitializers())
        count += countMirKind(initializer.value.get(), kind);
    return count;
}

void testLexicalContract(TestContext& tests)
{
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    nativesharp::source::SourceManager sources;
    const nativesharp::FileId id = sources.addBuffer("stage43.ns", "x => x + 1; () => 42; (a, b) => a + b;");
    const auto* file = sources.getFile(id);
    nativesharp::lexer::Lexer lexer(*file, diagnostics);
    const auto tokens = lexer.lexAll();
    tests.expect(!diagnostics.hasErrors(), "Stage 43 lambda lexical corpus is valid");
    tests.expect(std::ranges::count_if(tokens, [](const auto& token) {
        return token.kind() == nativesharp::lexer::TokenKind::FatArrow;
    }) == 3, "=> is a dedicated fat-arrow token with maximal munch");
}

void testPositivePipeline(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Positive";
    writeProject(directory, "Stage43Positive");
    writeFile(directory / "Main.ns", R"NS(
namespace Demo
{
    public delegate int Transform(int value);
    public delegate void Changed(int value);

    public class Box
    {
        public int Bias;
        public event Changed ChangedEvent;
        public static Transform Identity = x => x;

        public static int Apply(Transform transform, int value)
        {
            return transform(value);
        }

        public Transform Make(int seed)
        {
            return x => x + seed;
        }

        public int Run(int seed)
        {
            int local = 3;
            Transform noncapturing = x => x + 1;
            Transform captureLocal = x => x + local;
            Transform captureBoth = x => x + local + seed;
            Transform captureThis = x => x + Bias;
            Transform block = x => { int y = x + 2; return y; };
            Transform assigned = null;
            assigned = x => x + 4;
            int viaCall = Apply(x => x + 5, 10);
            ChangedEvent += x => { Bias = x; };
            int result = noncapturing(10) + captureLocal(10) + captureBoth(10) +
                captureThis(10) + block(10) + assigned(10) + viaCall;
            ChangedEvent(7);
            return result + Bias;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage43Positive.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 43 lambda/closure fixture reaches OOP MIR");
    tests.expect(compilation.methodBodyModel() != nullptr, "Stage 43 exposes lambda Sema bindings");
    tests.expect(compilation.oopHirModel() != nullptr, "Stage 43 produces OOP HIR");
    tests.expect(compilation.oopMirModel() != nullptr, "Stage 43 produces OOP MIR");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.oopHirModel() == nullptr ||
        compilation.oopMirModel() == nullptr || compilation.nameLookup() == nullptr) return;

    const auto lambdas = compilation.methodBodyModel()->lambdaBindings();
    tests.expect(lambdas.size() == 10, "all ten target-typed lambdas remain explicit Sema bindings");
    tests.expect(std::ranges::count_if(lambdas, [](const auto& lambda) {
        return lambda.blockBody;
    }) == 2, "block-bodied value and event lambdas remain distinguished");
    tests.expect(std::ranges::count_if(lambdas, [](const auto& lambda) {
        return lambda.captures.empty();
    }) == 5, "noncapturing lambdas have empty closure environments");
    tests.expect(std::ranges::count_if(lambdas, [](const auto& lambda) {
        return !lambda.captures.empty();
    }) == 5, "capturing lambdas retain explicit environment layouts");
    tests.expect(std::ranges::count_if(lambdas, [](const auto& lambda) {
        return std::ranges::any_of(lambda.captures, [](const auto& capture) {
            return capture.kind == MethodLambdaCaptureKind::Local;
        });
    }) == 2, "local captures are discovered from lambda body name bindings");
    tests.expect(std::ranges::count_if(lambdas, [](const auto& lambda) {
        return std::ranges::any_of(lambda.captures, [](const auto& capture) {
            return capture.kind == MethodLambdaCaptureKind::Parameter;
        });
    }) == 2, "outer parameter captures are explicit");
    tests.expect(std::ranges::count_if(lambdas, [](const auto& lambda) {
        return std::ranges::any_of(lambda.captures, [](const auto& capture) {
            return capture.kind == MethodLambdaCaptureKind::ThisReference;
        });
    }) == 2, "implicit instance member access captures this explicitly");
    tests.expect(std::ranges::all_of(lambdas, [](const auto& lambda) {
        return lambda.generatedName.starts_with("__nsc_lambda_");
    }), "each lambda owns a deterministic generated callable name");
    tests.expect(compilation.methodBodyModel()->lambdaValueBindings().size() >= 14,
        "lambda parameters and captures have explicit value bindings");

    std::size_t syntheticHirFunctions = 0;
    std::size_t capturingHirFunctions = 0;
    std::size_t staticHirFunctions = 0;
    for (const auto& function : compilation.oopHirModel()->functions())
    {
        if (!function.isLambda) continue;
        ++syntheticHirFunctions;
        if (function.hasClosureEnvironment) ++capturingHirFunctions;
        else ++staticHirFunctions;
    }
    tests.expect(syntheticHirFunctions == 10, "HIR materializes one synthetic callable per lambda");
    tests.expect(capturingHirFunctions == 5, "HIR marks five callables with closure-env:first ABI");
    tests.expect(staticHirFunctions == 5, "HIR keeps noncapturing lambda callables static");
    tests.expect(countHirKind(compilation, ProjectOopHirNodeKind::LambdaCreate) == 10,
        "HIR preserves ten lambda creation operations");
    tests.expect(countHirKind(compilation, ProjectOopHirNodeKind::ClosureCaptureValue) >= 6,
        "HIR loads captured values through explicit closure-capture nodes");
    tests.expect(countHirKind(compilation, ProjectOopHirNodeKind::EventAdd) == 1,
        "lambda event subscription reuses Stage 42 EventAdd");
    tests.expect(countHirKind(compilation, ProjectOopHirNodeKind::DelegateInvoke) >= 7,
        "lambda-created delegates reuse Stage 42 delegate invocation");

    std::size_t syntheticMirFunctions = 0;
    std::size_t captureSlots = 0;
    for (const auto& function : compilation.oopMirModel()->functions())
    {
        if (!function.isLambda) continue;
        ++syntheticMirFunctions;
        captureSlots += function.captureTypeKeys.size();
    }
    tests.expect(syntheticMirFunctions == 10, "MIR preserves all synthetic lambda callables");
    tests.expect(captureSlots == 6, "MIR records the six snapshot capture slots used by the fixture");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::LambdaCreate) == 10,
        "MIR preserves lambda construction separately from method-group delegate creation");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::ClosureCaptureLoad) >= 6,
        "MIR makes closure environment loads inspectable");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::EventAdd) == 1,
        "MIR lambda event subscription reuses Stage 42 event list semantics");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Stage 43 lambda MIR lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 43 LLVM verifies before optimization");
    const std::string llvm = module->text();
    tests.expect(llvm.find("define i32 @__nsc_lambda_") != std::string::npos,
        "generated lambda functions are real LLVM callables");
    tests.expect(llvm.find("ptr %closure.env") != std::string::npos,
        "capturing lambda callable receives closure environment as hidden first parameter");
    tests.expect(llvm.find("closure.env = call ptr @__nativesharp_alloc(i64 8)") != std::string::npos,
        "single-capture closure allocates one 8-byte environment slot");
    tests.expect(llvm.find("@__nativesharp_alloc(i64 16)") != std::string::npos,
        "multi-capture environment and delegate cells use explicit 16-byte allocations");
    tests.expect(llvm.find("closure.capture.store.addr") != std::string::npos,
        "captured values are snapshotted into closure storage at lambda creation");
    tests.expect(llvm.find("closure.capture.load") != std::string::npos,
        "generated callable reloads captured values from closure storage");
    tests.expect(llvm.find("store ptr null, ptr %lambda.delegate.receiver.addr") != std::string::npos,
        "noncapturing lambda delegate uses null receiver and Stage 42 static ABI");
    tests.expect(llvm.find("store ptr %closure.env, ptr %lambda.delegate.receiver.addr") != std::string::npos,
        "capturing lambda delegate stores environment pointer as Stage 42 receiver");
    tests.expect(llvm.find("delegate.has.receiver = icmp ne ptr") != std::string::npos,
        "Stage 42 invocation still dispatches by receiver nullness");
    tests.expect(llvm.find("delegate.instance.result = call i32") != std::string::npos,
        "capturing closure is invoked through the instance-shaped delegate ABI");
    tests.expect(llvm.find("delegate.static.result = call i32") != std::string::npos,
        "noncapturing lambda is invoked through the static delegate ABI");
    tests.expect(llvm.find("call void @__nativesharp_trap(i32 10)") != std::string::npos,
        "closure/delegate allocation failures retain explicit trap reason 10");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 43 obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError), "Stage 43 applies Win64 data layout");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(*module, *target, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "Stage 43 closure LLVM survives O2");
    tests.expect(module->verify(verifyError), "optimized Stage 43 LLVM remains verifier-clean");
    const auto object = directory / "stage43.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 43 emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 43 COFF passes structural verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 43 COFF machine is AMD64 / 0x8664");
}

void testNegativeContracts(TestContext& tests, const std::filesystem::path& root)
{
    const auto expectMethodReject = [&](std::string_view name, std::string_view source, std::string_view expected) {
        const auto directory = root / std::string(name);
        writeProject(directory, name);
        writeFile(directory / "Main.ns", source);
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / (std::string(name) + ".nsproj"), error);
        tests.expect(!ok, std::string(name) + " is rejected");
        tests.expect(hasDiagnosticText(compilation, expected), std::string(name) + " reports its Stage 43 contract");
    };

    expectMethodReject("LambdaNeedsTarget", R"NS(
class H { public static void F() { int value = x => x; } }
)NS", "requires a target delegate type");

    expectMethodReject("WrongArity", R"NS(
delegate int D(int x, int y);
class H { public static void F() { D value = x => x; } }
)NS", "lambda parameter count does not match target delegate signature");

    expectMethodReject("DuplicateParameters", R"NS(
delegate int D(int x, int y);
class H { public static void F() { D value = (x, x) => x; } }
)NS", "duplicate lambda parameter");

    expectMethodReject("WrongReturn", R"NS(
delegate bool D(int x);
class H { public static void F() { D value = x => x + 1; } }
)NS", "is not implicitly convertible to delegate return type");

    expectMethodReject("VoidExpressionMustBeVoid", R"NS(
delegate void D(int x);
class H { public static void F() { D value = x => x + 1; } }
)NS", "void delegate lambda expression body must have type void");

    expectMethodReject("CapturedLocalIsSnapshot", R"NS(
delegate int D(int x);
class H { public static D F() { int seed = 1; D value = x => { seed = x; return seed; }; return value; } }
)NS", "left side of assignment is not assignable");

    expectMethodReject("NestedLambdaDeferred", R"NS(
delegate int D(int x);
class H { public static D F() { D outer = x => { D inner = y => y; return x; }; return outer; } }
)NS", "nested lambdas are deferred beyond Stage 43");

    expectMethodReject("AmbiguousCallTarget", R"NS(
delegate int A(int x);
delegate int B(int x);
class H
{
    public static int Use(A value) { return 1; }
    public static int Use(B value) { return 2; }
    public static int F() { return Use(x => x); }
}
)NS", "requires a target delegate type");

    const auto directory = root / "LargeStructCapture";
    writeProject(directory, "LargeStructCapture");
    writeFile(directory / "Main.ns", R"NS(
delegate int D(int x);
struct Big { public int A; public int B; public int Third; }
class H
{
    public static D F(Big value) { return x => x + value.A; }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopHir(directory / "LargeStructCapture.nsproj", error);
    tests.expect(!ok, "closure capture of a struct larger than one 8-byte slot is rejected");
    tests.expect(error.find("struct values only when their native size is at most 8 bytes") != std::string::npos,
        "large struct capture reports the explicit Stage 43 environment ABI boundary");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage43_lambdas_closures_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testLexicalContract(tests);
    testPositivePipeline(tests, root);
    testNegativeContracts(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
