#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModelDumper.h"
#include "nativesharp/Sema/OwnershipKind.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::MethodBorrowKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::sema::OwnershipKind;
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
    tests.expect(!ok, std::string(caseName) + " is rejected by Stage 50.4 borrowed-parameter semantics");
    tests.expect(
        hasDiagnosticText(compilation, diagnostic),
        std::string(caseName) + " reports the expected Stage 50.4 diagnostic");
}

std::string resourcePrefix()
{
    return R"NS(
class Resource
{
    public int Value;
    public Resource(int value) { Value = value; }
    public int Read() { return Value; }
    ~Resource() { Value = 0; }
}
)NS";
}

std::string positiveSource()
{
    return resourcePrefix() + R"NS(
class Api
{
    public static void Inspect(borrowed Resource value)
    {
        int observed = value.Read();
    }

    public static void ForwardBorrow(borrowed Resource value)
    {
        Inspect(value);
    }

    public static void BorrowOwned(owned Resource value)
    {
        Inspect(value);
        Consume(move(value));
    }

    public static void Consume(owned Resource value)
    {
        int observed = value.Read();
    }

    public static int Route(borrowed Resource value)
    {
        return 1;
    }

    public static int Route(owned Resource value)
    {
        return 2;
    }

    public static int Run()
    {
        raii Resource first = new Resource(1);
        Inspect(first);
        int stillAlive = first.Read();

        raii Resource second = new Resource(2);
        int borrowedRoute = Route(second);
        int ownedRoute = Route(move(second));

        raii Resource third = new Resource(3);
        BorrowOwned(move(third));
        return stillAlive + borrowedRoute + ownedRoute;
    }
}
)NS";
}

void testPositiveBorrowCalls(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "Positive";
    writeProject(directory, "Stage50_4Positive", positiveSource());

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "Stage50_4Positive.nsproj", error);
    tests.expect(ok, "Stage 50.4 positive borrowed-parameter sample binds successfully");
    tests.expect(error.empty(), "Stage 50.4 positive sample has no loader/internal error");
    tests.expect(compilation.methodBodyModel() != nullptr && compilation.memberModel() != nullptr &&
            compilation.nameLookup() != nullptr,
        "Stage 50.4 exposes semantic models for borrow inspection");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.memberModel() == nullptr ||
        compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto& body = *compilation.methodBodyModel();
    const auto borrows = body.borrowBindings();
    tests.expect(std::ranges::count_if(borrows, [](const auto& borrow)
        {
            return borrow.kind == MethodBorrowKind::OwnerLocalToBorrowedParameter;
        }) == 2,
        "two RAII owners are borrowed temporarily by borrowed parameters");
    tests.expect(std::ranges::count_if(borrows, [](const auto& borrow)
        {
            return borrow.kind == MethodBorrowKind::BorrowedParameterToBorrowedParameter;
        }) == 1,
        "a borrowed parameter can be forwarded to another borrowed parameter");
    tests.expect(std::ranges::count_if(borrows, [](const auto& borrow)
        {
            return borrow.kind == MethodBorrowKind::OwnedParameterToBorrowedParameter;
        }) == 1,
        "an owned parameter can be observed through a borrowed call without transfer");
    tests.expect(std::ranges::all_of(borrows, [](const auto& borrow)
        {
            return borrow.targetMemberOrdinal.has_value() &&
                borrow.targetParameterOrdinal.has_value() && !borrow.typeKey.empty();
        }),
        "every Stage 50.4 borrow records its semantic target and physical type");

    const auto& members = compilation.memberModel()->members();
    tests.expect(std::ranges::any_of(members, [](const auto& member)
        {
            return member.name == "Inspect" && !member.parameters.empty() &&
                member.parameters.front().ownership == OwnershipKind::Borrowed;
        }),
        "borrowed parameter contract remains visible in the member model");

    std::ostringstream dump;
    nativesharp::frontend::ProjectMethodBodyModelDumper{}.dump(
        body, *compilation.memberModel(), *compilation.nameLookup(), dump);
    const std::string text = dump.str();
    tests.expect(text.find("borrow-bindings=4") != std::string::npos,
        "method-body dump freezes the four Stage 50.4 borrow edges");
    tests.expect(text.find("owner-local-to-borrowed-parameter") != std::string::npos &&
            text.find("owned-parameter-to-borrowed-parameter") != std::string::npos &&
            text.find("borrowed-parameter-to-borrowed-parameter") != std::string::npos,
        "method-body dump exposes Stage 50.4 borrow categories");
}

void testFreshOwnedStringTemporaryBorrow(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "FreshOwnedStringTemporary";
    writeProject(directory, "FreshOwnedStringTemporary", R"NS(
class Api
{
    public static int Inspect(borrowed string value) { return 2; }
    public static int InspectLegacy(string value) { return 3; }
    public static int Run()
    {
        return Inspect("a" + "b") + InspectLegacy("c" + "d");
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "FreshOwnedStringTemporary.nsproj", error);
    tests.expect(ok, "a fresh owned string temporary may be borrowed for one complete call expression");
    tests.expect(error.empty(), "fresh owned string temporary borrow has no loader/internal error");
    if (!ok || compilation.methodBodyModel() == nullptr) return;
    tests.expect(std::ranges::count_if(
        compilation.methodBodyModel()->borrowBindings(), [](const auto& borrow)
        {
            return borrow.kind == MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter;
        }) == 2,
        "explicit and legacy non-retaining string parameters record their temporary borrows");
}

void testNegativeBorrowCalls(TestContext& tests, const std::filesystem::path& root)
{
    const std::string prefix = resourcePrefix();

    expectInvalid(tests, root, "BorrowedToOwned", prefix + R"NS(
class Api
{
    public static void Consume(owned Resource value) { }
    public static void Forward(borrowed Resource view) { Consume(view); }
}
)NS", "cannot convert borrowed value 'view' to owned parameter");

    expectInvalid(tests, root, "MoveBorrowed", prefix + R"NS(
class Api
{
    public static void Consume(owned Resource value) { }
    public static void Forward(borrowed Resource view) { Consume(move(view)); }
}
)NS", "move(...) source must be a RAII local or owned parameter");

    expectInvalid(tests, root, "UncheckedToBorrowed", prefix + R"NS(
class Api
{
    public static void Inspect(borrowed Resource value) { }
    public static void Forward(Resource value) { Inspect(value); }
}
)NS", "borrowed parameter 'value' requires a checked owner or borrowed source");

    expectInvalid(tests, root, "MoveIntoBorrowed", prefix + R"NS(
class Api
{
    public static void Inspect(borrowed Resource value) { }
    public static void Run()
    {
        raii Resource owner = new Resource(1);
        Inspect(move(owner));
    }
}
)NS", "cannot transfer ownership with move(...) into a borrowed parameter");

    expectInvalid(tests, root, "BorrowAfterMove", prefix + R"NS(
class Api
{
    public static void Consume(owned Resource value) { }
    public static void Inspect(borrowed Resource value) { }
    public static void Run()
    {
        raii Resource owner = new Resource(1);
        Consume(move(owner));
        Inspect(owner);
    }
}
)NS", "use of moved-from RAII local 'owner'");

    expectInvalid(tests, root, "TemporaryToBorrowed", prefix + R"NS(
class Api
{
    public static void Inspect(borrowed Resource value) { }
    public static void Run() { Inspect(new Resource(1)); }
}
)NS", "borrowed parameter 'value' requires a checked owner or borrowed source");

}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage50_4_borrowed_parameters";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testPositiveBorrowCalls(tests, root);
    testFreshOwnedStringTemporaryBorrow(tests, root);
    testNegativeBorrowCalls(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
