#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::filesystem::path writeSource(std::string_view name, std::string_view text)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream stream(path, std::ios::binary);
    stream << text;
    return path;
}

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

nativesharp::ExitCode runDriver(
    const std::vector<std::string_view>& arguments,
    std::ostringstream& output,
    std::ostringstream& error)
{
    const nativesharp::driver::Driver driver;
    return driver.run(arguments, output, error);
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;

    {
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = runDriver({}, output, error);
        tests.expect(code == ExitCode::Success, "empty command line prints help successfully");
        const std::string helpText = "NativeSharp compiler (NativeSharp";
        tests.expect(
            output.str().find(helpText) != std::string::npos,
            "help text is written to stdout");
        tests.expect(output.str().find("/subsystem:windows") != std::string::npos,
            "help documents Windows GUI executable emission");
        tests.expect(output.str().find("-gline-tables-only") != std::string::npos,
            "help documents standard CodeView/PDB debug profiles");
        tests.expect(error.str().empty(), "help path keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {"--check", "input.ns", "-gprivate"};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CommandLineError,
            "an unknown -g spelling returns the command-line exit class");
        tests.expect(error.str().find("expected -g0, -gline-tables-only or -g") != std::string::npos,
            "an unknown -g spelling names the supported standard profiles");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {"--not-an-option"};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CommandLineError, "unknown option returns command-line exit code 2");
        tests.expect(output.str().empty(), "unknown option does not write normal output");
        tests.expect(error.str().find("unknown option") != std::string::npos,
            "unknown option diagnostic is written to stderr");
    }

    const std::filesystem::path validPath = writeSource(
        "nativesharp_driver_valid.ns",
        "int Main() { long x = 42; return (int)x; }\n");
    const std::filesystem::path libraryPath = writeSource(
        "nativesharp_driver_library.ns",
        "int Add(int a, int b) { return a + b; }\n");
    const std::filesystem::path invalidPath = writeSource(
        "nativesharp_driver_invalid.ns",
        "int Main() { int x = 1; if (x) { return 1; } return 0; }\n");
    const std::filesystem::path unresolvedPath = writeSource(
        "nativesharp_driver_unresolved.ns",
        "extern C int MissingNativeSymbol(); int Main() { return MissingNativeSymbol(); }\n");
    const std::filesystem::path classOnlyPath = writeSource(
        "nativesharp_driver_stage25_class_only.ns",
        "class Widget {}\n");
    const std::filesystem::path projectDirectory =
        std::filesystem::temp_directory_path() / "nativesharp_driver_stage25_project";
    std::error_code projectCleanupError;
    std::filesystem::remove_all(projectDirectory, projectCleanupError);
    std::filesystem::create_directories(projectDirectory);
    const std::filesystem::path projectPath = projectDirectory / "Game.nsproj";
    writeFile(projectPath,
        "<Project><PropertyGroup><Name>Game</Name><OutputType>Exe</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(projectDirectory / "Main.ns", "namespace Game; struct Marker { int X; } int Main() { return 0; }\n");

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = projectPath.string();
        const std::vector<std::string_view> arguments = {"--check-project", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--check-project validates a project descriptor");
        tests.expect(output.str().find("project check: ok (1 source, 0 project references)") !=
                std::string::npos,
            "project check reports the discovered source and reference counts");
        tests.expect(error.str().empty(), "valid project check keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = projectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-project loads a project descriptor");
        tests.expect(output.str().find("project Game") != std::string::npos &&
                output.str().find("[0] Main.ns") != std::string::npos,
            "project dump exposes the module and deterministic source ordinal");
        tests.expect(error.str().empty(), "valid project dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = (projectDirectory / "Missing.nsproj").string();
        const std::vector<std::string_view> arguments = {"--check-project", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CompilationError,
            "--check-project rejects a missing project with the compilation exit code");
        tests.expect(error.str().find("does not exist") != std::string::npos,
            "missing project diagnostic is written to stderr");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = projectPath.string();
        const std::vector<std::string_view> arguments = {"--check-project-graph", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--check-project-graph validates the transitive graph");
        tests.expect(output.str().find("project graph check: ok (1 project)") != std::string::npos,
            "project graph check reports graph cardinality");
        tests.expect(error.str().empty(), "valid project graph check keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = projectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project-graph", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-project-graph emits the dependency order");
        tests.expect(output.str().find("[0] Game (root)") != std::string::npos,
            "project graph dump marks the root project");
        tests.expect(error.str().empty(), "valid project graph dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = projectPath.string();
        const std::vector<std::string_view> arguments = {"--check-project-syntax", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--check-project-syntax parses every project source");
        tests.expect(output.str().find("project syntax check: ok (1 syntax unit)") != std::string::npos,
            "project syntax check reports syntax-unit cardinality");
        tests.expect(error.str().empty(), "valid project syntax check keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = projectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project-compilation", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-project-compilation exposes syntax slots");
        tests.expect(output.str().find("syntax-units: 1") != std::string::npos &&
                output.str().find("module=Game") != std::string::npos,
            "project compilation dump preserves source ownership");
        tests.expect(error.str().empty(), "valid project compilation dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = projectPath.string();
        const std::vector<std::string_view> arguments = {"--check-project-names", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--check-project-names binds project declaration spaces");
        tests.expect(output.str().find("project name lookup check: ok (1 named type") != std::string::npos,
            "project name lookup check reports named-type cardinality");
        tests.expect(error.str().empty(), "valid project name lookup keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = projectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project-names", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-project-names emits namespace/type lookup state");
        tests.expect(output.str().find("ProjectNameLookup") != std::string::npos &&
                output.str().find("Game::Game.Marker") != std::string::npos,
            "project name lookup dump exposes namespace and module-qualified type identity");
        tests.expect(error.str().empty(), "project name lookup dump keeps stderr empty");
    }

    const std::filesystem::path partialProjectDirectory =
        std::filesystem::temp_directory_path() / "nativesharp_driver_stage25_partial_project";
    std::filesystem::remove_all(partialProjectDirectory, projectCleanupError);
    std::filesystem::create_directories(partialProjectDirectory);
    const std::filesystem::path partialProjectPath = partialProjectDirectory / "PartialGame.nsproj";
    writeFile(partialProjectPath,
        "<Project><PropertyGroup><Name>PartialGame</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(partialProjectDirectory / "A.ns",
        "namespace Game.Model; public partial class Widget { private int Value; public int Get() { return this.Value; } }\n");
    writeFile(partialProjectDirectory / "B.ns",
        "namespace Game.Model; public partial class Widget { public int Add(int delta) { return this.Value + delta; } }\n");
    writeFile(partialProjectDirectory / "Use.ns",
        "using Game.Model; struct Holder { Widget Value; }\n");

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = partialProjectPath.string();
        const std::vector<std::string_view> arguments = {"--check-project-names", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "--check-project-names accepts compatible cross-file partial classes");
        tests.expect(output.str().find("project name lookup check: ok (2 named types") !=
                std::string::npos,
            "Stage 25 CLI check reports one aggregated class plus its consumer struct");
        tests.expect(error.str().empty(), "Stage 25 CLI partial-class check keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = partialProjectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project-names", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "--dump-project-names exposes aggregated class observability");
        tests.expect(output.str().find("class Game.Model.Widget") != std::string::npos &&
                output.str().find("parts=2") != std::string::npos &&
                output.str().find("A.ns") != std::string::npos &&
                output.str().find("B.ns") != std::string::npos,
            "Stage 25 CLI dump exposes class kind, part count and both source files");
        tests.expect(error.str().empty(), "Stage 25 CLI partial-class dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = partialProjectPath.string();
        const std::vector<std::string_view> arguments = {"--check-project-members", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "--check-project-members builds the aggregated Stage 26 member declaration space");
        tests.expect(output.str().find("project member binding check: ok (4 members, 4 body bindings)") !=
                std::string::npos,
            "Stage 26 CLI reports member and this-binding cardinality");
        tests.expect(error.str().empty(), "valid Stage 26 member check keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = partialProjectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project-members", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "--dump-project-members exposes member symbols and body bindings");
        tests.expect(output.str().find("ProjectMemberModel") != std::string::npos &&
                output.str().find("private field Value") != std::string::npos &&
                output.str().find("signature=Add(builtin:int)") != std::string::npos &&
                output.str().find("this-member") != std::string::npos,
            "Stage 26 member dump exposes field, method signature, and explicit this.member binding");
        tests.expect(error.str().empty(), "valid Stage 26 member dump keeps stderr empty");
    }

    const std::filesystem::path methodBodyProjectDirectory =
        std::filesystem::temp_directory_path() / "nativesharp_driver_stage27_method_body_project";
    std::filesystem::remove_all(methodBodyProjectDirectory, projectCleanupError);
    std::filesystem::create_directories(methodBodyProjectDirectory);
    const std::filesystem::path methodBodyProjectPath =
        methodBodyProjectDirectory / "Stage27Driver.nsproj";
    writeFile(methodBodyProjectPath,
        "<Project><PropertyGroup><Name>Stage27Driver</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(methodBodyProjectDirectory / "A.ns",
        "namespace Demo; public partial class Counter { int Value; "
        "public int Add(int delta) { return this.Value + delta; } "
        "public float Add(float delta) { return 0.0f; } }\n");
    writeFile(methodBodyProjectDirectory / "B.ns",
        "namespace Demo; public partial class Counter { "
        "public int Run(int Value) { int total = Add(Value); "
        "{ int Value = 1; total = this.Add(Value); } "
        "int answer = Add(total); return answer; } }\n");

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = methodBodyProjectPath.string();
        const std::vector<std::string_view> arguments = {"--check-project-method-bodies", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "--check-project-method-bodies runs Stage 27 semantic body binding");
        tests.expect(output.str().find(
                "project method-body semantic check: ok (4 scopes, 3 locals, 12 name bindings, 3 calls)") !=
                std::string::npos,
            "Stage 27 CLI check reports scope/local/name/call cardinalities");
        tests.expect(error.str().empty(), "valid Stage 27 method-body check keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = methodBodyProjectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project-method-bodies", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "--dump-project-method-bodies exposes Stage 27 semantic facts");
        tests.expect(output.str().find("parameter-shadows-member") != std::string::npos &&
                output.str().find("local-shadows-parameter") != std::string::npos &&
                output.str().find("receiver=this") != std::string::npos &&
                output.str().find("conversion=identity rank=0") != std::string::npos,
            "Stage 27 dump exposes shadowing, explicit receiver and selected conversion");
        tests.expect(error.str().empty(), "valid Stage 27 method-body dump keeps stderr empty");
    }

    const std::filesystem::path constructionProjectDirectory =
        std::filesystem::temp_directory_path() / "nativesharp_driver_stage28_construction_project";
    std::filesystem::remove_all(constructionProjectDirectory, projectCleanupError);
    std::filesystem::create_directories(constructionProjectDirectory);
    const std::filesystem::path constructionProjectPath =
        constructionProjectDirectory / "Stage28Driver.nsproj";
    writeFile(constructionProjectPath,
        "<Project><PropertyGroup><Name>Stage28Driver</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(constructionProjectDirectory / "Main.ns",
        "class Box { public Box(int x) { } public Box(float x) { } "
        "Box MakeInt() { return new Box(1); } Box MakeFloat() { return new Box(1.0f); } } "
        "class Empty { } class Factory { Empty Make() { return new Empty(); } }\n");

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = constructionProjectPath.string();
        const std::vector<std::string_view> arguments = {"--check-project-constructions", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "--check-project-constructions runs Stage 28 constructor and new-expression binding");
        tests.expect(output.str().find(
                "project construction semantic check: ok (3 constructions, 2 explicit, 1 implicit-default)") !=
                std::string::npos,
            "Stage 28 CLI reports explicit and implicit-default construction counts");
        tests.expect(error.str().empty(), "valid Stage 28 construction check keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = constructionProjectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project-constructions", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "--dump-project-constructions exposes Stage 28 constructor candidates and selections");
        tests.expect(output.str().find("selected=Box(builtin:int)") != std::string::npos &&
                output.str().find("selected=Box(builtin:float)") != std::string::npos &&
                output.str().find("selected=implicit-default") != std::string::npos,
            "Stage 28 construction dump exposes overload and synthesized-default selections");
        tests.expect(error.str().empty(), "valid Stage 28 construction dump keeps stderr empty");
    }

    const std::filesystem::path layoutProjectDirectory =
        std::filesystem::temp_directory_path() / "nativesharp_driver_stage29_layout_project";
    std::filesystem::remove_all(layoutProjectDirectory, projectCleanupError);
    std::filesystem::create_directories(layoutProjectDirectory);
    const std::filesystem::path layoutProjectPath = layoutProjectDirectory / "Stage29Driver.nsproj";
    writeFile(layoutProjectPath,
        "<Project><PropertyGroup><Name>Stage29Driver</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(layoutProjectDirectory / "A.ns",
        "struct Pair { short A; int B; } class Empty {} partial class Box { byte Tag; int Count; Pair PairValue; Empty Child; int[3] Samples; "
        "Box() { } Box Make() { return new Box(); } }\n");
    writeFile(layoutProjectDirectory / "B.ns",
        "partial class Box { double Weight; Empty MakeEmpty() { return new Empty(); } }\n");

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = layoutProjectPath.string();
        const std::vector<std::string_view> arguments = {"--check-project-layouts", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "--check-project-layouts runs Stage 29 physical object layout and allocation planning");
        tests.expect(output.str().find(
                "project object layout check: ok (2 class layouts, 6 field layouts, 2 allocation plans)") !=
                std::string::npos,
            "Stage 29 CLI reports class, field and allocation-plan cardinalities");
        tests.expect(error.str().empty(), "valid Stage 29 layout check keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = layoutProjectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project-layouts", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "--dump-project-layouts exposes Stage 29 field offsets and initialization plans");
        tests.expect(output.str().find("Box size=56 align=8 header=8") != std::string::npos &&
                output.str().find("name=Weight") != std::string::npos &&
                output.str().find("step=zero-initialize-storage") != std::string::npos &&
                output.str().find("step=invoke-constructor constructor=Box()") != std::string::npos,
            "Stage 29 dump exposes deterministic layout, zero-init and selected-constructor interface");
        tests.expect(error.str().empty(), "valid Stage 29 layout dump keeps stderr empty");
    }

    const std::filesystem::path oopProjectDirectory =
        std::filesystem::temp_directory_path() / "nativesharp_driver_stage30_oop_project";
    std::filesystem::remove_all(oopProjectDirectory, projectCleanupError);
    std::filesystem::create_directories(oopProjectDirectory);
    const std::filesystem::path oopProjectPath = oopProjectDirectory / "Stage30Driver.nsproj";
    writeFile(oopProjectPath,
        "<Project><PropertyGroup><Name>Stage30Driver</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(oopProjectDirectory / "A.ns",
        "namespace Demo; public partial class Counter { byte Tag; int Value; "
        "public Counter(int start) { this.Value = start; } "
        "public int Add(int delta) { this.Value = this.Value + delta; return this.Value; } }\n");
    writeFile(oopProjectDirectory / "B.ns",
        "namespace Demo; public partial class Counter { public Counter Make(int start) { return new Counter(start); } } "
        "public class Empty { } public class Factory { public Empty MakeEmpty() { return new Empty(); } }\n");

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = oopProjectPath.string();
        const std::vector<std::string_view> arguments = {"--check-project-oop-hir", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--check-project-oop-hir builds Stage 30 semantic OOP HIR");
        tests.expect(output.str().find("project OOP HIR check: ok (4 callables)") != std::string::npos,
            "Stage 30 HIR check reports explicit callable cardinality");
        tests.expect(error.str().empty(), "valid Stage 30 HIR check keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = oopProjectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project-oop-hir", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-project-oop-hir exposes Stage 30 OOP HIR");
        tests.expect(output.str().find("project.oop.hir functions=4") != std::string::npos &&
                output.str().find("object-construction") != std::string::npos &&
                output.str().find("abi=this:first") != std::string::npos,
            "Stage 30 HIR dump exposes construction and implicit-this ABI intent");
        tests.expect(error.str().empty(), "valid Stage 30 HIR dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = oopProjectPath.string();
        const std::vector<std::string_view> arguments = {"--check-project-oop-mir", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--check-project-oop-mir resolves Stage 30 physical OOP operations");
        tests.expect(output.str().find("project OOP MIR check: ok (4 callables)") != std::string::npos,
            "Stage 30 MIR check reports lowered callable cardinality");
        tests.expect(error.str().empty(), "valid Stage 30 MIR check keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = oopProjectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project-oop-mir", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-project-oop-mir exposes Stage 30 physical lowering decisions");
        tests.expect(output.str().find("project.oop.mir functions=4") != std::string::npos &&
                output.str().find("offset=12 align=4") != std::string::npos &&
                output.str().find("zero=true failure=null") != std::string::npos,
            "Stage 30 MIR dump exposes field offset and allocation contract");
        tests.expect(error.str().empty(), "valid Stage 30 MIR dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = oopProjectPath.string();
        const std::vector<std::string_view> arguments = {"--verify-project-oop-llvm", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--verify-project-oop-llvm runs the LLVM verifier on Stage 30 output");
        tests.expect(output.str().find("project OOP LLVM verification: ok (4 lowered callables)") != std::string::npos,
            "Stage 30 LLVM verification reports lowered callable cardinality");
        tests.expect(error.str().empty(), "valid Stage 30 LLVM verification keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = oopProjectPath.string();
        const std::vector<std::string_view> arguments = {"--dump-project-oop-llvm", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-project-oop-llvm emits verified Stage 30 LLVM IR");
        // NSC-LIMIT-009. `@__nativesharp_alloc` es un nombre FIJO del runtime y sale sin comillas;
        // `__nsc_ctor_` sale de una identidad, lleva el `$3A$3A` del `::` del modulo, y LLVM
        // entrecomilla todo nombre con un byte que no sea alfanumerico, `-`, `.` o `_`. Las dos
        // formas conviven en la misma linea a proposito: es el control de que la diferencia es la
        // que se dice y no otra cosa.
        tests.expect(output.str().find("declare ptr @__nativesharp_alloc(i64)") != std::string::npos &&
                output.str().find("getelementptr i8, ptr %this, i64 12") != std::string::npos &&
                output.str().find("call void @\"__nsc_ctor_") != std::string::npos,
            "Stage 30 LLVM dump exposes allocator, physical field addressing and constructor call");
        tests.expect(error.str().empty(), "valid Stage 30 LLVM dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = oopProjectPath.string();
        const std::filesystem::path objectPath = oopProjectDirectory / "stage30_driver.obj";
        const std::string object = objectPath.string();
        const std::vector<std::string_view> arguments = {
            "--emit-project-oop-obj", path, "-O2", "-o", object};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--emit-project-oop-obj emits Stage 30 Windows x64 object code");
        tests.expect(output.str().find("project OOP object emission: ok (machine=0x8664") != std::string::npos,
            "Stage 30 object command verifies AMD64 COFF structure");
        tests.expect(std::filesystem::exists(objectPath) && std::filesystem::file_size(objectPath) > 0,
            "Stage 30 object command materializes a non-empty COFF object");
        tests.expect(error.str().empty(), "valid Stage 30 OOP object emission keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "default program mode accepts a semantically valid Main");
        tests.expect(error.str().empty(), "valid frontend compilation keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = libraryPath.string();
        const std::vector<std::string_view> arguments = {"--check", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--check accepts a translation unit without Main");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = classOnlyPath.string();
        const std::vector<std::string_view> arguments = {"--check", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CompilationError,
            "single-file procedural --check does not pretend the Stage 30 project OOP pipeline is the procedural single-file path");
        tests.expect(error.str().find("Stage 34 project pipeline") != std::string::npos &&
                error.str().find("does not lower classes") != std::string::npos,
            "single-file class diagnostic explains the Stage 30 project/procedural boundary");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = invalidPath.string();
        const std::vector<std::string_view> arguments = {"--check", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CompilationError, "semantic error returns compilation exit code 1");
        tests.expect(error.str().find("if condition must have type bool") != std::string::npos,
            "semantic diagnostic is rendered to stderr");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--dump-tokens", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-tokens succeeds on valid source");
        tests.expect(output.str().find("KwInt") != std::string::npos,
            "token dump is written to stdout");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--dump-sema", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-sema succeeds on valid source");
        tests.expect(output.str().find("conversion implicit-numeric int -> long") != std::string::npos,
            "semantic dump exposes inserted conversions");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--dump-mir", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-mir succeeds on valid source");
        tests.expect(output.str().find("mir.module") != std::string::npos &&
                output.str().find("convert") != std::string::npos,
            "MIR dump is written to stdout and exposes lowered conversions");
        tests.expect(error.str().empty(), "valid MIR dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--verify-mir", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--verify-mir succeeds on valid source");
        tests.expect(output.str().find("MIR verification: ok") != std::string::npos,
            "MIR verification result is written to stdout");
        tests.expect(error.str().empty(), "valid MIR verification keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--dump-cfg", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-cfg succeeds on valid source");
        tests.expect(output.str().find("cfg.module") != std::string::npos &&
                output.str().find("rpo:") != std::string::npos,
            "CFG dump is written to stdout and exposes traversal metadata");
        tests.expect(error.str().empty(), "valid CFG dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--verify-cfg", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--verify-cfg succeeds on valid source");
        tests.expect(output.str().find("CFG verification: ok") != std::string::npos,
            "CFG verification result is written to stdout");
        tests.expect(error.str().empty(), "valid CFG verification keeps stderr empty");
    }


    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--dump-dominance", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-dominance succeeds on valid source");
        tests.expect(output.str().find("dominance.module") != std::string::npos &&
                output.str().find("idom=") != std::string::npos,
            "dominance dump is written to stdout and exposes immediate dominators");
        tests.expect(error.str().empty(), "valid dominance dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--verify-dominance", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--verify-dominance succeeds on valid source");
        tests.expect(output.str().find("Dominance verification: ok") != std::string::npos,
            "dominance verification result is written to stdout");
        tests.expect(error.str().empty(), "valid dominance verification keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--dump-ssa", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-ssa succeeds on valid source");
        tests.expect(output.str().find("ssa.module") != std::string::npos,
            "SSA dump is written to stdout");
        tests.expect(error.str().empty(), "valid SSA dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--verify-ssa", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--verify-ssa succeeds on valid source");
        tests.expect(output.str().find("SSA verification: ok") != std::string::npos,
            "SSA verification result is written to stdout");
        tests.expect(error.str().empty(), "valid SSA verification keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--dump-opt", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-opt succeeds on valid source");
        tests.expect(output.str().find("optimized.ssa.module") != std::string::npos,
            "optimized SSA dump is written to stdout");
        tests.expect(error.str().empty(), "valid optimized dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--verify-opt", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--verify-opt succeeds on valid source");
        tests.expect(output.str().find("Optimization verification: ok") != std::string::npos,
            "optimization verification result is written to stdout");
        tests.expect(error.str().empty(), "valid optimization verification keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--dump-pass-pipeline", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-pass-pipeline succeeds on valid source");
        tests.expect(output.str().find("pass.pipeline") != std::string::npos &&
                output.str().find("dead-code-elimination") != std::string::npos,
            "pass pipeline trace is written to stdout");
        tests.expect(error.str().empty(), "valid pass trace keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--dump-llvm", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-llvm succeeds on valid source");
        tests.expect(output.str().find("target triple = \"x86_64-pc-windows-msvc\"") != std::string::npos,
            "LLVM dump is written to stdout with the target triple");
        tests.expect(error.str().empty(), "valid LLVM dump keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--verify-llvm", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--verify-llvm succeeds on valid source");
        tests.expect(output.str().find("LLVM verification: ok") != std::string::npos,
            "LLVM verification result is written to stdout");
        tests.expect(error.str().empty(), "valid LLVM verification keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {"--print-llvm-pipeline", "-O2"};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--print-llvm-pipeline accepts -O2");
        tests.expect(output.str().find("default<O2>") != std::string::npos,
            "pipeline dump identifies LLVM's O2 default pipeline");
        tests.expect(output.str().find("verify-each: yes") != std::string::npos,
            "pipeline dump documents per-pass verification");
        tests.expect(error.str().empty(), "pipeline reporting keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"-O2", "--verify-llvm-opt", path};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "optimization flag may precede the LLVM optimization action");
        tests.expect(output.str().find("optimization: O2") != std::string::npos,
            "optimized LLVM verification reports the selected level");
        tests.expect(error.str().empty(), "optimized LLVM verification keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--check", path, "-O4"};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CommandLineError, "unsupported -O4 is rejected by the driver");
        tests.expect(error.str().find("unsupported LLVM optimization level") != std::string::npos,
            "unsupported optimization level has a precise command-line diagnostic");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string path = validPath.string();
        const std::vector<std::string_view> arguments = {"--check", path, "-O1", "-O2"};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CommandLineError, "duplicate optimization levels are rejected");
        tests.expect(error.str().find("duplicate LLVM optimization level") != std::string::npos,
            "duplicate optimization levels have a precise diagnostic");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {"--print-linker"};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--print-linker discovers the Stage 19 linker");
        tests.expect(output.str().find("linker:") != std::string::npos &&
                output.str().find("flavor: Windows COFF/PE") != std::string::npos,
            "--print-linker reports the resolved PE linker without assuming its implementation");
        tests.expect(output.str().find("librarian:") != std::string::npos,
            "--print-linker reports the independently resolved static librarian");
        tests.expect(error.str().empty(), "successful linker discovery keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {"--dump-runtime-llvm"};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--dump-runtime-llvm constructs the Stage 19 runtime");
        tests.expect(output.str().find("define void @__nativesharp_start()") != std::string::npos,
            "runtime dump exposes the native entry-point function");
        tests.expect(error.str().empty(), "valid runtime dump keeps stderr empty");
    }

    const std::filesystem::path executablePath =
        std::filesystem::temp_directory_path() / "nativesharp_driver_stage19.exe";
    const std::filesystem::path defaultExecutablePath =
        std::filesystem::temp_directory_path() / "nativesharp_driver_stage19_default.exe";
    const std::filesystem::path guiExecutablePath =
        std::filesystem::temp_directory_path() / "nativesharp_driver_stage19_gui.exe";
    const std::filesystem::path invalidExecutablePath =
        std::filesystem::temp_directory_path() / "nativesharp_driver_stage19_invalid.exe";
    const std::filesystem::path unresolvedExecutablePath =
        std::filesystem::temp_directory_path() / "nativesharp_driver_stage19_unresolved.exe";
    std::error_code stage19CleanupError;
    std::filesystem::remove(executablePath, stage19CleanupError);
    std::filesystem::remove(defaultExecutablePath, stage19CleanupError);
    std::filesystem::remove(guiExecutablePath, stage19CleanupError);
    std::filesystem::remove(invalidExecutablePath, stage19CleanupError);
    std::filesystem::remove(unresolvedExecutablePath, stage19CleanupError);

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string input = validPath.string();
        const std::string destination = executablePath.string();
        const std::vector<std::string_view> arguments = {"--emit-exe", input, "-o", destination};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "--emit-exe links a valid NativeSharp Main program");
        tests.expect(std::filesystem::is_regular_file(executablePath),
            "--emit-exe creates the requested PE executable");
        tests.expect(output.str().find("PE executable emission: ok") != std::string::npos,
            "--emit-exe reports successful PE32+ emission");
        tests.expect(error.str().empty(), "successful executable generation keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string input = validPath.string();
        const std::string destination = executablePath.string();
        const std::vector<std::string_view> arguments = {"-O2", "--emit-exe", input, "-o", destination};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success, "-O2 executable generation reaches Stage 19 linking");
        tests.expect(output.str().find("optimization: O2") != std::string::npos,
            "optimized executable generation reports O2");
        tests.expect(std::filesystem::is_regular_file(executablePath),
            "optimized executable generation creates a PE artifact");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string input = validPath.string();
        const std::string destination = defaultExecutablePath.string();
        const std::vector<std::string_view> arguments = {input, "-o", destination};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "nsc <source> -o <exe> provides the Stage 19 executable shorthand");
        tests.expect(std::filesystem::is_regular_file(defaultExecutablePath),
            "executable shorthand creates the requested PE file");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string input = validPath.string();
        const std::string destination = guiExecutablePath.string();
        const std::vector<std::string_view> arguments =
            {"/SUBSYSTEM:WINDOWS", "--emit-exe", input, "-o", destination};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::Success,
            "/subsystem:windows links a Windows GUI executable case-insensitively");
        nativesharp::linker::PeExecutableVerifier verifier;
        tests.expect(verifier.verify(guiExecutablePath),
            "GUI executable emitted by the driver passes PE verification");
        tests.expect(verifier.info().subsystem == 2,
            "driver propagates /subsystem:windows to IMAGE_SUBSYSTEM_WINDOWS_GUI");
        tests.expect(output.str().find("subsystem: Windows GUI") != std::string::npos,
            "driver reports the selected Windows GUI subsystem");
        tests.expect(error.str().empty(), "successful GUI executable generation keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string input = validPath.string();
        const std::string destination = guiExecutablePath.string();
        const std::vector<std::string_view> arguments =
            {"--emit-exe", input, "/subsystem:native", "-o", destination};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CommandLineError, "unsupported executable subsystem is rejected");
        tests.expect(error.str().find("unsupported executable subsystem") != std::string::npos,
            "unsupported subsystem has a precise diagnostic");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string input = validPath.string();
        const std::string destination = guiExecutablePath.string();
        const std::vector<std::string_view> arguments = {"--emit-exe",
            input,
            "/subsystem:console",
            "/subsystem:windows",
            "-o",
            destination};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CommandLineError, "duplicate executable subsystem is rejected");
        tests.expect(error.str().find("duplicate executable subsystem") != std::string::npos,
            "duplicate subsystem has a precise diagnostic");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string input = validPath.string();
        const std::vector<std::string_view> arguments = {"--check", input, "/subsystem:windows"};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CommandLineError,
            "/subsystem is rejected for actions that do not emit an executable");
        tests.expect(error.str().find("only valid when emitting an executable") != std::string::npos,
            "non-executable subsystem use has a precise diagnostic");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string input = invalidPath.string();
        const std::string destination = invalidExecutablePath.string();
        const std::vector<std::string_view> arguments = {"--emit-exe", input, "-o", destination};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CompilationError,
            "--emit-exe rejects a semantically invalid NativeSharp program before linking");
        tests.expect(!std::filesystem::exists(invalidExecutablePath),
            "frontend failure leaves no executable artifact");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string input = unresolvedPath.string();
        const std::string destination = unresolvedExecutablePath.string();
        const std::vector<std::string_view> arguments = {"--emit-exe", input, "-o", destination};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CompilationError,
            "--emit-exe propagates an unresolved external as a link failure");
        tests.expect(error.str().find("MissingNativeSymbol") != std::string::npos,
            "driver preserves lld-link's unresolved symbol name");
        tests.expect(!std::filesystem::exists(unresolvedExecutablePath),
            "link failure leaves no executable artifact");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::string input = validPath.string();
        const std::vector<std::string_view> arguments = {"--emit-exe", input, "-o"};
        const ExitCode code = runDriver(arguments, output, error);
        tests.expect(code == ExitCode::CommandLineError,
            "incomplete --emit-exe syntax is rejected as a command-line error");
    }

    std::filesystem::remove(executablePath, stage19CleanupError);
    std::filesystem::remove(defaultExecutablePath, stage19CleanupError);
    std::filesystem::remove(guiExecutablePath, stage19CleanupError);
    std::filesystem::remove(invalidExecutablePath, stage19CleanupError);
    std::filesystem::remove(unresolvedExecutablePath, stage19CleanupError);
    std::filesystem::remove(validPath);
    std::filesystem::remove(libraryPath);
    std::filesystem::remove(invalidPath);
    std::filesystem::remove(unresolvedPath);
    std::filesystem::remove(classOnlyPath);
    std::filesystem::remove_all(projectDirectory, projectCleanupError);
    std::filesystem::remove_all(partialProjectDirectory, projectCleanupError);
    std::filesystem::remove_all(methodBodyProjectDirectory, projectCleanupError);
    std::filesystem::remove_all(constructionProjectDirectory, projectCleanupError);
    std::filesystem::remove_all(layoutProjectDirectory, projectCleanupError);
    std::filesystem::remove_all(oopProjectDirectory, projectCleanupError);

    return tests.result();
}
