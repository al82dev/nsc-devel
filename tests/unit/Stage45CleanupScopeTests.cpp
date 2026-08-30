#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModelDumper.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::MethodCleanupRegionKind;
using nativesharp::frontend::MethodStructuredExitKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name, std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic) { return diagnostic.message().find(text) != std::string::npos; });
}

void testCleanupModel(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Positive";
    writeProject(directory, "Stage45CleanupPositive", R"NS(
class CleanupFlow
{
    public static void NestedReturn(bool stop)
    {
        try
        {
            try
            {
                if (stop)
                {
                    return;
                }
            }
            finally
            {
                int inner = 1;
            }
        }
        finally
        {
            int outer = 2;
        }
    }

    public static void BreakAcrossFinally(bool run)
    {
        while (run)
        {
            try
            {
                break;
            }
            finally
            {
                int cleanup = 3;
            }
        }
    }

    public static void ContinueAcrossFinally(bool run)
    {
        for (int i = 0; run; i = i + 1)
        {
            try
            {
                continue;
            }
            finally
            {
                int cleanup = 4;
            }
        }
    }

    public static void LoopInsideOuterFinally(bool run)
    {
        try
        {
            while (run)
            {
                if (run)
                {
                    break;
                }
                continue;
            }
        }
        finally
        {
            int cleanup = 5;
        }
    }

    public static void NestedLoopBoundary(bool outer, bool inner)
    {
        while (outer)
        {
            try
            {
                while (inner)
                {
                    break;
                }
                break;
            }
            finally
            {
                int cleanup = 6;
            }
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "Stage45CleanupPositive.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 45.3 cleanup-scope sample binds without diagnostics");
    tests.expect(compilation.methodBodyModel() != nullptr, "Stage 45.3 exposes cleanup regions and exit paths");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.memberModel() == nullptr ||
        compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto& model = *compilation.methodBodyModel();
    tests.expect(model.cleanupRegions().size() == 6,
        "every finally creates one explicit cleanup region");
    tests.expect(std::ranges::all_of(model.cleanupRegions(), [](const auto& region) {
        return region.kind == MethodCleanupRegionKind::Finally &&
            region.syntax != nullptr && region.cleanupSyntax != nullptr;
    }), "cleanup regions retain protected-try and cleanup syntax anchors");
    tests.expect(std::ranges::count_if(model.cleanupRegions(), [](const auto& region) {
        return region.parentCleanupRegionOrdinal.has_value();
    }) == 1, "nested finally records exactly one parent cleanup relationship");
    tests.expect(std::ranges::any_of(model.cleanupRegions(), [](const auto& region) {
        return region.depth == 1 && region.parentCleanupRegionOrdinal.has_value();
    }), "nested cleanup depth is explicit rather than inferred during lowering");

    const auto exits = model.structuredExits();
    tests.expect(exits.size() == 7,
        "return/break/continue sites are frozen as seven structured exits");
    tests.expect(std::ranges::count_if(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Return;
    }) == 1, "return receives one structured-exit binding");
    tests.expect(std::ranges::count_if(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Break;
    }) == 4, "four break sites receive structured-exit bindings");
    tests.expect(std::ranges::count_if(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Continue;
    }) == 2, "two continue sites receive structured-exit bindings");

    const auto nestedReturn = std::ranges::find_if(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Return && exit.cleanupRegionOrdinals.size() == 2;
    });
    tests.expect(nestedReturn != exits.end(),
        "return from nested try records both pending finally regions");
    if (nestedReturn != exits.end())
    {
        tests.expect(nestedReturn->cleanupRegionOrdinals[0] == 1 &&
                nestedReturn->cleanupRegionOrdinals[1] == 0,
            "nested return cleanup order is inner-to-outer");
        tests.expect(!nestedReturn->targetScopeOrdinal.has_value(),
            "return targets the method exit rather than a lexical loop scope");
    }

    tests.expect(std::ranges::count_if(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Break && exit.cleanupRegionOrdinals.size() == 1;
    }) == 2, "break crosses cleanup only when its target loop lies outside the protected region");
    tests.expect(std::ranges::count_if(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Break && exit.cleanupRegionOrdinals.empty();
    }) == 2, "break keeps an outer finally pending when the target loop remains inside that try");
    tests.expect(std::ranges::count_if(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Continue && exit.cleanupRegionOrdinals.size() == 1;
    }) == 1, "continue crossing an inner finally records that cleanup");
    tests.expect(std::ranges::count_if(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Continue && exit.cleanupRegionOrdinals.empty();
    }) == 1, "continue within a loop nested inside try does not prematurely execute the outer finally");
    tests.expect(std::ranges::all_of(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Return || exit.targetScopeOrdinal.has_value();
    }), "break/continue carry an explicit semantic target scope");

    std::ostringstream dump;
    nativesharp::frontend::ProjectMethodBodyModelDumper{}.dump(
        model, *compilation.memberModel(), *compilation.nameLookup(), dump);
    const std::string text = dump.str();
    tests.expect(text.find("cleanup-regions=6") != std::string::npos,
        "method-body dump freezes the number of cleanup regions");
    tests.expect(text.find("structured-exits=7") != std::string::npos,
        "method-body dump freezes the number of structured exits");
    tests.expect(text.find("kind=return") != std::string::npos &&
            text.find("cleanup-path=#1,#0") != std::string::npos,
        "method-body dump exposes inner-to-outer cleanup paths didactically");
}

void testInvalidLoopExits(TestContext& tests, const std::filesystem::path& root)
{
    const auto breakDirectory = root / "BreakOutside";
    writeProject(breakDirectory, "BreakOutside", R"NS(
class InvalidFlow
{
    public static void F()
    {
        break;
    }
}
)NS");
    ProjectCompilation breakCompilation;
    std::string breakError;
    const bool breakOk = breakCompilation.compileProjectMethodBodies(
        breakDirectory / "BreakOutside.nsproj", breakError);
    tests.expect(!breakOk, "break outside a loop is rejected by Stage 45.3 sema");
    tests.expect(hasDiagnosticText(breakCompilation, "break is only valid within a loop"),
        "break outside a loop reports the structured-control-flow contract");

    const auto continueDirectory = root / "ContinueOutside";
    writeProject(continueDirectory, "ContinueOutside", R"NS(
class InvalidFlow
{
    public static void F()
    {
        continue;
    }
}
)NS");
    ProjectCompilation continueCompilation;
    std::string continueError;
    const bool continueOk = continueCompilation.compileProjectMethodBodies(
        continueDirectory / "ContinueOutside.nsproj", continueError);
    tests.expect(!continueOk, "continue outside a loop is rejected by Stage 45.3 sema");
    tests.expect(hasDiagnosticText(continueCompilation, "continue is only valid within a loop"),
        "continue outside a loop reports the structured-control-flow contract");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage45_3_cleanup_scopes";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testCleanupModel(tests, root);
    testInvalidLoopExits(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
