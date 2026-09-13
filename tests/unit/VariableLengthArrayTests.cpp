#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

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

std::size_t countNodes(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr) return 0;
    std::size_t count = node->kind == kind ? 1 : 0;
    for (const auto& child : node->children) count += countNodes(child.get(), kind);
    return count;
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
        std::filesystem::temp_directory_path() / "nativesharp_variable_length_array_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    {
        const auto directory = root / "DynamicLocal";
        writeProject(
            directory,
            "VariableLengthLocal",
            "public class Program { public static int Fill(int length) { "
            "int[length] values; values[0] = 40; values[length - 1] = 2; "
            "return values[0] + values[length - 1]; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(
            directory / "VariableLengthLocal.nsproj", error);
        tests.expect(ok && error.empty(),
            "a runtime local extent reaches OOP MIR as stack-backed storage");

        if (ok && compilation.methodBodyModel() != nullptr)
        {
            const auto locals = compilation.methodBodyModel()->locals();
            tests.expect(locals.size() == 1 && locals[0].isVariableLengthArray,
                "the semantic local preserves that its source declaration is a VLA");
            tests.expect(locals.size() == 1 &&
                    locals[0].variableLengthArrayElementTypeKey == "builtin:int" &&
                    locals[0].typeKey == "builtin:int*",
                "the VLA keeps its element identity while reusing raw pointer indexing internally");
        }

        std::size_t allocations = 0;
        std::size_t addresses = 0;
        if (ok && compilation.oopMirModel() != nullptr)
        {
            for (const auto& function : compilation.oopMirModel()->functions())
            {
                allocations += countNodes(function.body.get(), ProjectOopMirNodeKind::StackAllocation);
                addresses += countNodes(function.body.get(), ProjectOopMirNodeKind::RawElementAddress);
            }
        }
        tests.expect(allocations == 1,
            "the declaration emits exactly one stack allocation at its lexical position");
        tests.expect(addresses >= 4,
            "VLA reads and writes use the existing raw element-address operation");
    }

    {
        const auto directory = root / "FixedStillInline";
        writeProject(
            directory,
            "FixedStillInline",
            "public class Program { public static int Main() { int[4] values; "
            "values[0] = 42; return values[0]; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(
            directory / "FixedStillInline.nsproj", error);
        tests.expect(ok && error.empty(), "constant local arrays remain valid inline storage");
        if (ok && compilation.methodBodyModel() != nullptr)
        {
            const auto locals = compilation.methodBodyModel()->locals();
            tests.expect(locals.size() == 1 && !locals[0].isVariableLengthArray &&
                    locals[0].typeKey == "builtin:int[4]",
                "a constant extent is not reclassified as a VLA");
        }
    }

    {
        const auto directory = root / "Multidimensional";
        writeProject(
            directory,
            "MultidimensionalVariableLength",
            "public class Program { public static int Fill(int rows, int columns) { "
            "int[rows, columns] matrix; columns = 1; "
            "matrix[0, 0] = 1; matrix[rows - 1, 2] = 41; "
            "return matrix[0, 0] + matrix[rows - 1, 2]; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(
            directory / "MultidimensionalVariableLength.nsproj", error);
        tests.expect(ok && error.empty(),
            "a multidimensional VLA reaches OOP MIR with row-major indexing");
        if (ok && compilation.methodBodyModel() != nullptr)
        {
            const auto locals = compilation.methodBodyModel()->locals();
            tests.expect(locals.size() == 3 && locals[0].isVariableLengthArray &&
                    locals[0].variableLengthArrayExtentSyntaxes.size() == 2 &&
                    locals[0].variableLengthArrayExtentLocalOrdinals.size() == 2,
                "two source-order extents are captured into two hidden locals");
            tests.expect(locals.size() == 3 &&
                    locals[locals[0].variableLengthArrayExtentLocalOrdinals[0]].typeKey ==
                        "builtin:nuint" &&
                    locals[locals[0].variableLengthArrayExtentLocalOrdinals[1]].typeKey ==
                        "builtin:nuint",
                "captured extents use pointer-width unsigned storage for size arithmetic");
        }
        std::size_t allocations = 0;
        std::size_t addresses = 0;
        if (ok && compilation.oopMirModel() != nullptr)
        {
            for (const auto& function : compilation.oopMirModel()->functions())
            {
                allocations += countNodes(function.body.get(), ProjectOopMirNodeKind::StackAllocation);
                addresses += countNodes(function.body.get(), ProjectOopMirNodeKind::RawElementAddress);
            }
        }
        tests.expect(allocations == 1,
            "all VLA dimensions compose into one contiguous stack reservation");
        tests.expect(addresses == 4,
            "each multidimensional VLA read or write lowers to one flattened raw address");
    }

    {
        const auto directory = root / "RankMismatch";
        writeProject(
            directory,
            "VariableLengthRankMismatch",
            "public class Program { public static int Main(int rows, int columns) { "
            "int[rows, columns] matrix; return matrix[0]; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "VariableLengthRankMismatch.nsproj", error);
        tests.expect(!ok && hasDiagnostic(compilation, DiagnosticId::InvalidIndex),
            "multidimensional VLA access requires exactly one index per dimension");
    }

    {
        const auto directory = root / "FieldRejected";
        writeProject(
            directory,
            "VariableLengthField",
            "public struct Invalid { public int[Count] Values; public int Count; }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectObjectLayouts(
            directory / "VariableLengthField.nsproj", error);
        tests.expect(!ok && hasDiagnostic(compilation, DiagnosticId::InvalidArrayCount),
            "runtime extents remain forbidden in fields");
    }

    {
        const auto directory = root / "NonIntegral";
        writeProject(
            directory,
            "VariableLengthNonIntegral",
            "public class Program { public static int Main() { double n = 3.0; "
            "int[n] values; return 0; } }");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "VariableLengthNonIntegral.nsproj", error);
        tests.expect(!ok && hasDiagnostic(compilation, DiagnosticId::InvalidArrayCount),
            "a VLA extent must have an integral type");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
