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
using nativesharp::frontend::MethodBorrowState;
using nativesharp::frontend::MethodLocalBorrowSourceKind;
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
    tests.expect(!ok, std::string(caseName) + " is rejected by Stage 50.5 lexical borrow semantics");
    tests.expect(
        hasDiagnosticText(compilation, diagnostic),
        std::string(caseName) + " reports the expected Stage 50.5 diagnostic");
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

    public static int FromOwned(owned Resource input)
    {
        borrowed Resource view = input;
        Inspect(view);
        return view.Read();
    }

    public static int FromBorrowed(borrowed Resource input)
    {
        borrowed Resource view = input;
        Inspect(view);
        return view.Read();
    }

    public static int MoveAfterNestedBorrow()
    {
        raii Resource owner = new Resource(11);
        {
            borrowed Resource view = owner;
            Inspect(view);
        }
        Consume(move(owner));
        return 1;
    }

    public static void Consume(owned Resource value)
    {
        int observed = value.Read();
    }

    public static int Run()
    {
        raii Resource owner = new Resource(7);
        borrowed Resource outer = owner;
        Inspect(outer);

        int nested = 0;
        {
            borrowed Resource inner = outer;
            Inspect(inner);
            nested = inner.Read();
        }

        int afterNested = outer.Read();
        return nested + afterNested + FromBorrowed(outer) + MoveAfterNestedBorrow();
    }
}
)NS";
}

void testPositiveLocalBorrows(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "Positive";
    writeProject(directory, "Stage50_5Positive", positiveSource());

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "Stage50_5Positive.nsproj", error);
    tests.expect(ok, "Stage 50.5 local-borrow sample binds successfully");
    tests.expect(error.empty(), "Stage 50.5 positive sample has no loader/internal error");
    tests.expect(compilation.methodBodyModel() != nullptr,
        "Stage 50.5 exposes the method-body lifetime model");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.memberModel() == nullptr ||
        compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto& body = *compilation.methodBodyModel();
    const auto localBorrows = body.localBorrowBindings();
    tests.expect(localBorrows.size() == 5,
        "five persistent local borrows are tracked explicitly");
    tests.expect(std::ranges::count_if(localBorrows, [](const auto& borrow)
        {
            return borrow.sourceKind == MethodLocalBorrowSourceKind::OwnerLocal;
        }) == 2,
        "owner-local to borrowed-local creation is represented, including a nested lexical borrow");
    tests.expect(std::ranges::count_if(localBorrows, [](const auto& borrow)
        {
            return borrow.sourceKind == MethodLocalBorrowSourceKind::OwnedParameter;
        }) == 1,
        "owned-parameter to borrowed-local creation is represented");
    tests.expect(std::ranges::count_if(localBorrows, [](const auto& borrow)
        {
            return borrow.sourceKind == MethodLocalBorrowSourceKind::BorrowedParameter;
        }) == 1,
        "borrowed-parameter to borrowed-local creation is represented");
    tests.expect(std::ranges::count_if(localBorrows, [](const auto& borrow)
        {
            return borrow.sourceKind == MethodLocalBorrowSourceKind::BorrowedLocal;
        }) == 1,
        "nested borrowed-local forwarding is represented");
    tests.expect(std::ranges::all_of(localBorrows, [](const auto& borrow)
        {
            return borrow.state == MethodBorrowState::Valid;
        }),
        "all accepted Stage 50.5 local borrows finish semantic binding as valid");
    tests.expect(std::ranges::all_of(localBorrows, [&](const auto& borrow)
        {
            const auto& scopes = body.scopes();
            if (borrow.creationScopeOrdinal >= scopes.size() ||
                borrow.maximumValidScopeOrdinal >= scopes.size()) return false;
            return scopes[borrow.creationScopeOrdinal].depth >=
                scopes[borrow.maximumValidScopeOrdinal].depth;
        }),
        "borrow creation regions never exceed their maximum owner lifetime region");

    const auto& locals = body.locals();
    tests.expect(std::ranges::count_if(locals, [](const auto& local)
        {
            return local.ownership == OwnershipKind::Borrowed &&
                local.localBorrowBindingOrdinal.has_value();
        }) == 5,
        "borrowed locals link back to their lifetime bindings");

    tests.expect(std::ranges::any_of(body.borrowBindings(), [](const auto& borrow)
        {
            return nativesharp::frontend::methodBorrowKindName(borrow.kind) ==
                std::string_view("borrowed-local-to-borrowed-parameter");
        }),
        "borrowed locals can be passed to borrowed parameters without losing provenance");

    std::ostringstream dump;
    nativesharp::frontend::ProjectMethodBodyModelDumper{}.dump(
        body, *compilation.memberModel(), *compilation.nameLookup(), dump);
    const std::string text = dump.str();
    tests.expect(text.find("local-borrow-bindings=5") != std::string::npos,
        "method-body dump freezes the five lexical borrow bindings");
    tests.expect(text.find("source-kind=borrowed-local") != std::string::npos &&
            text.find("state=valid") != std::string::npos &&
            text.find("max-scope=") != std::string::npos,
        "method-body dump exposes source, state, and maximum lifetime region");
}

void testNegativeLocalBorrows(TestContext& tests, const std::filesystem::path& root)
{
    const std::string prefix = resourcePrefix();

    expectInvalid(tests, root, "MoveWhileBorrowed", prefix + R"NS(
class Api
{
    public static void Consume(owned Resource value) { }
    public static void Run()
    {
        raii Resource owner = new Resource(1);
        borrowed Resource view = owner;
        Consume(move(owner));
    }
}
)NS", "cannot move owner 'owner' while a borrowed local is alive");

    expectInvalid(tests, root, "MoveOwnedParameterWhileBorrowed", prefix + R"NS(
class Api
{
    public static void Consume(owned Resource value) { }
    public static void Forward(owned Resource owner)
    {
        borrowed Resource view = owner;
        Consume(move(owner));
    }
}
)NS", "cannot move owned parameter 'owner' while a borrowed local is alive");

    expectInvalid(tests, root, "PartialFieldMoveWhileOwnerBorrowed", prefix + R"NS(
class Holder
{
    public owned Resource slot;
    public Holder(owned Resource value) { slot = move(value); }
}
class Api
{
    public static void Run()
    {
        raii Holder owner = new Holder(new Resource(1));
        borrowed Holder view = owner;
        raii Resource extracted = move(owner.slot);
    }
}
)NS", "cannot partially move from owner 'owner' while a borrowed local is alive");

    expectInvalid(tests, root, "PartialFieldMoveFromBorrowedOwnedParameter", prefix + R"NS(
class Holder
{
    public owned Resource slot;
    public Holder(owned Resource value) { slot = move(value); }
}
class Api
{
    public static owned Resource Take(owned Holder owner)
    {
        borrowed Holder view = owner;
        return move(owner.slot);
    }
}
)NS", "cannot partially move from owned parameter 'owner' while a borrowed local is alive");

    expectInvalid(tests, root, "PartialElementMoveWhileOwnerBorrowed", prefix + R"NS(
class Holder
{
    public owned Resource[] items;
    public Holder()
    {
        items = new Resource[1];
        items[0] = new Resource(1);
    }
}
class Api
{
    public static void Run()
    {
        raii Holder owner = new Holder();
        borrowed Holder view = owner;
        raii Resource extracted = move(owner.items[0]);
    }
}
)NS", "cannot partially move from owner 'owner' while a borrowed local is alive");

    expectInvalid(tests, root, "BorrowAfterOwnerMove", prefix + R"NS(
class Api
{
    public static void Consume(owned Resource value) { }
    public static void Run()
    {
        raii Resource owner = new Resource(1);
        Consume(move(owner));
        borrowed Resource view = owner;
    }
}
)NS", "use of moved-from RAII local 'owner'");

    expectInvalid(tests, root, "BorrowUnqualifiedParameter", prefix + R"NS(
class Api
{
    public static void Observe(Resource input)
    {
        borrowed Resource view = input;
    }
}
)NS", "borrowed local source parameter 'input' must be declared owned or borrowed");

    expectInvalid(tests, root, "BorrowTemporary", prefix + R"NS(
class Api
{
    public static void Run()
    {
        borrowed Resource view = new Resource(1);
    }
}
)NS", "borrowed local requires a direct live owner or borrowed identifier source");

    expectInvalid(tests, root, "BorrowedLocalToOwned", prefix + R"NS(
class Api
{
    public static void Consume(owned Resource value) { }
    public static void Run()
    {
        raii Resource owner = new Resource(1);
        borrowed Resource view = owner;
        Consume(view);
    }
}
)NS", "cannot convert borrowed value 'view' to owned parameter");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage50_5_local_borrows_lifetimes";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testPositiveLocalBorrows(tests, root);
    testNegativeLocalBorrows(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
