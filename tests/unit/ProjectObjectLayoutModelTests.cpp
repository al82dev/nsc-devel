#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModelDumper.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::frontend::ClassObjectLayout;
using nativesharp::frontend::ObjectInitializationStepKind;
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
        count += diagnostic.id() == id ? 1u : 0u;
    }
    return count;
}

const ClassObjectLayout* findLayout(const ProjectCompilation& compilation, std::string_view suffix)
{
    if (compilation.objectLayoutModel() == nullptr)
    {
        return nullptr;
    }
    for (const ClassObjectLayout& layout : compilation.objectLayoutModel()->classLayouts())
    {
        if (layout.typeKey.ends_with(suffix))
        {
            return &layout;
        }
    }
    return nullptr;
}

void testPhysicalLayout(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Physical";
    writeProject(directory, "Physical");
    writeFile(directory / "A.ns",
        "namespace Demo; public struct Pair { short A; int B; } public class Empty {} "
        "public partial class Payload { byte Tag; int Count; Pair PairValue; Empty Child; int[3] Samples; "
        "public Payload() { this.Count = 0; } public Payload Make() { return new Payload(); } }\n");
    writeFile(directory / "B.ns",
        "namespace Demo; public partial class Payload { double Weight; "
        "public Empty MakeEmpty() { return new Empty(); } }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectObjectLayouts(directory / "Physical.nsproj", error);
    tests.expect(ok, "mixed class object layouts and allocation plans build successfully");
    tests.expect(error.empty(), "physical layout fixture has no project-loader error");
    tests.expect(compilation.objectLayoutModel() != nullptr && compilation.constructionModel() != nullptr,
        "Stage 29 preserves construction model and adds object-layout model");
    if (compilation.objectLayoutModel() == nullptr)
    {
        return;
    }

    const auto reference = compilation.objectLayoutModel()->classReferenceLayout();
    tests.expect(reference.size == 8 && reference.alignment == 8,
        "class-reference representation is one Windows x64 pointer");
    tests.expect(compilation.objectLayoutModel()->classLayouts().size() == 2,
        "exactly two class layouts are emitted; struct layout remains an inline dependency");

    const ClassObjectLayout* empty = findLayout(compilation, "Demo.Empty");
    const ClassObjectLayout* payload = findLayout(compilation, "Demo.Payload");
    tests.expect(empty != nullptr && empty->size == 8 && empty->alignment == 8 && empty->headerSize == 8,
        "empty Macro A class consists of the eight-byte object descriptor header");
    tests.expect(payload != nullptr, "payload class layout is available by canonical type identity");
    if (payload == nullptr)
    {
        return;
    }

    tests.expect(payload->size == 56 && payload->alignment == 8,
        "mixed payload includes the Macro A object header and receives expected padded Windows x64 size/alignment");
    tests.expect(payload->referenceSize == 8 && payload->referenceAlignment == 8,
        "object size is distinct from class-reference size");
    tests.expect(payload->fields.size() == 6, "six instance fields participate in physical layout");
    if (payload->fields.size() == 6)
    {
        tests.expect(payload->fields[0].offset == 8 && payload->fields[0].size == 1,
            "byte field starts immediately after the object descriptor header");
        tests.expect(payload->fields[1].offset == 12 && payload->fields[1].size == 4,
            "int field aligns to offset 12 after the header and byte field");
        tests.expect(payload->fields[2].offset == 16 && payload->fields[2].size == 8 && payload->fields[2].alignment == 4,
            "inline Pair struct occupies eight bytes at offset 16");
        tests.expect(payload->fields[3].offset == 24 && payload->fields[3].size == 8 && payload->fields[3].alignment == 8,
            "class-valued field stores one reference pointer at offset 24, not an inline Empty object");
        tests.expect(payload->fields[4].offset == 32 && payload->fields[4].size == 12,
            "fixed int[3] field uses inline array storage after the header-adjusted preceding fields");
        tests.expect(payload->fields[5].offset == 48 && payload->fields[5].size == 8,
            "field from second partial part follows deterministic semantic order and aligns to offset 48");
    }

    tests.expect(compilation.objectLayoutModel()->allocationPlans().size() == 2,
        "one allocation plan is produced for each resolved new C(args) site");
    if (compilation.objectLayoutModel()->allocationPlans().size() == 2)
    {
        const auto& payloadPlan = compilation.objectLayoutModel()->allocationPlans()[0];
        tests.expect(payloadPlan.objectSize == 56 && payloadPlan.objectAlignment == 8,
            "Payload allocation plan consumes the header-inclusive class layout");
        tests.expect(payloadPlan.allocationFailureReturnsNull && payloadPlan.zeroInitializesStorage,
            "allocation contract freezes null-on-failure and zero-initialization before constructor execution");
        tests.expect(payloadPlan.initializationSteps.size() == 7,
            "explicit-constructor allocation plan has seven ordered semantic steps");
        if (payloadPlan.initializationSteps.size() == 7)
        {
            tests.expect(payloadPlan.initializationSteps[0].kind == ObjectInitializationStepKind::RequestStorage &&
                    payloadPlan.initializationSteps[1].kind == ObjectInitializationStepKind::CheckAllocationResult &&
                    payloadPlan.initializationSteps[2].kind == ObjectInitializationStepKind::BeginObjectLifetime,
                "allocation/lifetime steps precede object initialization");
            tests.expect(payloadPlan.initializationSteps[3].kind == ObjectInitializationStepKind::ZeroInitializeStorage &&
                    payloadPlan.initializationSteps[4].kind == ObjectInitializationStepKind::InitializeObjectInfrastructure,
                "default field state and reserved infrastructure initialization precede constructor body");
            tests.expect(payloadPlan.initializationSteps[5].kind == ObjectInitializationStepKind::InvokeConstructor &&
                    payloadPlan.initializationSteps[5].constructorMemberOrdinal.has_value() &&
                    payloadPlan.initializationSteps[6].kind == ObjectInitializationStepKind::ReturnReference,
                "selected explicit constructor runs before returning the class reference");
        }
        const auto& emptyPlan = compilation.objectLayoutModel()->allocationPlans()[1];
        tests.expect(emptyPlan.selectedImplicitDefault && emptyPlan.initializationSteps.size() == 6,
            "implicit default constructor requires no explicit constructor-call step");
    }

    std::ostringstream dump;
    nativesharp::frontend::ProjectObjectLayoutModelDumper().dump(
        *compilation.objectLayoutModel(), *compilation.memberModel(), *compilation.nameLookup(), dump);
    tests.expect(dump.str().find("Payload size=56 align=8 header=8") != std::string::npos &&
            dump.str().find("name=Weight") != std::string::npos &&
            dump.str().find("step=invoke-constructor constructor=Payload()") != std::string::npos,
        "Stage 29 dump exposes layout, cross-part field provenance and constructor-call interface");
}

void testTypeLayoutCoverage(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Coverage";
    writeProject(directory, "Coverage");
    writeFile(directory / "Main.ns",
        "enum Mode { A, B } struct Node { Node* Next; int Value; } "
        "class Ref {} class Holder { bool Flag; Mode ModeValue; Node Root; Ref[2] Refs; int[3][2] Matrix; "
        "Holder Make() { return new Holder(); } }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectObjectLayouts(directory / "Coverage.nsproj", error);
    tests.expect(ok, "enum, self-pointer struct, class-reference array and nested fixed array layouts are supported");
    const ClassObjectLayout* holder = findLayout(compilation, "Holder");
    tests.expect(holder != nullptr && holder->fields.size() == 5,
        "coverage holder receives five physical fields");
    if (holder != nullptr && holder->fields.size() == 5)
    {
        tests.expect(holder->fields[0].size == 1 && holder->fields[1].size == 4,
            "bool and enum use target scalar representations");
        tests.expect(holder->fields[2].size == 16 && holder->fields[2].alignment == 8,
            "self-referential struct through pointer is complete and inline-sized");
        tests.expect(holder->fields[3].size == 16 && holder->fields[3].alignment == 8,
            "array of two class references occupies two pointer slots");
        tests.expect(holder->fields[4].size == 24 && holder->fields[4].alignment == 4,
            "nested int[3][2] modifiers are applied in source order");
    }
}


void testDeterminismAndReferences(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "DeterministicParts";
        writeProject(directory, "DeterministicParts");
        // Deliberately create B before A. Project source discovery must still establish A before B.
        writeFile(directory / "B.ns",
            "partial class Packet { double Z; Packet Make() { return new Packet(); } }\n");
        writeFile(directory / "A.ns",
            "partial class Packet { byte A; int B; }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(directory / "DeterministicParts.nsproj", error);
        tests.expect(ok, "partial-field layout is independent from filesystem creation order");
        const ClassObjectLayout* packet = findLayout(compilation, "Packet");
        tests.expect(packet != nullptr && packet->fields.size() == 3,
            "deterministic partial class exposes all three fields");
        if (packet != nullptr && packet->fields.size() == 3 && compilation.memberModel() != nullptr)
        {
            const auto& members = compilation.memberModel()->members();
            tests.expect(members[packet->fields[0].memberOrdinal].name == "A" &&
                    members[packet->fields[1].memberOrdinal].name == "B" &&
                    members[packet->fields[2].memberOrdinal].name == "Z",
                "field order follows deterministic project source/member order A,B,Z rather than creation order");
            tests.expect(packet->fields[0].offset == 8 && packet->fields[1].offset == 12 &&
                    packet->fields[2].offset == 16 && packet->size == 24,
                "deterministic field order remains stable after the Macro A object header");
        }
    }

    {
        const auto core = root / "Core";
        const auto app = root / "App";
        writeProject(core, "Core");
        writeFile(core / "Types.ns",
            "namespace Shared; public struct Pair { short A; int B; } "
            "public class Large { byte A; double B; }\n");
        writeProject(app, "App", "<ProjectReference Include=\"../Core/Core.nsproj\" />");
        writeFile(app / "Host.ns",
            "using Shared; class Host { Large Ref; Pair Inline; Host Make() { return new Host(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(app / "App.nsproj", error);
        tests.expect(ok, "object layout resolves public class and struct field types across ProjectReference");
        const ClassObjectLayout* large = findLayout(compilation, "Shared.Large");
        const ClassObjectLayout* host = findLayout(compilation, "Host");
        tests.expect(large != nullptr && large->size == 24,
            "referenced Large object has a twenty-four-byte header-inclusive physical representation");
        tests.expect(host != nullptr && host->fields.size() == 2 &&
                host->fields[0].size == 8 && host->fields[1].size == 8,
            "Host stores Large as one reference pointer but referenced Pair inline by value");
        if (host != nullptr && host->fields.size() == 2)
        {
            tests.expect(host->fields[0].offset == 8 && host->fields[1].offset == 16 && host->size == 24,
                "cross-project class-reference and struct-inline fields receive stable header-adjusted offsets");
        }
    }
}

void testInvalidLayouts(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "RecursiveStruct";
        writeProject(directory, "RecursiveStruct");
        writeFile(directory / "Main.ns",
            "struct Node { Node Value; } class Holder { Node Root; Holder Make() { return new Holder(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(directory / "RecursiveStruct.nsproj", error);
        tests.expect(!ok, "recursive struct by value is rejected by physical layout");
        tests.expect(diagnosticCount(compilation, DiagnosticId::IncompleteType) == 1,
            "recursive by-value struct reports IncompleteType exactly once");
    }

    {
        const auto directory = root / "ZeroArray";
        writeProject(directory, "ZeroArray");
        writeFile(directory / "Main.ns", "class Box { int[0] Data; Box Make() { return new Box(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(directory / "ZeroArray.nsproj", error);
        tests.expect(!ok, "zero-length field array is rejected");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidArrayCount) == 1,
            "zero-length field array reports InvalidArrayCount");
    }

    {
        const auto directory = root / "NonConstantArray";
        writeProject(directory, "NonConstantArray");
        writeFile(directory / "Main.ns", "class Box { int[N] Data; Box Make() { return new Box(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(directory / "NonConstantArray.nsproj", error);
        tests.expect(!ok, "unresolved/non-constant project field array count is rejected at layout time");
        tests.expect(diagnosticCount(compilation, DiagnosticId::InvalidArrayCount) == 1,
            "non-constant project field array count reports InvalidArrayCount");
    }

    {
        const auto directory = root / "ExpressionArray";
        writeProject(directory, "ExpressionArray");
        writeFile(directory / "Main.ns", "class Box { int[2 + 3 * 4] Data; Box Make() { return new Box(); } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(directory / "ExpressionArray.nsproj", error);
        tests.expect(ok, "integer constant-expression array count is evaluated structurally at project layout time");
        const ClassObjectLayout* box = findLayout(compilation, "Box");
        tests.expect(box != nullptr && box->fields.size() == 1 && box->fields[0].size == 56,
            "2 + 3 * 4 evaluates to fourteen int elements and fifty-six bytes");
    }
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_project_object_layout_model_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testPhysicalLayout(tests, root);
    testTypeLayoutCoverage(tests, root);
    testDeterminismAndReferences(tests, root);
    testInvalidLayouts(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
