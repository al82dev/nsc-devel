#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::MethodCoreOperationKind;
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

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view extraItems = {})
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup>" + std::string(extraItems) +
            "<Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic) { return diagnostic.message().find(text) != std::string::npos; });
}

std::optional<std::size_t> findType(const ProjectCompilation& compilation, std::string_view simpleName)
{
    if (compilation.nameLookup() == nullptr) return std::nullopt;
    for (std::size_t ordinal = 0; ordinal < compilation.nameLookup()->namedTypes().size(); ++ordinal)
    {
        const auto& type = compilation.nameLookup()->namedTypes()[ordinal];
        if (type.identity.name() == simpleName) return ordinal;
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

std::size_t countCoreKind(const ProjectCompilation& compilation, MethodCoreOperationKind kind)
{
    if (compilation.methodBodyModel() == nullptr) return 0;
    return static_cast<std::size_t>(std::ranges::count_if(
        compilation.methodBodyModel()->coreBindings(),
        [kind](const auto& binding) { return binding.kind == kind; }));
}

std::size_t countMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr) return 0;
    std::size_t result = node->kind == kind ? 1U : 0U;
    for (const auto& child : node->children) result += countMirKind(child.get(), kind);
    return result;
}

std::size_t countMirKind(const ProjectCompilation& compilation, ProjectOopMirNodeKind kind)
{
    if (compilation.oopMirModel() == nullptr) return 0;
    std::size_t result = 0;
    for (const auto& function : compilation.oopMirModel()->functions())
    {
        result += countMirKind(function.body.get(), kind);
    }
    return result;
}

const ProjectOopMirNode* findMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr) return nullptr;
    if (node->kind == kind) return node;
    for (const auto& child : node->children)
    {
        if (const ProjectOopMirNode* found = findMirKind(child.get(), kind); found != nullptr) return found;
    }
    return nullptr;
}

void writeCore(const std::filesystem::path& core)
{
    writeProject(core, "NativeSharpCore");
    writeFile(core / "Console.ns", R"NS(
namespace NativeSharp.Core
{
    public class Console
    {
        public static void WriteLine(string value)
        {
        }
    }
}
)NS");
}

void testSemanticModelAndPhysicalRepresentation(TestContext& tests, const std::filesystem::path& root)
{
    const auto core = root / "Core";
    const auto app = root / "App";
    writeCore(core);
    writeProject(app, "Stage41App", "<ProjectReference Include=\"../Core/NativeSharpCore.nsproj\" />");
    writeFile(app / "Main.ns", R"NS(
using NativeSharp.Core;
namespace Demo
{
    public struct Pair
    {
        public int A;
        public int B;
        public void SetA(int value) { A = value; }
    }

    public class Holder
    {
        public int[4] Fixed;
        public int[] Dynamic;
        public string Name;
        public Pair[] Pairs;
    }

    public class Harness
    {
        public static int Run()
        {
            int[] values = new int[4];
            values[0] = 7;
            values[1] = 11;
            Pair[] pairs = new Pair[2];
            pairs[0].SetA(values[0]);
            pairs[0].B = values[1];
            Pair copy = pairs[0];
            string text = "hello";
            int length = text.Length;
            char e = text[1];
            Console.WriteLine(text);
            int answer = values.Length + pairs.Length + copy.A + copy.B + length + e;
            delete[] pairs;
            delete[] values;
            return answer;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(app / "Stage41App.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 41 arrays/strings/Core fixture reaches OOP MIR");
    tests.expect(compilation.methodBodyModel() != nullptr, "Stage 41 exposes method-body core bindings");
    tests.expect(compilation.objectLayoutModel() != nullptr, "Stage 41 retains physical object/value layouts");
    tests.expect(compilation.oopMirModel() != nullptr, "Stage 41 produces OOP MIR");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.objectLayoutModel() == nullptr ||
        compilation.oopMirModel() == nullptr || compilation.memberModel() == nullptr) return;

    tests.expect(countCoreKind(compilation, MethodCoreOperationKind::ArrayAllocation) == 2,
        "two new T[count] expressions become explicit array-allocation bindings");
    tests.expect(countCoreKind(compilation, MethodCoreOperationKind::ArrayLength) == 2,
        "array Length accesses are explicit compiler-core bindings");
    tests.expect(countCoreKind(compilation, MethodCoreOperationKind::StringLiteral) == 1,
        "string literal is retained as a native string binding");
    tests.expect(countCoreKind(compilation, MethodCoreOperationKind::StringLength) == 1,
        "string Length is retained as an intrinsic binding");
    tests.expect(countCoreKind(compilation, MethodCoreOperationKind::StringElement) == 1,
        "string indexing is retained as a bounds-checked intrinsic binding");
    tests.expect(countCoreKind(compilation, MethodCoreOperationKind::ConsoleWriteLine) == 1,
        "NativeSharp.Core.Console.WriteLine resolves as the Stage 41 runtime bridge");
    tests.expect(countCoreKind(compilation, MethodCoreOperationKind::ArrayElement) >= 7,
        "array element accesses remain visible to HIR/MIR instead of decaying to raw pointers");

    const auto holder = findType(compilation, "Holder");
    tests.expect(holder.has_value(), "Holder type resolves");
    if (holder.has_value())
    {
        const auto fixed = findMember(compilation, *holder, "Fixed");
        const auto dynamic = findMember(compilation, *holder, "Dynamic");
        const auto name = findMember(compilation, *holder, "Name");
        const auto pairs = findMember(compilation, *holder, "Pairs");
        tests.expect(fixed.has_value() && dynamic.has_value() && name.has_value() && pairs.has_value(),
            "fixed array, dynamic array, string and struct-array fields coexist");
        if (fixed && dynamic && name && pairs)
        {
            const auto* fixedLayout = compilation.objectLayoutModel()->fieldLayoutForMember(*fixed);
            const auto* dynamicLayout = compilation.objectLayoutModel()->fieldLayoutForMember(*dynamic);
            const auto* nameLayout = compilation.objectLayoutModel()->fieldLayoutForMember(*name);
            const auto* pairsLayout = compilation.objectLayoutModel()->fieldLayoutForMember(*pairs);
            tests.expect(fixedLayout != nullptr && fixedLayout->size == 16 && fixedLayout->alignment == 4,
                "historical int[4] remains a 16-byte inline fixed array");
            tests.expect(dynamicLayout != nullptr && dynamicLayout->size == 8 && dynamicLayout->alignment == 8,
                "int[] is a native reference-sized dynamic array handle");
            tests.expect(nameLayout != nullptr && nameLayout->size == 8 && nameLayout->alignment == 8,
                "string is a native reference-sized immutable handle");
            tests.expect(pairsLayout != nullptr && pairsLayout->size == 8 && pairsLayout->alignment == 8,
                "Pair[] stores a native array reference, not the Pair payload inline in the field");
        }
    }

    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::ArrayAllocate) == 2,
        "MIR has two first-class array allocation operations");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::ArrayElementAddress) >= 4,
        "mutable and struct receiver array accesses preserve element addresses");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::ArrayElementLoad) >= 3,
        "read contexts load array values explicitly");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::StringLiteral) == 1,
        "MIR retains one pooled string literal operation");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::StringLength) == 1,
        "MIR preserves native string length load");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::StringElementLoad) == 1,
        "MIR preserves native string byte indexing");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::CoreConsoleWriteLine) == 1,
        "MIR makes the Console bridge explicit");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::Deallocate) == 2,
        "delete[] becomes direct release of two dynamic array headers");

    const ProjectOopMirNode* intArray = nullptr;
    const ProjectOopMirNode* pairArray = nullptr;
    for (const auto& function : compilation.oopMirModel()->functions())
    {
        const ProjectOopMirNode* candidate = findMirKind(function.body.get(), ProjectOopMirNodeKind::ArrayAllocate);
        if (candidate != nullptr)
        {
            if (candidate->elementTypeKey == "builtin:int") intArray = candidate;
            if (candidate->elementTypeKey.ends_with(".Pair")) pairArray = candidate;
        }
        // The first-only helper above may encounter int[] first; recursively walk children for Pair[] below.
        const auto collect = [&](const auto& self, const ProjectOopMirNode* node) -> void
        {
            if (node == nullptr) return;
            if (node->kind == ProjectOopMirNodeKind::ArrayAllocate)
            {
                if (node->elementTypeKey == "builtin:int") intArray = node;
                if (node->elementTypeKey.ends_with(".Pair")) pairArray = node;
            }
            for (const auto& child : node->children) self(self, child.get());
        };
        collect(collect, function.body.get());
    }
    tests.expect(intArray != nullptr && intArray->immediate == 4 && intArray->alignment == 4,
        "int[] MIR records element size/alignment 4/4");
    tests.expect(pairArray != nullptr && pairArray->immediate == 8 && pairArray->alignment == 4,
        "Pair[] MIR reuses Stage 40 struct layout size/alignment 8/4");
}

void testLlvmBoundsPoolRuntimeAndCoff(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Llvm";
    writeProject(directory, "Stage41Llvm");
    writeFile(directory / "Main.ns", R"NS(
namespace Demo
{
    public struct Pair { public int A; public int B; }
    public class H
    {
        public static int Run(int index)
        {
            Pair[] pairs = new Pair[3];
            pairs[index].A = 17;
            string text = "h\0i";
            int result = pairs.Length + text.Length + text[2] + pairs[index].A;
            delete[] pairs;
            return result;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage41Llvm.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 41 LLVM fixture reaches MIR");
    if (!ok || compilation.nameLookup() == nullptr || compilation.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Stage 41 native arrays/strings lower to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 41 LLVM verifies before optimization");
    const std::string llvm = module->text();
    tests.expect(llvm.find("call ptr @__nativesharp_alloc") != std::string::npos,
        "dynamic array allocation uses the existing native allocator hook");
    tests.expect(llvm.find("call void @llvm.memset.p0.i64") != std::string::npos,
        "dynamic array payload is zero-initialized explicitly");
    tests.expect(llvm.find("store i64 3") != std::string::npos,
        "array header stores the 64-bit physical length metadata");
    tests.expect(llvm.find("icmp ult i64") != std::string::npos,
        "array/string indexing emits unsigned upper-bound checks");
    tests.expect(llvm.find("icmp sge i64") != std::string::npos,
        "signed runtime index emits an explicit non-negative check");
    tests.expect(llvm.find("call void @__nativesharp_trap(i32 4)") != std::string::npos,
        "bounds failure uses the established BoundsViolation trap reason 4");
    tests.expect(llvm.find("getelementptr i8") != std::string::npos,
        "array data addressing is byte-offset based from the native header");
    tests.expect(llvm.find("i64 3, [4 x i8] c\"h\\00i\\00\"") != std::string::npos,
        "string pool preserves embedded NUL using explicit length plus terminal byte");
    tests.expect(llvm.find("native.length64") != std::string::npos &&
            llvm.find("trunc i64") != std::string::npos,
        "source Length is int while physical metadata stays i64");
    tests.expect(llvm.find("call void @__nativesharp_free") != std::string::npos,
        "delete[] releases the array header through the native free hook");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 41 obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 41 applies Windows x64 data layout");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(*module, *target, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "Stage 41 array/string lowering survives LLVM O2");
    tests.expect(module->verify(verifyError), "optimized Stage 41 LLVM remains verifier-clean");
    const auto object = directory / "stage41.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 41 emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 41 object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 41 object machine is AMD64 / 0x8664");
}

void testCoreRuntimeBridge(TestContext& tests)
{
    nativesharp::runtime::WindowsRuntimeBuilder builder;
    std::string error;
    auto runtime = builder.build(error);
    tests.expect(runtime != nullptr && error.empty(), "Windows runtime builds with NativeSharp.Core Console bridge");
    if (runtime == nullptr) return;
    std::string verifyError;
    tests.expect(runtime->verify(verifyError), "Windows runtime remains LLVM-verifier clean");
    const std::string llvm = runtime->text();
    tests.expect(llvm.find("define void @__nativesharp_console_writeline(ptr") != std::string::npos,
        "runtime exports __nativesharp_console_writeline");
    tests.expect(llvm.find("%length64 = load i64") != std::string::npos,
        "Console bridge reads the native string length header");
    tests.expect(llvm.find("getelementptr i8, ptr %0, i64 8") != std::string::npos,
        "Console bridge reads UTF-8 bytes at fixed string data offset 8");
    tests.expect(llvm.find("@WriteFile") != std::string::npos,
        "Console bridge writes through the existing Win32 WriteFile runtime import");
    tests.expect(llvm.find("i32 2") != std::string::npos,
        "Console bridge emits CRLF after exact-length string bytes");
}

void testNullAndNegativeBoundaries(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "NullReferences";
        writeProject(directory, "NullReferences");
        writeFile(directory / "Main.ns", R"NS(
class H
{
    public static int F()
    {
        int[] a = null;
        string s = null;
        int[] b = new int[0];
        delete[] b;
        return 0;
    }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(directory / "NullReferences.nsproj", error);
        tests.expect(ok && error.empty(), "null converts to native array and string references without CLR metadata");
    }

    {
        const auto directory = root / "StringImmutable";
        writeProject(directory, "StringImmutable");
        writeFile(directory / "Main.ns", R"NS(
class H
{
    public static void F()
    {
        string s = "abc";
        s[0] = 'z';
    }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "StringImmutable.nsproj", error);
        tests.expect(!ok, "string element assignment is rejected");
        tests.expect(hasDiagnosticText(compilation, "not assignable"),
            "string immutability is diagnosed at semantic assignment checking");
    }

    {
        const auto directory = root / "LengthImmutable";
        writeProject(directory, "LengthImmutable");
        writeFile(directory / "Main.ns", R"NS(
class H
{
    public static void F()
    {
        int[] a = new int[1];
        a.Length = 4;
    }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "LengthImmutable.nsproj", error);
        tests.expect(!ok, "array Length assignment is rejected");
        tests.expect(hasDiagnosticText(compilation, "not assignable"),
            "Length is a read-only intrinsic property");
    }

    {
        const auto directory = root / "FloatIndex";
        writeProject(directory, "FloatIndex");
        writeFile(directory / "Main.ns", R"NS(
class H
{
    public static int F(int[] a)
    {
        return a[1.5];
    }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "FloatIndex.nsproj", error);
        tests.expect(!ok, "floating array index is rejected statically");
        tests.expect(hasDiagnosticText(compilation, "exactly one integral index"),
            "array index diagnostic names the integral-index contract");
    }

    {
        const auto directory = root / "FloatCount";
        writeProject(directory, "FloatCount");
        writeFile(directory / "Main.ns", R"NS(
class H
{
    public static void F()
    {
        int[] a = new int[2.5];
    }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(directory / "FloatCount.nsproj", error);
        tests.expect(!ok, "floating array allocation count is rejected statically");
        tests.expect(hasDiagnosticText(compilation, "integral element count"),
            "new T[count] diagnostic names the integral-count contract");
    }

    {
        const auto directory = root / "VoidArray";
        writeProject(directory, "VoidArray");
        writeFile(directory / "Main.ns", "class H { public void[] Bad; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectSyntax(directory / "VoidArray.nsproj", error);
        tests.expect(!ok, "void[] is rejected as an object/data type");
        tests.expect(hasDiagnosticText(compilation, "void"), "void[] rejection is diagnostic, not a lowering crash");
    }

    {
        const auto directory = root / "ClassReferenceArray";
        writeProject(directory, "ClassReferenceArray");
        writeFile(directory / "Main.ns", R"NS(
class Payload
{
}

class H
{
    public static int F()
    {
        Payload[] values = new Payload[2];
        int length = values.Length;
        delete[] values;
        return length;
    }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(directory / "ClassReferenceArray.nsproj", error);
        tests.expect(ok && error.empty(), "class-reference T[] is a valid Stage 41 dynamic array");
        if (ok && compilation.oopMirModel() != nullptr)
        {
            const std::string dump = nativesharp::frontend::ProjectOopMirModelDumper::dump(*compilation.oopMirModel());
            tests.expect(dump.find("element=type:ClassReferenceArray::Payload element-size=8 element-align=8") != std::string::npos,
                "class-reference arrays use the native pointer element layout");
        }
    }

    {
        const auto directory = root / "WrongDeleteArray";
        writeProject(directory, "WrongDeleteArray");
        writeFile(directory / "Main.ns", R"NS(
class H
{
    public static void F(int* p)
    {
        delete[] p;
    }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopHir(directory / "WrongDeleteArray.nsproj", error);
        tests.expect(!ok, "delete[] on a raw pointer is outside the Stage 41 dynamic-array ownership contract");
        tests.expect(error.find("delete[] requires a native dynamic array reference") != std::string::npos,
            "wrong delete[] lowering boundary is explicit");
    }
}

void testUtf8CodeUnitContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Utf8";
    writeProject(directory, "Utf8");
    writeFile(directory / "Main.ns", R"NS(
class H
{
    public static int F()
    {
        string s = "é";
        return s.Length;
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Utf8.nsproj", error);
    tests.expect(ok && error.empty(), "UTF-8 string fixture reaches MIR");
    if (!ok || compilation.nameLookup() == nullptr || compilation.oopMirModel() == nullptr) return;
    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr, "UTF-8 literal lowers to native pooled storage");
    if (module != nullptr)
    {
        const std::string llvm = module->text();
        tests.expect(llvm.find("i64 2, [3 x i8]") != std::string::npos,
            "Stage 41 string Length counts UTF-8 code units/bytes, so é has length 2");
    }
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage41_arrays_strings_core_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testSemanticModelAndPhysicalRepresentation(tests, root);
    testLlvmBoundsPoolRuntimeAndCoff(tests, root);
    testCoreRuntimeBridge(tests);
    testNullAndNegativeBoundaries(tests, root);
    testUtf8CodeUnitContract(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
