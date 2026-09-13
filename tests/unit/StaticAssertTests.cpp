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
        std::filesystem::temp_directory_path() / "nativesharp_static_assert_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    {
        const auto directory = root / "Positive";
        writeProject(
            directory,
            "StaticAssertPositive",
            "public struct Pair { public int A; public byte B; } "
            "public class Program { public static int Main() { "
            "static_assert(sizeof(int) == 4u, \"int ABI\"); "
            "static_assert(sizeof(Pair) == 8u && alignof(Pair) == 4u, \"Pair ABI\"); "
            "static_assert((2 + 3) * 4 == 20); return 42; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            compilation.compileProjectOopMir(
                directory / "StaticAssertPositive.nsproj", error) && error.empty(),
            "project OOP pipeline accepts true static assertions, including canonical layout queries");
    }

    {
        const auto directory = root / "False";
        writeProject(
            directory,
            "StaticAssertFalse",
            "public class Program { public static int Main() { "
            "static_assert(sizeof(int) == 8u, \"expected wrong int width\"); return 0; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopHir(
            directory / "StaticAssertFalse.nsproj", error);
        tests.expect(
            !ok && error.find("static assertion failed: expected wrong int width") != std::string::npos,
            "project OOP pipeline rejects a false assertion and preserves its message");
    }

    {
        const auto directory = root / "Runtime";
        writeProject(
            directory,
            "StaticAssertRuntime",
            "public class Program { public static int Main() { int value = 4; "
            "static_assert(value == 4); return 0; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopHir(
            directory / "StaticAssertRuntime.nsproj", error);
        tests.expect(
            !ok && error.find("not a compile-time bool constant") != std::string::npos,
            "project OOP pipeline rejects runtime-dependent static assertions");
    }

    {
        const auto directory = root / "Type";
        writeProject(
            directory,
            "StaticAssertType",
            "public class Program { public static int Main() { static_assert(1); return 0; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "StaticAssertType.nsproj", error);
        tests.expect(
            !ok && hasDiagnostic(compilation, DiagnosticId::InvalidCondition),
            "project OOP semantic model requires a bool assertion condition");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
