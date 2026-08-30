#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
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
#include <optional>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::ClassMemberKind;
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

std::optional<std::size_t> findType(const ProjectCompilation& compilation, std::string_view simpleName)
{
    if (compilation.nameLookup() == nullptr) return std::nullopt;
    const std::string suffix = "." + std::string(simpleName);
    for (std::size_t i = 0; i < compilation.nameLookup()->namedTypes().size(); ++i)
    {
        const auto& type = compilation.nameLookup()->namedTypes()[i];
        if (type.identity.fullyQualifiedName() == simpleName ||
            type.identity.fullyQualifiedName().ends_with(suffix)) return i;
    }
    return std::nullopt;
}

std::optional<std::size_t> findMember(
    const ProjectCompilation& compilation, std::size_t typeOrdinal, std::string_view name)
{
    if (compilation.memberModel() == nullptr) return std::nullopt;
    for (const std::size_t ordinal : compilation.memberModel()->membersOfType(typeOrdinal))
    {
        if (ordinal < compilation.memberModel()->members().size() &&
            compilation.memberModel()->members()[ordinal].name == name) return ordinal;
    }
    return std::nullopt;
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
    for (const auto& function : compilation.oopMirModel()->functions()) count += countMirKind(function.body.get(), kind);
    return count;
}

void testValueModelAndNestedLayout(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Core";
    writeProject(directory, "Stage40Core");
    writeFile(directory / "Main.ns", R"NS(
namespace Demo
{
    public struct Word
    {
        public short Lo;
        public short Hi;
        public int Sum() { return Lo + Hi; }
    }

    public struct Pair
    {
        public int A;
        public int B;
        public void SetA(int value) { A = value; }
        public int Sum() { return A + B; }
        public static Pair Echo(Pair value) { return value; }
    }

    public struct Nested
    {
        public Word Left;
        public Word Right;
        public int Read() { return Left.Lo; }
    }

    public class Harness
    {
        public static int Run()
        {
            Pair p;
            p.A = 2;
            p.B = 3;
            Pair copy = p;
            copy.SetA(7);
            Pair round = Pair.Echo(copy);
            Nested n;
            n.Left.Lo = 11;
            n.Left.Hi = 13;
            n.Right.Lo = 17;
            n.Right.Hi = 19;
            return p.A + round.Sum() + n.Read();
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage40Core.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 40 core struct fixture reaches OOP MIR");
    tests.expect(compilation.memberModel() != nullptr, "Stage 40 builds a member model containing structs");
    tests.expect(compilation.objectLayoutModel() != nullptr, "Stage 40 builds value layouts");
    tests.expect(compilation.oopMirModel() != nullptr, "Stage 40 builds OOP MIR with struct metadata");
    if (!ok || compilation.memberModel() == nullptr || compilation.objectLayoutModel() == nullptr ||
        compilation.oopMirModel() == nullptr || compilation.nameLookup() == nullptr) return;

    const auto pair = findType(compilation, "Pair");
    const auto word = findType(compilation, "Word");
    const auto nested = findType(compilation, "Nested");
    const auto harness = findType(compilation, "Harness");
    tests.expect(pair.has_value() && word.has_value() && nested.has_value() && harness.has_value(),
        "all Stage 40 named types resolve");
    if (!pair || !word || !nested || !harness) return;

    tests.expect(compilation.nameLookup()->namedTypes()[*pair].kind == NamedTypeDeclarationKind::Struct,
        "Pair remains nominally a struct, not a synthetic class");
    tests.expect(compilation.memberModel()->membersOfType(*pair).size() == 5,
        "Pair exposes two fields and three methods in the project member model");
    for (const auto memberOrdinal : compilation.memberModel()->membersOfType(*pair))
    {
        tests.expect(compilation.memberModel()->members()[memberOrdinal].declaringTypeIsStruct,
            "every Pair member records a struct declaring type");
    }

    const auto* pairLayout = compilation.objectLayoutModel()->structLayoutForType(*pair);
    const auto* wordLayout = compilation.objectLayoutModel()->structLayoutForType(*word);
    const auto* nestedLayout = compilation.objectLayoutModel()->structLayoutForType(*nested);
    const auto* classLayout = compilation.objectLayoutModel()->layoutForType(*harness);
    tests.expect(pairLayout != nullptr && pairLayout->size == 8 && pairLayout->alignment == 4,
        "Pair has inline Windows x64 layout size 8 align 4");
    tests.expect(pairLayout != nullptr && pairLayout->fields.size() == 2 &&
            pairLayout->fields[0].offset == 0 && pairLayout->fields[1].offset == 4,
        "Pair field offsets are 0 and 4");
    tests.expect(wordLayout != nullptr && wordLayout->size == 4 && wordLayout->alignment == 2,
        "Word has inline size 4 align 2");
    tests.expect(nestedLayout != nullptr && nestedLayout->size == 8 && nestedLayout->alignment == 2,
        "Nested lays two Word values inline without object references");
    tests.expect(nestedLayout != nullptr && nestedLayout->fields.size() == 2 &&
            nestedLayout->fields[0].offset == 0 && nestedLayout->fields[1].offset == 4,
        "nested struct field offsets use physical value layout");
    tests.expect(classLayout != nullptr && classLayout->headerSize == 8,
        "class layout still carries its ClassDescriptor header independently of structs");
    tests.expect(compilation.oopMirModel()->structLayouts().size() == 3,
        "MIR retains all three struct value layouts");

    const auto setA = findMember(compilation, *pair, "SetA");
    const auto echo = findMember(compilation, *pair, "Echo");
    tests.expect(setA.has_value() && echo.has_value(), "Pair instance and static callables are present");
    if (setA && echo)
    {
        const auto& setMember = compilation.memberModel()->members()[*setA];
        const auto& echoMember = compilation.memberModel()->members()[*echo];
        tests.expect(setMember.kind == ClassMemberKind::Method && setMember.hasImplicitThis && !setMember.isStatic,
            "struct instance method retains hidden this receiver");
        tests.expect(echoMember.kind == ClassMemberKind::Method && echoMember.isStatic && !echoMember.hasImplicitThis,
            "struct static method has no hidden receiver");
        const auto* setFunction = compilation.oopMirModel()->functionForMember(*setA);
        const auto* echoFunction = compilation.oopMirModel()->functionForMember(*echo);
        tests.expect(setFunction != nullptr && !setFunction->isStatic,
            "MIR marks SetA as an instance callable");
        tests.expect(echoFunction != nullptr && echoFunction->isStatic &&
                echoFunction->parameterTypeKeys.size() == 1 &&
                echoFunction->returnTypeKey == echoMember.declaredTypeKey,
            "MIR preserves Pair-by-value parameter and return identity for Echo");
    }

    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::LocalAddress) >= 5,
        "struct locals become explicit addressable storage when used as receivers/field bases");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::FieldAddress) >= 8,
        "struct mutation and nested access preserve explicit field addresses");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::DirectInstanceCall) >= 3,
        "struct instance calls lower as ordinary direct calls");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::DirectStaticCall) >= 1,
        "struct static call lowers as an ordinary direct static call");
}

void testDirectAbiCarriers(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Abi";
    writeProject(directory, "Stage40Abi");
    writeFile(directory / "Main.ns", R"NS(
namespace Demo
{
    public struct S1 { public byte X; public static S1 Echo(S1 x) { return x; } }
    public struct S2 { public short X; public static S2 Echo(S2 x) { return x; } }
    public struct S4 { public int X; public static S4 Echo(S4 x) { return x; } }
    public struct S8
    {
        public int A;
        public int B;
        public void Mutate(int value) { A = value; }
        public static S8 Echo(S8 x) { x.Mutate(9); return x; }
    }
    public class Harness
    {
        public static S8 RoundTrip(S8 value) { return S8.Echo(value); }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage40Abi.nsproj", error);
    tests.expect(ok && error.empty(), "1/2/4/8-byte struct ABI fixture reaches MIR");
    if (!ok || compilation.nameLookup() == nullptr || compilation.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Stage 40 direct struct ABI lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 40 direct carrier LLVM verifies before optimization");
    const std::string llvm = module->text();
    tests.expect(llvm.find("__Demo_S1__Echo__") != std::string::npos && llvm.find("define i8 @") != std::string::npos,
        "1-byte struct uses an i8 direct carrier");
    tests.expect(llvm.find("__Demo_S2__Echo__") != std::string::npos && llvm.find("define i16 @") != std::string::npos,
        "2-byte struct uses an i16 direct carrier");
    tests.expect(llvm.find("__Demo_S4__Echo__") != std::string::npos && llvm.find("define i32 @") != std::string::npos,
        "4-byte struct uses an i32 direct carrier");
    tests.expect(llvm.find("__Demo_S8__Echo__") != std::string::npos && llvm.find("define i64 @") != std::string::npos,
        "8-byte struct uses an i64 direct carrier");
    tests.expect(llvm.find("__Demo_S8__Mutate__builtin_int(ptr %this, i32 %p0)") != std::string::npos,
        "struct instance receiver ABI is an address-first ptr, not a copied value");
    tests.expect(llvm.find("%p0.addr = alloca i64") != std::string::npos &&
            llvm.find("store i64 %p0, ptr %p0.addr") != std::string::npos,
        "by-value S8 parameter is copied into callee-owned addressable storage");
    tests.expect(llvm.find("classdesc_Stage40Abi__Demo_S8") == std::string::npos,
        "struct values do not receive a ClassDescriptor");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 40 obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError), "Stage 40 applies Windows x64 target layout");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(*module, *target, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "Stage 40 direct struct ABI survives LLVM O2");
    tests.expect(module->verify(verifyError), "optimized Stage 40 LLVM remains verifier-clean");
    const auto object = directory / "stage40.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 40 emits Windows x64 COFF with direct struct values");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 40 object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 40 object machine is AMD64 / 0x8664");
}

void testExplicitBoundaries(TestContext& tests, const std::filesystem::path& root)
{
    const auto expectSyntaxInvalid = [&](std::string_view name, std::string_view source)
    {
        const auto directory = root / std::string(name);
        writeProject(directory, name);
        writeFile(directory / "Main.ns", source);
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectSyntax(directory / (std::string(name) + ".nsproj"), error);
        tests.expect(!ok, std::string(name) + " is rejected at syntax/member-shape boundary");
        tests.expect(hasDiagnosticText(compilation, "Stage 40 structs support fields plus ordinary instance/static methods"),
            std::string(name) + " reports the Stage 40 struct member subset");
    };

    expectSyntaxInvalid("StructConstructor", "struct S { public S() { } }");
    expectSyntaxInvalid("StructProperty", "struct S { public int X { get; set; } }");
    expectSyntaxInvalid("StructVirtual", "struct S { public virtual int F() { return 0; } }");
    expectSyntaxInvalid("StructProtected", "struct S { protected int X; }");

    {
        const auto directory = root / "Recursive";
        writeProject(directory, "Recursive");
        writeFile(directory / "Main.ns", "struct Node { public Node Next; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(directory / "Recursive.nsproj", error);
        tests.expect(!ok, "recursive by-value struct is rejected as physically incomplete");
        tests.expect(hasDiagnosticText(compilation, "recursive by-value struct field"),
            "recursive struct rejection identifies the by-value cycle");
    }

    {
        const auto directory = root / "Large";
        writeProject(directory, "Large");
        writeFile(directory / "Main.ns",
            "struct Large { public int A; public int B; public int Z; } "
            "class H { public static Large Echo(Large value) { return value; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(directory / "Large.nsproj", error);
        tests.expect(ok && error.empty(), "12-byte struct remains semantically/layout-valid through MIR");
        if (ok && compilation.nameLookup() != nullptr && compilation.oopMirModel() != nullptr)
        {
            const auto large = findType(compilation, "Large");
            tests.expect(large.has_value() && compilation.objectLayoutModel() != nullptr &&
                    compilation.objectLayoutModel()->structLayoutForType(*large)->size == 12,
                "large struct layout is computed exactly before ABI classification");
            nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
            auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
            // HISTORY: this used to assert REJECTION ("supports only 1, 2, 4 or 8-byte
            // aggregates") so the documented limitation could not decay into a silent
            // miscompile. The limitation is now the implemented Win64 indirect ABI (clang
            // ground truth): the 12-byte struct lowers with a pointer parameter and an
            // sret([12 x i8]) return, and the direct 1/2/4/8 classification above stays intact.
            tests.expect(module != nullptr && lowerer.error().empty(),
                "12-byte struct now lowers through the Win64 indirect ABI (" + lowerer.error() + ")");
            if (module != nullptr)
            {
                const std::string llvm = module->text();
                tests.expect(llvm.find("sret([12 x i8])") != std::string::npos,
                    "the 12-byte return travels through an sret([12 x i8]) slot");
            }
        }
    }

    {
        const auto directory = root / "TemporaryReceiver";
        writeProject(directory, "TemporaryReceiver");
        writeFile(directory / "Main.ns", R"NS(
struct Pair
{
    public int A;
    public void SetA(int value) { A = value; }
    public static Pair Echo(Pair value) { return value; }
}
class H
{
    public static void F(Pair p) { Pair.Echo(p).SetA(4); }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopHir(directory / "TemporaryReceiver.nsproj", error);
        tests.expect(!ok, "temporary struct instance receiver is rejected in Stage 40");
        tests.expect(error.find("addressable local, parameter or field") != std::string::npos,
            "temporary receiver diagnostic explains the addressability contract");
    }
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage40_struct_value_abi_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testValueModelAndNestedLayout(tests, root);
    testDirectAbiCarriers(tests, root);
    testExplicitBoundaries(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
