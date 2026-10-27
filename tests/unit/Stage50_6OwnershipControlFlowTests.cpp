#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::MethodStructuredExitKind;
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
            "</Name><OutputType>StaticLibrary</OutputType></PropertyGroup>"
            "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic)
        {
            return diagnostic.message().find(text) != std::string::npos;
        });
}

void expectInvalid(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view caseName,
    std::string_view source,
    std::string_view diagnostic)
{
    const std::filesystem::path directory = root / std::string(caseName);
    writeProject(directory, caseName, source);

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / (std::string(caseName) + ".nsproj"), error);
    tests.expect(!ok, std::string(caseName) + " is rejected by Stage 50.6 ownership control-flow analysis");
    tests.expect(
        hasDiagnosticText(compilation, diagnostic),
        std::string(caseName) + " reports the expected Stage 50.6 control-flow diagnostic");
}

std::string prefix()
{
    return R"NS(
namespace NativeSharp.Core
{
    public class Exception { }
}
using NativeSharp.Core;

class Problem : Exception { }

class Resource
{
    public int Value;
    public Resource(int value) { Value = value; }
    public int Read() { return Value; }
    ~Resource() { Value = 0; }
}

class Sink
{
    public static void Consume(owned Resource value)
    {
        int observed = value.Read();
    }
}
)NS";
}

std::string positiveSource()
{
    return prefix() + R"NS(
class Positive
{
    public static int ReturnPathDoesNotPoisonJoin(bool choose)
    {
        raii Resource owner = new Resource(1);
        if (choose)
        {
            Sink.Consume(move(owner));
            return 10;
        }
        return owner.Read();
    }

    public static int ThrowPathDoesNotPoisonJoin(bool choose) throws Problem
    {
        raii Resource owner = new Resource(2);
        if (choose)
        {
            Sink.Consume(move(owner));
            throw new Problem();
        }
        return owner.Read();
    }

    public static int OwnedParameterReturnPath(bool choose, owned Resource owner)
    {
        if (choose)
        {
            Sink.Consume(move(owner));
            return 20;
        }
        return owner.Read();
    }

    public static int WhileReturnPath(bool run)
    {
        raii Resource owner = new Resource(3);
        while (run)
        {
            Sink.Consume(move(owner));
            return 30;
        }
        return owner.Read();
    }

    public static int ForReturnPath(bool run)
    {
        raii Resource owner = new Resource(4);
        for (int i = 0; run; i = i + 1)
        {
            Sink.Consume(move(owner));
            return 40;
        }
        return owner.Read();
    }

    public static int TryCatchTerminatingPath(bool choose)
    {
        raii Resource owner = new Resource(5);
        try
        {
            if (choose)
            {
                Sink.Consume(move(owner));
                throw new Problem();
            }
        }
        catch (Problem problem)
        {
            return 60;
        }
        return owner.Read();
    }

    public static int BranchBorrowEndsBeforeMove(bool choose)
    {
        raii Resource owner = new Resource(5);
        if (choose)
        {
            borrowed Resource view = owner;
            int observed = view.Read();
        }
        Sink.Consume(move(owner));
        return 50;
    }

    public static int RoundTripAcrossBranches(bool choose)
    {
        raii Resource owner = new Resource(6);
        if (choose)
        {
            raii Resource temporary = move(owner);
            owner = move(temporary);
        }
        else
        {
            int observed = owner.Read();
        }
        return owner.Read();
    }

    public static int OverwriteAfterBorrowScope()
    {
        raii Resource owner = new Resource(7);
        {
            borrowed Resource view = owner;
            int observed = view.Read();
        }
        raii Resource replacement = new Resource(8);
        owner = move(replacement);
        return owner.Read();
    }
}
)NS";
}

void testTerminatingPaths(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "Positive";
    writeProject(directory, "Stage50_6Positive", positiveSource());

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "Stage50_6Positive.nsproj", error);
    tests.expect(ok, "Stage 50.6 accepts ownership-safe terminating control-flow paths");
    tests.expect(error.empty(), "Stage 50.6 positive control-flow project has no loader/internal error");
    tests.expect(compilation.methodBodyModel() != nullptr,
        "Stage 50.6 retains the method-body ownership model after path-sensitive analysis");
    if (!ok || compilation.methodBodyModel() == nullptr)
    {
        return;
    }

    const auto exits = compilation.methodBodyModel()->structuredExits();
    tests.expect(std::ranges::count_if(exits, [](const auto& exit)
        {
            return exit.kind == MethodStructuredExitKind::Return;
        }) >= 10,
        "Stage 50.6 positive suite preserves structured return exits while pruning them from joins");
}

void testIfJoins(TestContext& tests, const std::filesystem::path& root)
{
    expectInvalid(tests, root, "IfMaybeMoved", prefix() + R"NS(
class Case
{
    public static int Run(bool choose)
    {
        raii Resource owner = new Resource(1);
        if (choose)
        {
            Sink.Consume(move(owner));
        }
        return owner.Read();
    }
}
)NS", "possibly moved after a control-flow join");

    expectInvalid(tests, root, "IfDefinitelyMoved", prefix() + R"NS(
class Case
{
    public static int Run(bool choose)
    {
        raii Resource owner = new Resource(1);
        if (choose)
        {
            Sink.Consume(move(owner));
        }
        else
        {
            Sink.Consume(move(owner));
        }
        return owner.Read();
    }
}
)NS", "use of moved-from RAII local 'owner'");

    expectInvalid(tests, root, "OwnedParameterMaybeMoved", prefix() + R"NS(
class Case
{
    public static int Run(bool choose, owned Resource owner)
    {
        if (choose)
        {
            Sink.Consume(move(owner));
        }
        return owner.Read();
    }
}
)NS", "possibly moved after a control-flow join");

    expectInvalid(tests, root, "BorrowAfterMaybeMove", prefix() + R"NS(
class Case
{
    public static int Run(bool choose)
    {
        raii Resource owner = new Resource(1);
        if (choose)
        {
            Sink.Consume(move(owner));
        }
        borrowed Resource view = owner;
        return view.Read();
    }
}
)NS", "possibly moved after a control-flow join");

    expectInvalid(tests, root, "CatchSeesMaybeMovedOwner", prefix() + R"NS(
class Case
{
    public static int Run()
    {
        raii Resource owner = new Resource(1);
        try
        {
            Sink.Consume(move(owner));
            throw new Problem();
        }
        catch (Problem problem)
        {
            return owner.Read();
        }
    }
}
)NS", "possibly moved after a control-flow join");


    expectInvalid(tests, root, "OverwriteOwnerWhileBorrowed", prefix() + R"NS(
class Case
{
    public static int Run()
    {
        raii Resource owner = new Resource(1);
        borrowed Resource view = owner;
        raii Resource replacement = new Resource(2);
        owner = move(replacement);
        return view.Read();
    }
}
)NS", "cannot overwrite owner 'owner' while a borrowed local is alive");
}

void testLoopJoins(TestContext& tests, const std::filesystem::path& root)
{
    expectInvalid(tests, root, "WhileBreakMaybeMoved", prefix() + R"NS(
class Case
{
    public static int Run(bool run)
    {
        raii Resource owner = new Resource(1);
        while (run)
        {
            Sink.Consume(move(owner));
            break;
        }
        return owner.Read();
    }
}
)NS", "possibly moved after a control-flow join");

    expectInvalid(tests, root, "WhileContinueBackEdge", prefix() + R"NS(
class Case
{
    public static void Run(bool run)
    {
        raii Resource owner = new Resource(1);
        while (run)
        {
            Sink.Consume(move(owner));
            continue;
        }
    }
}
)NS", "may remain moved across a while-loop back-edge");

    expectInvalid(tests, root, "ForBreakMaybeMoved", prefix() + R"NS(
class Case
{
    public static int Run(bool run)
    {
        raii Resource owner = new Resource(1);
        for (int i = 0; run; i = i + 1)
        {
            Sink.Consume(move(owner));
            break;
        }
        return owner.Read();
    }
}
)NS", "possibly moved after a control-flow join");

    expectInvalid(tests, root, "ForContinueBackEdge", prefix() + R"NS(
class Case
{
    public static void Run(bool run)
    {
        raii Resource owner = new Resource(1);
        for (int i = 0; run; i = i + 1)
        {
            Sink.Consume(move(owner));
            continue;
        }
    }
}
)NS", "may remain moved across a for-loop back-edge");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage50_6_control_flow";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testTerminatingPaths(tests, root);
    testIfJoins(tests, root);
    testLoopJoins(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
