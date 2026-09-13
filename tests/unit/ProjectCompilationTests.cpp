#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectCompilationDumper.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ast::AstNodeKind;
using nativesharp::frontend::FrontendCompilation;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectCompilationDumper;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void createValidGraph(const std::filesystem::path& root)
{
    const std::filesystem::path core = root / "Core";
    writeFile(core / "Core.ns", R"NS(
using Contracts;

int CoreValue()
{
    ValueOverlay sameProject;
    return 10;
}
)NS");
    writeFile(core / "Math.ns", R"NS(
namespace Contracts
{
    public union ValueOverlay
    {
        public int Number;
        public float Floating;
    }

    public class Outer
    {
        public union Nested
        {
            public int Number;
            public float Floating;
        }
    }
}

int Double(int x) { return x + x; }
)NS");
    writeFile(
        core / "Core.nsproj",
        "<Project><PropertyGroup><Name>Core</Name><OutputType>Library</OutputType></PropertyGroup></Project>\n");

    const std::filesystem::path app = root / "App";
    writeFile(app / "Feature.ns", R"NS(
using Contracts;
using OverlayAlias = Contracts.ValueOverlay;

int Feature()
{
    ValueOverlay simple;
    Contracts.ValueOverlay qualified;
    OverlayAlias alias;
    Outer.Nested nested;
    return 1;
}
)NS");
    writeFile(app / "Main.ns", "int Main() { return 0; }\n");
    writeFile(
        app / "App.nsproj",
        "<Project>\n"
        "  <PropertyGroup><Name>App</Name><OutputType>Exe</OutputType></PropertyGroup>\n"
        "  <ItemGroup><ProjectReference Include=\"../Core/Core.nsproj\" /></ItemGroup>\n"
        "</Project>\n");
}

void testProjectSyntaxSlots(TestContext& tests, const std::filesystem::path& root)
{
    createValidGraph(root);

    ProjectCompilation compilation;
    std::string error;
    const bool success = compilation.compileProjectSyntax(root / "App" / "App.nsproj", error);
    tests.expect(success, "project syntax compilation succeeds for every discovered source");
    tests.expect(error.empty(), "successful project syntax compilation has no load error");
    tests.expect(compilation.projectGraph() != nullptr, "project compilation retains its project graph");
    tests.expect(
        compilation.projectGraph() != nullptr && compilation.projectGraph()->projects().size() == 2,
        "project compilation contains both dependency and root modules");
    tests.expect(compilation.units().size() == 4, "one syntax unit slot exists for every discovered source");

    if (compilation.units().size() == 4)
    {
        tests.expect(compilation.units()[0]->moduleName() == "Core", "dependency units are created first");
        tests.expect(compilation.units()[0]->sourceOrdinal() == 0, "first dependency source keeps project ordinal");
        tests.expect(compilation.units()[1]->sourceOrdinal() == 1, "second dependency source keeps project ordinal");
        tests.expect(compilation.units()[2]->moduleName() == "App", "root module units follow dependencies");
        tests.expect(compilation.units()[3]->sourceOrdinal() == 1, "root source ordinal is stable");

        for (std::size_t index = 0; index < compilation.units().size(); ++index)
        {
            const auto& unit = compilation.units()[index];
            tests.expect(unit->fileId().value() == index + 1, "SourceManager FileId follows deterministic unit order");
            tests.expect(unit->syntaxTree() != nullptr, "syntax unit retains its CompilationUnitSyntax tree");
            tests.expect(unit->isSyntaxValid(), "valid source slot is marked syntax-valid");
            tests.expect(
                unit->syntaxTree() != nullptr && unit->syntaxTree()->kind() == AstNodeKind::CompilationUnit,
                "each project source owns an independent compilation-unit AST root");
        }
    }

    std::ostringstream dump;
    ProjectCompilationDumper().dump(compilation, dump);
    tests.expect(dump.str().find("modules: 2") != std::string::npos, "compilation dump exposes module count");
    tests.expect(dump.str().find("syntax-units: 4") != std::string::npos, "compilation dump exposes unit count");
    tests.expect(dump.str().find("module=Core") != std::string::npos, "compilation dump preserves module ownership");
    tests.expect(dump.str().find("module=App") != std::string::npos, "compilation dump includes root module units");

    std::string secondLoadError;
    tests.expect(
        !compilation.compileProjectSyntax(root / "App" / "App.nsproj", secondLoadError) &&
            secondLoadError.find("single-use") != std::string::npos,
        "ProjectCompilation rejects accidental reuse instead of appending duplicate syntax slots");
}

void testSyntaxFailurePreservesSlots(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path project = root / "Broken";
    writeFile(project / "A.ns", "int Good() { return 1; }\n");
    writeFile(project / "B.ns", "int Broken( { return 0; }\n");
    writeFile(
        project / "Broken.nsproj",
        "<Project><PropertyGroup><Name>Broken</Name></PropertyGroup></Project>\n");

    ProjectCompilation compilation;
    std::string error;
    const bool success = compilation.compileProjectSyntax(project / "Broken.nsproj", error);
    tests.expect(!success, "syntax error in one source rejects project syntax compilation");
    tests.expect(error.empty(), "syntax diagnostics are distinct from project/source load errors");
    tests.expect(compilation.units().size() == 2, "syntax failure still preserves a slot for every source");
    tests.expect(compilation.diagnostics().hasErrors(), "syntax failure is recorded in shared diagnostics");
    if (compilation.units().size() == 2)
    {
        tests.expect(compilation.units()[0]->isSyntaxValid(), "valid sibling source remains syntax-valid");
        tests.expect(!compilation.units()[1]->isSyntaxValid(), "broken source slot is marked syntax-invalid");
    }
}

void testParallelSyntaxMatchesSerial(TestContext& tests, const std::filesystem::path& root)
{
    createValidGraph(root);
    ProjectCompilation serial;
    serial.setCompilationJobs(1);
    ProjectCompilation parallel;
    parallel.setCompilationJobs(4);
    std::string serialError;
    std::string parallelError;
    const bool serialOk = serial.compileProjectSyntax(root / "App" / "App.nsproj", serialError);
    const bool parallelOk = parallel.compileProjectSyntax(root / "App" / "App.nsproj", parallelError);
    tests.expect(serialOk && parallelOk, "serial and parallel syntax compilation both succeed");
    tests.expect(serialError == parallelError, "serial and parallel load errors are identical");

    std::ostringstream serialDump;
    std::ostringstream parallelDump;
    ProjectCompilationDumper{}.dump(serial, serialDump);
    ProjectCompilationDumper{}.dump(parallel, parallelDump);
    tests.expect(serialDump.str() == parallelDump.str(),
        "parallel reading, lexing and parsing preserve deterministic syntax-unit slots");
    if (serial.units().size() != parallel.units().size()) return;
    bool tokensMatch = true;
    for (std::size_t unit = 0; unit < serial.units().size(); ++unit)
    {
        const auto left = serial.units()[unit]->tokens();
        const auto right = parallel.units()[unit]->tokens();
        if (left.size() != right.size())
        {
            tokensMatch = false;
            break;
        }
        for (std::size_t token = 0; token < left.size(); ++token)
        {
            if (left[token].kind() != right[token].kind() ||
                left[token].lexeme() != right[token].lexeme())
            {
                tokensMatch = false;
                break;
            }
        }
    }
    tests.expect(tokensMatch, "parallel lexer output is byte-for-byte equivalent to serial output");

    const std::filesystem::path broken = root / "BrokenParallel";
    writeFile(broken / "A.ns", "int A( { return 1; }\n");
    writeFile(broken / "B.ns", "int B() { return @; }\n");
    writeFile(
        broken / "Broken.nsproj",
        "<Project><PropertyGroup><Name>BrokenParallel</Name></PropertyGroup></Project>\n");
    ProjectCompilation brokenSerial;
    brokenSerial.setCompilationJobs(1);
    ProjectCompilation brokenParallel;
    brokenParallel.setCompilationJobs(4);
    brokenSerial.compileProjectSyntax(broken / "Broken.nsproj", serialError);
    brokenParallel.compileProjectSyntax(broken / "Broken.nsproj", parallelError);
    const auto& serialDiagnostics = brokenSerial.diagnostics().diagnostics();
    const auto& parallelDiagnostics = brokenParallel.diagnostics().diagnostics();
    bool diagnosticsMatch = serialDiagnostics.size() == parallelDiagnostics.size();
    for (std::size_t index = 0; diagnosticsMatch && index < serialDiagnostics.size(); ++index)
    {
        diagnosticsMatch = serialDiagnostics[index].id() == parallelDiagnostics[index].id() &&
            serialDiagnostics[index].location() == parallelDiagnostics[index].location() &&
            serialDiagnostics[index].message() == parallelDiagnostics[index].message();
    }
    tests.expect(diagnosticsMatch,
        "parallel lexer and parser diagnostics preserve exact serial order and locations");
}

void testSingleFileCompatibility(TestContext& tests)
{
    FrontendCompilation compilation;
    const bool success = compilation.compileBuffer(
        "single_file_compatibility.ns",
        "int Main()\n{\n    return 23;\n}\n",
        true);
    tests.expect(success, "legacy single-file FrontendCompilation still reaches the full procedural pipeline");
    tests.expect(!compilation.diagnostics().hasErrors(), "single-file compatibility path remains diagnostic-clean");
    tests.expect(compilation.optimizationModule() != nullptr, "single-file compatibility still builds optimization IR");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage23_project_compilation";
    std::error_code errorCode;
    std::filesystem::remove_all(root, errorCode);
    std::filesystem::create_directories(root, errorCode);

    testProjectSyntaxSlots(tests, root);
    testSyntaxFailurePreservesSlots(tests, root);
    testParallelSyntaxMatchesSerial(tests, root / "ParallelEquivalence");
    testSingleFileCompatibility(tests);

    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}
