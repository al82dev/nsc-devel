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
        std::filesystem::temp_directory_path() / "nativesharp_offsetof_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    {
        const auto directory = root / "Canonical";
        writeProject(
            directory,
            "OffsetofCanonical",
            "public struct Header { public byte Tag; public long Payload; } "
            "public class Program { public static int Main() { "
            "static_assert(offsetof(Header, Tag) == 0u); "
            "static_assert(offsetof(Header, Payload) == 8u, \"Header.Payload ABI\"); "
            "return (int)offsetof(Header, Payload); } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            compilation.compileProjectOopMir(
                directory / "OffsetofCanonical.nsproj", error) && error.empty(),
            "project OOP pipeline folds offsetof from the canonical struct layout");
    }

    {
        const auto directory = root / "Generic";
        writeProject(
            directory,
            "OffsetofGeneric",
            "public struct Box<T> { public byte Tag; public T Value; } "
            "public class Program { public static int Main() { "
            "static_assert(offsetof(Box<long>, Value) == 8u); return 0; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            compilation.compileProjectOopMir(
                directory / "OffsetofGeneric.nsproj", error) && error.empty(),
            "offsetof resolves the field of a closed generic struct specialization");
    }

    {
        const auto directory = root / "Missing";
        writeProject(
            directory,
            "OffsetofMissing",
            "public struct Header { public int Value; } "
            "public class Program { public static int Main() { "
            "return (int)offsetof(Header, Missing); } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "OffsetofMissing.nsproj", error);
        tests.expect(
            !ok && hasDiagnostic(compilation, DiagnosticId::InvalidOffsetof),
            "project semantic model rejects an unknown offsetof field");
    }

    {
        const auto directory = root / "Class";
        writeProject(
            directory,
            "OffsetofClass",
            "public class ReferenceType { public int Value; } "
            "public class Program { public static int Main() { "
            "return (int)offsetof(ReferenceType, Value); } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "OffsetofClass.nsproj", error);
        tests.expect(
            !ok && hasDiagnostic(compilation, DiagnosticId::InvalidOffsetof),
            "offsetof rejects reference-object layout and accepts value aggregates only");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
