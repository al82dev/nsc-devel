#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

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

const nativesharp::frontend::ObjectFieldLayout* fieldLayout(
    const ProjectCompilation& compilation,
    std::string_view name)
{
    if (compilation.memberModel() == nullptr || compilation.objectLayoutModel() == nullptr)
        return nullptr;
    const auto& members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        if (members[ordinal].name == name)
            return compilation.objectLayoutModel()->fieldLayoutForMember(ordinal);
    }
    return nullptr;
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
}

int main()
{
    using nativesharp::diagnostics::DiagnosticId;

    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_explicit_field_alignment_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    {
        const auto directory = root / "Struct";
        writeProject(
            directory,
            "ExplicitFieldStruct",
            "public struct Packet { public byte Tag; public align(16) int Value; public byte Tail; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(
            directory / "ExplicitFieldStruct.nsproj", error);
        const auto* value = fieldLayout(compilation, "Value");
        const auto* tail = fieldLayout(compilation, "Tail");
        const auto* layout = compilation.objectLayoutModel() != nullptr
            ? compilation.objectLayoutModel()->structLayoutForType(0) : nullptr;
        tests.expect(ok && error.empty(), "align(N) is accepted on an instance struct field");
        tests.expect(value != nullptr && value->offset == 16 && value->alignment == 16,
            "field alignment raises its placement and recorded access alignment");
        tests.expect(tail != nullptr && tail->offset == 20,
            "the following field starts after the explicitly aligned field value");
        tests.expect(layout != nullptr && layout->size == 32 && layout->alignment == 16,
            "field alignment raises aggregate alignment and trailing padding");
    }

    {
        const auto directory = root / "Packed";
        writeProject(
            directory,
            "ExplicitFieldPacked",
            "public pack(1) struct Packet { public byte Tag; public align(16) int Value; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(
            directory / "ExplicitFieldPacked.nsproj", error);
        const auto* value = fieldLayout(compilation, "Value");
        tests.expect(ok && value != nullptr && value->offset == 16 && value->alignment == 16,
            "an explicit field alignment raises the post-pack natural alignment");
    }

    {
        const auto directory = root / "Class";
        writeProject(
            directory,
            "ExplicitFieldClass",
            "public class Packet { public byte Tag; public align(16) int Value; public byte Tail; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(
            directory / "ExplicitFieldClass.nsproj", error);
        const auto* value = fieldLayout(compilation, "Value");
        const auto* layout = compilation.objectLayoutModel() != nullptr
            ? compilation.objectLayoutModel()->layoutForType(0) : nullptr;
        tests.expect(ok && value != nullptr && value->offset == 16 && value->alignment == 16,
            "class fields honor explicit alignment after the object header");
        tests.expect(layout != nullptr && layout->size == 32 && layout->alignment == 16,
            "an over-aligned class field raises the native object allocation contract");
    }

    {
        const auto directory = root / "MisalignedOffset";
        writeProject(
            directory,
            "ExplicitFieldMisalignedOffset",
            "public struct Bad { public at(2) align(4) int Value; }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(!compilation.compileProjectObjectLayouts(
                directory / "ExplicitFieldMisalignedOffset.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidType),
            "at(N) cannot contradict an explicit field alignment");
    }

    {
        const auto directory = root / "Static";
        writeProject(
            directory,
            "ExplicitStaticFieldAlignment",
            "public class Bad { public static align(16) int Value; }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(!compilation.compileProjectMembers(
                directory / "ExplicitStaticFieldAlignment.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidType),
            "a static global cannot carry an instance-field alignment modifier");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
