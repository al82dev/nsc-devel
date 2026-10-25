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
using nativesharp::frontend::MethodExceptionOperationKind;
using nativesharp::frontend::MethodScopeKind;
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

constexpr std::string_view kCore = R"NS(
namespace NativeSharp.Core
{
    public class Exception { }
    public class IOException : Exception { }
    public class FileException : IOException { }
    public class Other { }
}
using NativeSharp.Core;
)NS";

void testPositiveSema(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Positive";
    writeProject(directory, "Stage45SemaPositive", std::string(kCore) + R"NS(
class Handler
{
    public static void Run(FileException file)
    {
        try
        {
            throw new FileException();
        }
        catch (FileException specific)
        {
            Exception captured = specific;
            throw;
        }
        catch (IOException io)
        {
            Exception capturedIo = io;
        }
        catch (Exception general)
        {
            Exception capturedGeneral = general;
        }
        catch
        {
            int marker = 1;
        }

        try
        {
            int work = 2;
        }
        finally
        {
            int cleanup = 3;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(directory / "Stage45SemaPositive.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 45.2 valid exception semantics reach method-body binding");
    tests.expect(compilation.methodBodyModel() != nullptr, "Stage 45.2 exposes method-body exception bindings");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.memberModel() == nullptr ||
        compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto bindings = compilation.methodBodyModel()->exceptionBindings();
    tests.expect(bindings.size() == 6, "four catches, one throw, and one rethrow are bound explicitly");
    tests.expect(std::ranges::count_if(bindings, [](const auto& binding) {
        return binding.kind == MethodExceptionOperationKind::Catch;
    }) == 4, "all catch clauses receive semantic bindings");
    tests.expect(std::ranges::count_if(bindings, [](const auto& binding) {
        return binding.kind == MethodExceptionOperationKind::Throw;
    }) == 1, "throw expression receives a semantic binding");
    tests.expect(std::ranges::count_if(bindings, [](const auto& binding) {
        return binding.kind == MethodExceptionOperationKind::Rethrow;
    }) == 1, "throw; receives a rethrow semantic binding");
    tests.expect(std::ranges::count_if(compilation.methodBodyModel()->scopes(), [](const auto& scope) {
        return scope.kind == MethodScopeKind::Catch;
    }) == 4, "every catch owns an explicit lexical scope");
    tests.expect(std::ranges::any_of(compilation.methodBodyModel()->locals(), [](const auto& local) {
        return local.name == "specific" && local.typeKey.find("FileException") != std::string::npos;
    }), "typed catch variable is materialized as a scoped local");

    std::ostringstream dump;
    nativesharp::frontend::ProjectMethodBodyModelDumper{}.dump(
        *compilation.methodBodyModel(), *compilation.memberModel(), *compilation.nameLookup(), dump);
    tests.expect(dump.str().find("exception-bindings=6") != std::string::npos,
        "method-body dumper freezes Stage 45.2 exception binding evidence");
    tests.expect(dump.str().find("kind=rethrow") != std::string::npos,
        "method-body dumper distinguishes rethrow from throw expression");
}

void expectReject(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view body,
    std::string_view expected)
{
    const auto directory = root / std::string(name);
    writeProject(directory, name, std::string(kCore) + std::string(body));
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / (std::string(name) + ".nsproj"), error);
    tests.expect(!ok, std::string(name) + " is rejected by Stage 45.2 sema");
    tests.expect(hasDiagnosticText(compilation, expected),
        std::string(name) + " reports the precise exception semantic contract");
}

void testNegativeSema(TestContext& tests, const std::filesystem::path& root)
{
    expectReject(tests, root, "ThrowInt", R"NS(
class H { public static void F() { throw 42; } }
)NS", "throw operand type 'builtin:int'");

    expectReject(tests, root, "ThrowNonException", R"NS(
class H { public static void F(Other value) { throw value; } }
)NS", "must be NativeSharp.Core.Exception or derive from it");

    expectReject(tests, root, "RethrowOutsideCatch", R"NS(
class H { public static void F() { throw; } }
)NS", "throw; is only valid within a catch block");

    expectReject(tests, root, "CatchBuiltin", R"NS(
class H { public static void F() { try { int x = 0; } catch (int value) { } } }
)NS", "catch type 'builtin:int'");

    expectReject(tests, root, "CatchNonException", R"NS(
class H { public static void F() { try { int x = 0; } catch (Other value) { } } }
)NS", "must be NativeSharp.Core.Exception or derive from it");

    expectReject(tests, root, "BaseBeforeDerived", R"NS(
class H
{
    public static void F()
    {
        try { int x = 0; }
        catch (Exception general) { }
        catch (IOException derived) { }
    }
}
)NS", "unreachable because an earlier catch handles the same or a base exception type");

    expectReject(tests, root, "CatchAllBeforeTyped", R"NS(
class H
{
    public static void F()
    {
        try { int x = 0; }
        catch { }
        catch (Exception value) { }
    }
}
)NS", "previous catch-all handles every exception");

    expectReject(tests, root, "BareTry", R"NS(
class H { public static void F() { try { int x = 0; } } }
)NS", "requires at least one catch clause or a finally block");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage45_2_exception_sema";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testPositiveSema(tests, root);
    testNegativeSema(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
