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
using nativesharp::frontend::MethodBorrowKind;
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
    tests.expect(!ok, std::string(caseName) + " is rejected by Stage 50.7 escape analysis");
    tests.expect(
        hasDiagnosticText(compilation, diagnostic),
        std::string(caseName) + " reports the expected Stage 50.7 escape diagnostic");
}

std::string prefix()
{
    return R"NS(
class Resource
{
    public int Value;
    public Resource(int value) { Value = value; }
    public int Read() { return Value; }
    ~Resource() { Value = 0; }
}

public delegate int Reader();

class BorrowApi
{
    public static int Inspect(borrowed Resource value)
    {
        return value.Read();
    }

    public static void Unqualified(Resource value)
    {
    }
}
)NS";
}

void testSafeBorrowedReturns(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "SafeReturns";
    writeProject(directory, "Stage50_7SafeReturns", prefix() + R"NS(
class SafeReturns
{
    public static borrowed Resource ForwardParameter(borrowed Resource input)
    {
        return input;
    }

    public static borrowed Resource ForwardLocal(borrowed Resource input)
    {
        borrowed Resource view = input;
        return view;
    }

    public static borrowed Resource ForwardCall(borrowed Resource input)
    {
        return ForwardParameter(input);
    }

    public static borrowed Resource NullView()
    {
        return null;
    }

    public static int Observe(borrowed Resource input)
    {
        borrowed Resource view = input;
        BorrowApi.Inspect(view);
        return view.Read();
    }

    public static int OrdinaryClosureStillWorks(int seed)
    {
        Reader reader = () => seed;
        return reader();
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "Stage50_7SafeReturns.nsproj", error);
    tests.expect(ok, "Stage 50.7 accepts nonescaping borrows and lifetime-preserving borrowed returns");
    tests.expect(error.empty(), "Stage 50.7 safe-return project has no loader/internal error");
    tests.expect(compilation.methodBodyModel() != nullptr,
        "Stage 50.7 exposes escape-safe borrow bindings in the method-body model");
    if (!ok || compilation.methodBodyModel() == nullptr)
    {
        return;
    }

    const auto borrows = compilation.methodBodyModel()->borrowBindings();
    tests.expect(std::ranges::any_of(borrows, [](const auto& borrow)
        {
            return borrow.kind == MethodBorrowKind::BorrowedParameterToBorrowedReturn;
        }), "borrowed parameter -> borrowed return is represented explicitly");
    tests.expect(std::ranges::any_of(borrows, [](const auto& borrow)
        {
            return borrow.kind == MethodBorrowKind::BorrowedLocalToBorrowedReturn;
        }), "borrowed local rooted in a borrowed parameter can escape through a borrowed return");
    tests.expect(std::ranges::any_of(borrows, [](const auto& borrow)
        {
            return borrow.kind == MethodBorrowKind::BorrowedReturnForward;
        }), "borrowed-returning calls can be forwarded without manufacturing ownership");
}

void testReturnEscapes(TestContext& tests, const std::filesystem::path& root)
{
    expectInvalid(tests, root, "LocalOwnerDirectReturn", prefix() + R"NS(
class Case
{
    public static borrowed Resource Leak()
    {
        raii Resource owner = new Resource(1);
        return owner;
    }
}
)NS", "borrowed return value outlives local owner 'owner'");

    expectInvalid(tests, root, "LocalBorrowReturn", prefix() + R"NS(
class Case
{
    public static borrowed Resource Leak()
    {
        raii Resource owner = new Resource(1);
        borrowed Resource view = owner;
        return view;
    }
}
)NS", "borrowed return value 'view' outlives its owner 'owner'");

    expectInvalid(tests, root, "OwnedParameterDirectReturn", prefix() + R"NS(
class Case
{
    public static borrowed Resource Leak(owned Resource owner)
    {
        return owner;
    }
}
)NS", "borrowed return value refers to owned parameter 'owner'");

    expectInvalid(tests, root, "OwnedParameterBorrowReturn", prefix() + R"NS(
class Case
{
    public static borrowed Resource Leak(owned Resource owner)
    {
        borrowed Resource view = owner;
        return view;
    }
}
)NS", "borrowed return value 'view' refers to owned parameter 'owner'");

    expectInvalid(tests, root, "UnqualifiedReturn", prefix() + R"NS(
class Case
{
    public static Resource Leak(borrowed Resource input)
    {
        return input;
    }
}
)NS", "borrowed value cannot escape through an unqualified return type");
}

void testStorageAndCallEscapes(TestContext& tests, const std::filesystem::path& root)
{
    expectInvalid(tests, root, "UnqualifiedCall", prefix() + R"NS(
class Case
{
    public static void Run(borrowed Resource input)
    {
        BorrowApi.Unqualified(input);
    }
}
)NS", "borrowed argument requires a borrowed parameter");

    expectInvalid(tests, root, "UnqualifiedLocalInitialization", prefix() + R"NS(
class Case
{
    public static void Run(borrowed Resource input)
    {
        Resource copy = input;
    }
}
)NS", "borrowed value cannot be stored in unqualified local 'copy'");

    expectInvalid(tests, root, "UnqualifiedLocalAssignment", prefix() + R"NS(
class Case
{
    public static void Run(borrowed Resource input)
    {
        Resource copy = null;
        copy = input;
    }
}
)NS", "borrowed value cannot be stored in unqualified local 'copy'");

    expectInvalid(tests, root, "InstanceFieldStore", prefix() + R"NS(
class Case
{
    public Resource Cache;

    public void Store(borrowed Resource input)
    {
        Cache = input;
    }
}
)NS", "borrowed value cannot be stored in field 'Cache'");

    expectInvalid(tests, root, "StaticFieldStore", prefix() + R"NS(
class Case
{
    public static Resource Cache;

    public static void Store(borrowed Resource input)
    {
        Cache = input;
    }
}
)NS", "borrowed value cannot be stored in field 'Cache'");

    // Accepting a full-expression string borrow at a constructor boundary must not make the
    // parameter storable in the constructed object. The ordinary escape rule remains in force.
    expectInvalid(tests, root, "ConstructorFieldStore", R"NS(
class Case
{
    public string Cache;

    public Case(borrowed string input)
    {
        Cache = input;
    }
}
)NS", "borrowed value cannot be stored in field 'Cache'");

    expectInvalid(tests, root, "BorrowedFieldDeclaration", prefix() + R"NS(
class Case
{
    public borrowed Resource View;
}
)NS", "borrowed field 'View' is rejected in Stage 50");
}

void testClosureEscapes(TestContext& tests, const std::filesystem::path& root)
{
    expectInvalid(tests, root, "CaptureBorrowedParameter", prefix() + R"NS(
class Case
{
    public static Reader Make(borrowed Resource input)
    {
        Reader reader = () => input.Read();
        return reader;
    }
}
)NS", "borrowed parameter 'input' cannot be captured by a lambda");

    expectInvalid(tests, root, "CaptureBorrowedLocal", prefix() + R"NS(
class Case
{
    public static Reader Make(borrowed Resource input)
    {
        borrowed Resource view = input;
        Reader reader = () => view.Read();
        return reader;
    }
}
)NS", "borrowed local 'view' cannot be captured by a lambda");

    expectInvalid(tests, root, "CaptureRaiiOwner", prefix() + R"NS(
class Case
{
    public static Reader Make()
    {
        raii Resource owner = new Resource(1);
        Reader reader = () => owner.Read();
        return reader;
    }
}
)NS", "RAII owner 'owner' cannot be captured by a lambda");

    expectInvalid(tests, root, "CaptureOwnedParameter", prefix() + R"NS(
class Case
{
    public static Reader Make(owned Resource owner)
    {
        Reader reader = () => owner.Read();
        return reader;
    }
}
)NS", "owned parameter 'owner' cannot be captured by a lambda");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage50_7_borrow_escape";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testSafeBorrowedReturns(tests, root);
    testReturnEscapes(tests, root);
    testStorageAndCallEscapes(tests, root);
    testClosureEscapes(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
