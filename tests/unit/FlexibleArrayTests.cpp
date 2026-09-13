#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
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
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile "
            "Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

bool hasDiagnostic(
    const ProjectCompilation& compilation,
    nativesharp::diagnostics::DiagnosticId id)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.id() == id) return true;
    }
    return false;
}

std::size_t countKind(
    const nativesharp::frontend::ProjectOopMirNode* node,
    nativesharp::frontend::ProjectOopMirNodeKind kind)
{
    if (node == nullptr) return 0;
    std::size_t count = node->kind == kind ? 1 : 0;
    for (const auto& child : node->children) count += countKind(child.get(), kind);
    return count;
}
}

int main()
{
    using nativesharp::diagnostics::DiagnosticId;
    using nativesharp::frontend::ProjectOopMirNodeKind;

    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_flexible_array_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    {
        const auto directory = root / "Canonical";
        writeProject(
            directory,
            "FlexibleCanonical",
            "public struct Packet { public byte Tag; public int[*] Values; } "
            "public class Program { public static int Read(Packet* packet, int index) { "
            "return packet->Values[index]; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(
            directory / "FlexibleCanonical.nsproj", error);
        const auto* layout = compilation.objectLayoutModel() != nullptr
            ? compilation.objectLayoutModel()->structLayoutForType(0) : nullptr;
        tests.expect(ok && error.empty(), "T[*] reaches OOP MIR as a trailing inline array");
        tests.expect(layout != nullptr && layout->size == 4 && layout->alignment == 4,
            "sizeof the host struct ends at the aligned start of its flexible tail");
        tests.expect(layout != nullptr && layout->fields.size() == 2 &&
                layout->fields[1].offset == 4 && layout->fields[1].size == 0 &&
                layout->fields[1].alignment == 4,
            "the tail records its offset and element alignment without adding an element");
        std::size_t rawAddresses = 0;
        if (compilation.oopMirModel() != nullptr)
        {
            for (const auto& function : compilation.oopMirModel()->functions())
                rawAddresses += countKind(function.body.get(), ProjectOopMirNodeKind::RawElementAddress);
        }
        tests.expect(rawAddresses == 1,
            "indexing a flexible tail lowers through raw element address arithmetic");
    }

    {
        const auto directory = root / "NotLast";
        writeProject(directory, "FlexibleNotLast",
            "public struct Bad { public int[*] Values; public int Tail; }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(!compilation.compileProjectObjectLayouts(
                directory / "FlexibleNotLast.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidType),
            "a flexible array must be the final instance field");
    }

    {
        const auto directory = root / "Union";
        writeProject(directory, "FlexibleUnion",
            "public union Bad { public int Tag; public int[*] Values; }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(!compilation.compileProjectObjectLayouts(
                directory / "FlexibleUnion.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidType),
            "a union cannot contain a flexible trailing array");
    }

    {
        const auto directory = root / "Class";
        writeProject(directory, "FlexibleClass",
            "public class Bad { public int[*] Values; }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(!compilation.compileProjectObjectLayouts(
                directory / "FlexibleClass.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidType),
            "a class keeps fixed allocation size and rejects flexible fields");
    }

    {
        const auto directory = root / "OwnedElement";
        writeProject(directory, "FlexibleOwnedElement",
            "public struct Item { public owned string Text; } "
            "public struct Bad { public Item[*] Values; }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(!compilation.compileProjectObjectLayouts(
                directory / "FlexibleOwnedElement.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidType),
            "a flexible tail rejects elements whose unknown count requires destruction");
    }

    {
        const auto directory = root / "Local";
        writeProject(directory, "FlexibleLocal",
            "public class Bad { public static int F() { int[*] values; return 0; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(!compilation.compileProjectMethodBodies(
                directory / "FlexibleLocal.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidType),
            "an incomplete flexible array cannot be declared as a local");
    }

    {
        const auto directory = root / "Signature";
        writeProject(directory, "FlexibleSignature",
            "public class Bad { public int[*] F(int[*] values) { return values; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(!compilation.compileProjectMembers(
                directory / "FlexibleSignature.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidType),
            "flexible storage cannot cross a callable signature by value");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
