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
        std::filesystem::temp_directory_path() / "nativesharp_readonly_struct_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    {
        const auto directory = root / "Positive";
        writeProject(
            directory,
            "ReadonlyPositive",
            "public readonly struct Point { "
            "public readonly int X; public readonly int Y; public static int Created; "
            "public Point(int x, int y) { X = x; this.Y = y; } "
            "public int Sum() { return X + Y; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            compilation.compileProjectMethodBodies(directory / "ReadonlyPositive.nsproj", error),
            "readonly struct fields can be initialized by their declaring constructor");
        tests.expect(error.empty(), "readonly struct acceptance does not use a loader error");
    }

    {
        const auto directory = root / "MutableField";
        writeProject(
            directory,
            "ReadonlyMutableField",
            "public readonly struct Bad { public int Value; }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectMembers(directory / "ReadonlyMutableField.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidType),
            "every instance field of a readonly struct must be readonly");
    }

    {
        const auto directory = root / "Mutation";
        writeProject(
            directory,
            "ReadonlyMutation",
            "public readonly struct Bad { public readonly int Value; "
            "public Bad(int value) { Value = value; } "
            "public void Change() { Value = 2; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectMethodBodies(directory / "ReadonlyMutation.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidAssignmentTarget),
            "readonly struct fields cannot be assigned after construction");
    }

    for (const auto& invalid : {
             std::pair{"ReadonlyClass", "public readonly class Bad { }"},
             std::pair{"ReadonlyUnion", "public readonly union Bad { public readonly int Value; }"}})
    {
        const auto directory = root / invalid.first;
        writeProject(directory, invalid.first, invalid.second);
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectMembers(directory / (std::string(invalid.first) + ".nsproj"), error) &&
                hasDiagnostic(compilation, DiagnosticId::ExpectedDeclaration),
            std::string(invalid.first) + " rejects readonly outside a non-union struct");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
