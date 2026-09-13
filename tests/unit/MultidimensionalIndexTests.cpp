#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
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
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile "
            "Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

void collectRawStrides(const ProjectOopMirNode* node, std::vector<std::uint64_t>& strides)
{
    if (node == nullptr) return;
    if (node->kind == ProjectOopMirNodeKind::RawElementAddress) strides.push_back(node->immediate);
    for (const auto& child : node->children) collectRawStrides(child.get(), strides);
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
        std::filesystem::temp_directory_path() / "nativesharp_multidimensional_index_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    {
        const auto directory = root / "ReadWrite";
        writeProject(
            directory,
            "MultidimensionalReadWrite",
            "public class Program { public static int Main() { "
            "int[2, 3] matrix; matrix[0, 0] = 1; matrix[1, 2] = 41; "
            "return matrix[0, 0] + matrix[1, 2]; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(
            directory / "MultidimensionalReadWrite.nsproj", error);
        tests.expect(ok && error.empty(),
            "multidimensional fixed-array reads and writes reach OOP MIR");
        std::vector<std::uint64_t> strides;
        if (ok && compilation.oopMirModel() != nullptr)
        {
            for (const auto& function : compilation.oopMirModel()->functions())
                collectRawStrides(function.body.get(), strides);
        }
        bool sawRowStride = false;
        bool sawElementStride = false;
        for (const std::uint64_t stride : strides)
        {
            sawRowStride = sawRowStride || stride == 12;
            sawElementStride = sawElementStride || stride == 4;
        }
        tests.expect(sawRowStride,
            "the first index advances by one complete three-int row");
        tests.expect(sawElementStride,
            "the second index advances by one int within the selected row");
    }

    {
        const auto directory = root / "Chained";
        writeProject(
            directory,
            "MultidimensionalChained",
            "public class Program { public static int Main() { "
            "int[2, 3] matrix; matrix[1][2] = 42; return matrix[1, 2]; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(compilation.compileProjectOopMir(
                directory / "MultidimensionalChained.nsproj", error) && error.empty(),
            "comma and chained indexing address compatible fixed-array storage");
    }

    {
        const auto directory = root / "Bounds";
        writeProject(
            directory,
            "MultidimensionalBounds",
            "public class Program { public static int Main() { "
            "int[2, 3] matrix; return matrix[1, 3]; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "MultidimensionalBounds.nsproj", error);
        tests.expect(!ok && hasDiagnostic(compilation, DiagnosticId::ConstantIndexOutOfBounds),
            "constant indices are checked against the extent of their own dimension");
    }

    {
        const auto directory = root / "Rank";
        writeProject(
            directory,
            "MultidimensionalRank",
            "public class Program { public static int Main() { "
            "int[2, 3] matrix; return matrix[0, 0, 0]; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "MultidimensionalRank.nsproj", error);
        tests.expect(!ok && hasDiagnostic(compilation, DiagnosticId::InvalidIndex),
            "an index list cannot consume more dimensions than the fixed-array rank");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
