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
        std::filesystem::temp_directory_path() / "nativesharp_fixed_multidimensional_array_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    {
        const auto directory = root / "Canonical";
        writeProject(
            directory,
            "FixedMultidimensionalCanonical",
            "public struct Grid { public int[3, 4] Cells; public byte Tail; } "
            "public class Program { public static int Main() { "
            "int[3, 4] local; static_assert(sizeof(local) == 48u); return 0; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(
            directory / "FixedMultidimensionalCanonical.nsproj", error);
        tests.expect(ok && error.empty(),
            "rank syntax reaches OOP MIR as a fixed inline multidimensional array");
        if (ok && compilation.memberModel() != nullptr && compilation.objectLayoutModel() != nullptr)
        {
            const auto& members = compilation.memberModel()->members();
            const nativesharp::frontend::ClassMemberSymbol* cells = nullptr;
            const nativesharp::frontend::ClassMemberSymbol* tail = nullptr;
            for (const auto& member : members)
            {
                if (member.name == "Cells") cells = &member;
                if (member.name == "Tail") tail = &member;
            }
            tests.expect(cells != nullptr && cells->declaredTypeKey == "builtin:int[4][3]",
                "T[3,4] canonicalizes to three rows whose contiguous row contains four T values");
            const auto* cellsLayout = cells != nullptr
                ? compilation.objectLayoutModel()->fieldLayoutForMember(
                    static_cast<std::size_t>(cells - members.data()))
                : nullptr;
            const auto* tailLayout = tail != nullptr
                ? compilation.objectLayoutModel()->fieldLayoutForMember(
                    static_cast<std::size_t>(tail - members.data()))
                : nullptr;
            tests.expect(cellsLayout != nullptr && cellsLayout->size == 48 && cellsLayout->alignment == 4,
                "three-by-four int storage is 48 inline bytes with int alignment");
            tests.expect(tailLayout != nullptr && tailLayout->offset == 48,
                "the field after a multidimensional array starts after every inline element");
        }
    }

    {
        const auto directory = root / "NestedValue";
        writeProject(
            directory,
            "FixedMultidimensionalNestedValue",
            "public struct Cell { public long Value; } "
            "public struct Grid { public Cell[2, 3, 4] Cells; } "
            "public class Program { public static int Main() { "
            "static_assert(sizeof(Grid) == 192u); return 0; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            compilation.compileProjectOopMir(
                directory / "FixedMultidimensionalNestedValue.nsproj", error) && error.empty(),
            "three fixed dimensions compose recursively for a named value element type");
    }

    {
        const auto directory = root / "ZeroDimension";
        writeProject(
            directory,
            "FixedMultidimensionalZero",
            "public struct Grid { public int[3, 0] Cells; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(
            directory / "FixedMultidimensionalZero.nsproj", error);
        tests.expect(!ok && hasDiagnostic(compilation, DiagnosticId::InvalidArrayCount),
            "every fixed dimension must be a positive integer constant");
    }

    {
        const auto directory = root / "MissingDimension";
        writeProject(
            directory,
            "FixedMultidimensionalMissing",
            "public struct Grid { public int[3,] Cells; }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(!compilation.compileProjectSyntax(
                directory / "FixedMultidimensionalMissing.nsproj", error),
            "a trailing comma cannot manufacture an omitted fixed dimension");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
