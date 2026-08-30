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
using nativesharp::frontend::MethodCleanupContinuationKind;
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

void testFinallyContinuationModel(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Positive";
    writeProject(directory, "Stage45FinallyPositive", R"NS(
public delegate int Transform(int value);

class FinallyFlow
{
    public static void Normal()
    {
        try
        {
            int body = 1;
        }
        finally
        {
            int cleanup = 2;
        }
        int after = 3;
    }

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
                int inner = 4;
            }
        }
        finally
        {
            int outer = 5;
        }
    }

    public static void BreakAcross(bool run)
    {
        while (run)
        {
            try
            {
                break;
            }
            finally
            {
                int cleanup = 6;
            }
        }
    }

    public static void ContinueAcross(bool run)
    {
        while (run)
        {
            try
            {
                continue;
            }
            finally
            {
                int cleanup = 7;
            }
        }
    }

    public static void BreakInsideFinallyLoop(bool run)
    {
        try
        {
            int body = 8;
        }
        finally
        {
            while (run)
            {
                break;
            }
        }
    }

    public static void ContinueInsideFinallyLoop(bool run)
    {
        try
        {
            int body = 9;
        }
        finally
        {
            while (run)
            {
                continue;
            }
        }
    }

    public static void LambdaReturnInsideFinally()
    {
        try
        {
            int body = 10;
        }
        finally
        {
            Transform transform = x => { return x + 1; };
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "Stage45FinallyPositive.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 45.4 finally-flow sample binds without diagnostics");
    tests.expect(compilation.methodBodyModel() != nullptr,
        "Stage 45.4 exposes explicit cleanup continuations");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.memberModel() == nullptr ||
        compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto& model = *compilation.methodBodyModel();
    const auto continuations = model.cleanupContinuations();
    tests.expect(model.cleanupRegions().size() == 8,
        "eight finally blocks create eight cleanup regions");
    tests.expect(continuations.size() == 12,
        "normal and structured exits materialize twelve per-finally continuation contracts");
    tests.expect(std::ranges::count_if(continuations, [](const auto& continuation) {
        return continuation.kind == MethodCleanupContinuationKind::Normal;
    }) == 8, "every finally has an explicit normal-completion continuation");
    tests.expect(std::ranges::count_if(continuations, [](const auto& continuation) {
        return continuation.kind == MethodCleanupContinuationKind::Return;
    }) == 2, "nested return carries its reason through both finally regions");
    tests.expect(std::ranges::count_if(continuations, [](const auto& continuation) {
        return continuation.kind == MethodCleanupContinuationKind::Break;
    }) == 1, "break crossing finally creates one cleanup continuation");
    tests.expect(std::ranges::count_if(continuations, [](const auto& continuation) {
        return continuation.kind == MethodCleanupContinuationKind::Continue;
    }) == 1, "continue crossing finally creates one cleanup continuation");
    tests.expect(std::ranges::none_of(continuations, [](const auto& continuation) {
        return continuation.kind == MethodCleanupContinuationKind::Exception;
    }), "exception is reserved as an exit reason but is not fabricated before exceptional-flow binding");

    const auto firstReturn = std::ranges::find_if(continuations, [](const auto& continuation) {
        return continuation.kind == MethodCleanupContinuationKind::Return &&
            continuation.nextCleanupRegionOrdinal.has_value();
    });
    tests.expect(firstReturn != continuations.end(),
        "inner finally resumes the return reason by naming the next outer cleanup");
    if (firstReturn != continuations.end())
    {
        const auto secondReturn = std::ranges::find_if(continuations, [&](const auto& continuation) {
            return continuation.kind == MethodCleanupContinuationKind::Return &&
                continuation.cleanupRegionOrdinal == *firstReturn->nextCleanupRegionOrdinal;
        });
        tests.expect(secondReturn != continuations.end(),
            "the next cleanup named by the inner continuation exists explicitly");
        if (secondReturn != continuations.end())
        {
            tests.expect(!secondReturn->nextCleanupRegionOrdinal.has_value(),
                "outermost return continuation ends the cleanup chain");
            tests.expect(!secondReturn->targetScopeOrdinal.has_value(),
                "outermost return continuation resumes at method exit");
        }
    }

    const auto exits = model.structuredExits();
    tests.expect(std::ranges::count_if(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Break && exit.cleanupRegionOrdinals.empty();
    }) == 1, "break whose loop is wholly inside finally is legal and requires no recursive cleanup");
    tests.expect(std::ranges::count_if(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Continue && exit.cleanupRegionOrdinals.empty();
    }) == 1, "continue whose loop is wholly inside finally is legal and requires no recursive cleanup");

    std::ostringstream dump;
    nativesharp::frontend::ProjectMethodBodyModelDumper{}.dump(
        model, *compilation.memberModel(), *compilation.nameLookup(), dump);
    const std::string text = dump.str();
    tests.expect(text.find("cleanup-continuations=12") != std::string::npos,
        "dumper freezes the cleanup-continuation count");
    tests.expect(text.find("reason=normal") != std::string::npos,
        "dumper exposes normal finally continuation");
    tests.expect(text.find("reason=return") != std::string::npos &&
            text.find("next=#") != std::string::npos,
        "dumper exposes return reason and chained cleanup target");
    tests.expect(text.find("reason=break") != std::string::npos &&
            text.find("reason=continue") != std::string::npos,
        "dumper exposes structured loop-exit reasons");
    tests.expect(std::ranges::none_of(exits, [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Return && exit.cleanupRegionOrdinals.empty();
    }), "return inside a lambda nested lexically in finally belongs to the lambda, not the enclosing finally");
    tests.expect(std::string(nativesharp::frontend::methodCleanupContinuationKindName(
        MethodCleanupContinuationKind::Exception)) == "exception",
        "exception continuation reason is reserved explicitly for later EH integration");
}

void testReturnCannotLeaveFinally(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "ReturnLeavesFinally";
    writeProject(directory, "ReturnLeavesFinally", R"NS(
class InvalidFinally
{
    public static void F()
    {
        try
        {
            int body = 1;
        }
        finally
        {
            return;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(directory / "ReturnLeavesFinally.nsproj", error);
    tests.expect(!ok, "return leaving finally is rejected");
    tests.expect(hasDiagnosticText(compilation, "return cannot leave a finally block"),
        "return leaving finally reports the dedicated finally-control diagnostic");
}

void testLoopExitCannotLeaveFinally(TestContext& tests, const std::filesystem::path& root)
{
    const auto breakDirectory = root / "BreakLeavesFinally";
    writeProject(breakDirectory, "BreakLeavesFinally", R"NS(
class InvalidFinally
{
    public static void F(bool run)
    {
        while (run)
        {
            try
            {
                int body = 1;
            }
            finally
            {
                break;
            }
        }
    }
}
)NS");
    ProjectCompilation breakCompilation;
    std::string breakError;
    const bool breakOk = breakCompilation.compileProjectMethodBodies(
        breakDirectory / "BreakLeavesFinally.nsproj", breakError);
    tests.expect(!breakOk, "break cannot jump from finally to a loop established outside it");
    tests.expect(hasDiagnosticText(breakCompilation, "break cannot leave a finally block"),
        "break leaving finally reports the dedicated control-transfer diagnostic");

    const auto continueDirectory = root / "ContinueLeavesFinally";
    writeProject(continueDirectory, "ContinueLeavesFinally", R"NS(
class InvalidFinally
{
    public static void F(bool run)
    {
        while (run)
        {
            try
            {
                int body = 1;
            }
            finally
            {
                continue;
            }
        }
    }
}
)NS");
    ProjectCompilation continueCompilation;
    std::string continueError;
    const bool continueOk = continueCompilation.compileProjectMethodBodies(
        continueDirectory / "ContinueLeavesFinally.nsproj", continueError);
    tests.expect(!continueOk, "continue cannot jump from finally to a loop established outside it");
    tests.expect(hasDiagnosticText(continueCompilation, "continue cannot leave a finally block"),
        "continue leaving finally reports the dedicated control-transfer diagnostic");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage45_4_finally_flow";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testFinallyContinuationModel(tests, root);
    testReturnCannotLeaveFinally(tests, root);
    testLoopExitCannotLeaveFinally(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
