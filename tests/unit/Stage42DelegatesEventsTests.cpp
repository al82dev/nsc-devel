#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::MethodDelegateOperationKind;
using nativesharp::frontend::MethodEventOperationKind;
using nativesharp::frontend::NamedTypeDeclarationKind;
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

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic) { return diagnostic.message().find(text) != std::string::npos; });
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
    return count;
}

void testPositivePipeline(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Positive";
    writeProject(directory, "Stage42Positive");
    writeFile(directory / "Main.ns", R"NS(
namespace Demo
{
    public delegate int Transform(int value);
    public delegate void Changed(int value);

    public class Listener
    {
        public int Last;
        public void OnChanged(int value) { Last = value; }
        public int Double(int value) { return value + value; }
    }

    public class Publisher
    {
        public event Changed ChangedEvent;
        public static event Changed StaticChanged;

        public void Raise(int value) { ChangedEvent(value); }
        public static void RaiseStatic(int value) { StaticChanged(value); }
    }

    public class Harness
    {
        public static int Increment(int value) { return value + 1; }

        public static int Run()
        {
            Listener listener = new Listener();
            Publisher publisher = new Publisher();
            Transform first = Increment;
            Transform second = listener.Double;
            Transform empty = null;
            int a = first(10);
            int b = second(20);
            publisher.ChangedEvent += listener.OnChanged;
            Publisher.StaticChanged += listener.OnChanged;
            publisher.Raise(77);
            Publisher.RaiseStatic(88);
            publisher.ChangedEvent -= listener.OnChanged;
            Publisher.StaticChanged -= listener.OnChanged;
            empty = first;
            return a + b + listener.Last;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage42Positive.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 42 delegate/event fixture reaches OOP MIR");
    tests.expect(compilation.nameLookup() != nullptr, "Stage 42 exposes named delegate declarations");
    tests.expect(compilation.methodBodyModel() != nullptr, "Stage 42 exposes delegate/event Sema bindings");
    tests.expect(compilation.oopHirModel() != nullptr, "Stage 42 produces OOP HIR");
    tests.expect(compilation.oopMirModel() != nullptr, "Stage 42 produces OOP MIR");
    if (!ok || compilation.nameLookup() == nullptr || compilation.methodBodyModel() == nullptr ||
        compilation.oopMirModel() == nullptr) return;

    std::size_t delegates = 0;
    for (const auto& type : compilation.nameLookup()->namedTypes())
        if (type.kind == NamedTypeDeclarationKind::Delegate) ++delegates;
    tests.expect(delegates == 2, "delegate declarations are first-class nominal types");

    const auto delegateBindings = compilation.methodBodyModel()->delegateBindings();
    const auto eventBindings = compilation.methodBodyModel()->eventBindings();
    tests.expect(delegateBindings.size() >= 7, "method groups and invocations remain explicit delegate Sema bindings");
    tests.expect(eventBindings.size() == 6, "instance/static event add/remove/invoke operations are explicit Sema bindings");
    tests.expect(std::ranges::count_if(delegateBindings, [](const auto& b) {
        return b.kind == MethodDelegateOperationKind::Create;
    }) >= 5, "static and instance method-group conversions become delegate-create bindings");
    tests.expect(std::ranges::count_if(delegateBindings, [](const auto& b) {
        return b.kind == MethodDelegateOperationKind::Invoke;
    }) == 2, "delegate invocations remain distinct from ordinary calls");
    tests.expect(std::ranges::count_if(eventBindings, [](const auto& b) {
        return b.kind == MethodEventOperationKind::Add;
    }) == 2, "event += is represented for instance and static storage");
    tests.expect(std::ranges::count_if(eventBindings, [](const auto& b) {
        return b.kind == MethodEventOperationKind::Remove;
    }) == 2, "event -= is represented for instance and static storage");
    tests.expect(std::ranges::count_if(eventBindings, [](const auto& b) {
        return b.kind == MethodEventOperationKind::Invoke;
    }) == 2, "event invocation is represented only in declaring methods");

    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::DelegateCreate) >= 5,
        "MIR preserves delegate-cell construction");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::DelegateInvoke) == 2,
        "MIR preserves delegate indirect invocation");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::EventAdd) == 2,
        "MIR preserves event-list prepend operations");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::EventRemove) == 2,
        "MIR preserves event-list removal operations");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::EventInvoke) == 2,
        "MIR preserves event-list traversal/invocation operations");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Stage 42 delegate/event MIR lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 42 LLVM verifies before optimization");
    const std::string llvm = module->text();
    tests.expect(llvm.find("delegate.cell = call ptr @__nativesharp_alloc(i64 16)") != std::string::npos,
        "delegate cell is exactly two native pointers (16 bytes on x64)");
    tests.expect(llvm.find("delegate.receiver.addr = getelementptr i8") != std::string::npos,
        "delegate receiver lives at native pointer offset 8");
    tests.expect(llvm.find("delegate.has.receiver = icmp ne ptr") != std::string::npos,
        "delegate invocation distinguishes static and instance ABI by receiver nullness");
    tests.expect(llvm.find("delegate.static.result = call i32") != std::string::npos,
        "static delegate target uses user arguments only");
    tests.expect(llvm.find("delegate.instance.result = call i32") != std::string::npos,
        "instance delegate target receives hidden receiver first");
    tests.expect(llvm.find("event.node = call ptr @__nativesharp_alloc(i64 16)") != std::string::npos,
        "event subscription allocates a two-pointer list node");
    tests.expect(llvm.find("event.remove.loop") != std::string::npos,
        "event removal lowers to an explicit linked-list walk");
    tests.expect(llvm.find("event.code.equal") != std::string::npos &&
            llvm.find("event.receiver.equal") != std::string::npos,
        "event removal compares delegate semantic identity as code+receiver");
    tests.expect(llvm.find("call void @__nativesharp_free(ptr %event.remove.current)") != std::string::npos,
        "removing a subscription releases its event node");
    tests.expect(llvm.find("event.invoke.loop") != std::string::npos,
        "event invocation traverses all subscribed nodes");
    tests.expect(llvm.find("call void @__nativesharp_trap(i32 10)") != std::string::npos,
        "compiler-generated delegate/event allocations have an explicit allocation-failure trap path");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 42 obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError), "Stage 42 applies Win64 data layout");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(*module, *target, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "Stage 42 delegate/event LLVM survives O2");
    tests.expect(module->verify(verifyError), "optimized Stage 42 LLVM remains verifier-clean");
    const auto object = directory / "stage42.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 42 emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 42 COFF passes structural verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 42 COFF machine is AMD64 / 0x8664");
}

void testNegativeContracts(TestContext& tests, const std::filesystem::path& root)
{
    const auto expectReject = [&](std::string_view name, std::string_view source, std::string_view expected) {
        const auto directory = root / std::string(name);
        writeProject(directory, name);
        writeFile(directory / "Main.ns", source);
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / (std::string(name) + ".nsproj"), error);
        tests.expect(!ok, std::string(name) + " is rejected");
        tests.expect(hasDiagnosticText(compilation, expected), std::string(name) + " reports its Stage 42 contract");
    };

    expectReject("EventNeedsDelegate", R"NS(
class H { public event int Changed; }
)NS", "must have a delegate type");

    expectReject("EventNeedsVoidDelegate", R"NS(
delegate int Query(int x);
class H { public event Query Changed; }
)NS", "void-returning delegate");

    expectReject("DirectEventAssignment", R"NS(
delegate void Changed(int x);
class H { public event Changed E; public void F(Changed h) { E = h; } }
)NS", "support only += and -=");

    expectReject("DirectEventRead", R"NS(
delegate void Changed(int x);
class H { public event Changed E; public Changed F() { return E; } }
)NS", "may only be used with +=, -=, or invocation");

    expectReject("ExternalEventInvoke", R"NS(
delegate void Changed(int x);
class P { public event Changed E; }
class H { public static void F(P p) { p.E(1); } }
)NS", "only be invoked from within their declaring type");

    expectReject("WrongDelegateSignature", R"NS(
delegate int Transform(int x);
class H { public static int F(int x, int y) { return x + y; } public static void G() { Transform t = F; } }
)NS", "exactly matches delegate signature");

    expectReject("DelegateCompound", R"NS(
delegate void D(int x);
class H { public static void F(int x) {} public static void G() { D a = F; D b = F; a += b; } }
)NS", "single-cast values");

    expectReject("NullEventHandler", R"NS(
delegate void D(int x);
class H { public event D E; public void F() { E += null; } }
)NS", "requires a non-null delegate value");

    expectReject("VirtualTargetDeferred", R"NS(
delegate int D(int x);
class B { public virtual int F(int x) { return x; } public D G() { return F; } }
)NS", "non-virtual concrete target");

    expectReject("StructTargetDeferred", R"NS(
delegate int D(int x);
struct S { public int F(int x) { return x; } }
class H { public static D G(S s) { return s.F; } }
)NS", "struct receiver lifetime capture is deferred");
}

void testNullDelegateInvocationTrap(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "NullDelegate";
    writeProject(directory, "NullDelegate");
    writeFile(directory / "Main.ns", R"NS(
delegate int D(int x);
class H
{
    public static int F()
    {
        D value = null;
        return value(1);
    }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "NullDelegate.nsproj", error);
    tests.expect(ok && error.empty(), "null is a valid delegate reference value");
    if (!ok || compilation.nameLookup() == nullptr || compilation.oopMirModel() == nullptr) return;
    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr, "null delegate invocation still lowers with a runtime guard");
    if (module != nullptr)
        tests.expect(module->text().find("call void @__nativesharp_trap(i32 7)") != std::string::npos,
            "invoking a null delegate uses established NullReference trap reason 7");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage42_delegates_events_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testPositivePipeline(tests, root);
    testNegativeContracts(tests, root);
    testNullDelegateInvocationTrap(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
