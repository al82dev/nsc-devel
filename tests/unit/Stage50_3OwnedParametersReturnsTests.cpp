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
using nativesharp::frontend::MethodOwnershipTransferKind;
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
    tests.expect(!ok, std::string(caseName) + " is rejected by Stage 50.3 ownership semantics");
    tests.expect(
        hasDiagnosticText(compilation, diagnostic),
        std::string(caseName) + " reports the expected Stage 50.3 ownership diagnostic");
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
    public static owned Resource Create(int value)
    {
        raii Resource result = new Resource(value);
        return move(result);
    }

    public static owned Resource Forward(owned Resource value)
    {
        return move(value);
    }

    public static owned Resource ForwardCreated(int value)
    {
        return Create(value);
    }

    public static void Consume(owned Resource value)
    {
        int observed = value.Read();
    }

    public static void ConsumeAgain(owned Resource value)
    {
        Consume(move(value));
    }

    public static void Route(Resource value)
    {
        int observed = value.Read();
    }

    public static void Route(owned Resource value)
    {
        int observed = value.Read();
    }

    public static int Run()
    {
        raii Resource first = Create(1);
        Consume(move(first));

        raii Resource second = Create(2);
        raii Resource third = Forward(move(second));
        Route(move(third));

        raii Resource fourth = ForwardCreated(4);
        Consume(move(fourth));
        return 0;
    }
}
)NS";
}

void testPositiveTransfers(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "Positive";
    writeProject(directory, "Stage50_3Positive", positiveSource());

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "Stage50_3Positive.nsproj", error);
    tests.expect(ok, "Stage 50.3 positive owned-parameter/return sample binds successfully");
    tests.expect(error.empty(), "Stage 50.3 positive sample has no loader/internal error");
    tests.expect(compilation.methodBodyModel() != nullptr && compilation.memberModel() != nullptr &&
            compilation.nameLookup() != nullptr,
        "Stage 50.3 exposes body/member/name models for ownership inspection");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.memberModel() == nullptr ||
        compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto& body = *compilation.methodBodyModel();
    const auto transfers = body.ownershipTransferBindings();

    tests.expect(std::ranges::count_if(transfers, [](const auto& transfer)
        {
            return transfer.kind == MethodOwnershipTransferKind::MoveLocalToOwnedParameter;
        }) == 4,
        "four RAII locals transfer ownership into owned parameters");
    tests.expect(std::ranges::count_if(transfers, [](const auto& transfer)
        {
            return transfer.kind == MethodOwnershipTransferKind::MoveParameterToOwnedParameter;
        }) == 1,
        "an owned parameter can be explicitly forwarded into another owned parameter");
    tests.expect(std::ranges::count_if(transfers, [](const auto& transfer)
        {
            return transfer.kind == MethodOwnershipTransferKind::MoveLocalToOwnedReturn;
        }) == 1,
        "a RAII local can be explicitly transferred through an owned return");
    tests.expect(std::ranges::count_if(transfers, [](const auto& transfer)
        {
            return transfer.kind == MethodOwnershipTransferKind::MoveParameterToOwnedReturn;
        }) == 1,
        "an owned parameter can be explicitly transferred through an owned return");
    tests.expect(std::ranges::count_if(transfers, [](const auto& transfer)
        {
            return transfer.kind == MethodOwnershipTransferKind::OwnedReturnForward;
        }) == 1,
        "an owned-returning call can be forwarded through another owned return");

    tests.expect(std::ranges::all_of(transfers, [](const auto& transfer)
        {
            return transfer.targetMemberOrdinal.has_value() && !transfer.typeKey.empty();
        }),
        "every Stage 50.3 transfer records its semantic target and physical type");

    std::ostringstream dump;
    nativesharp::frontend::ProjectMethodBodyModelDumper{}.dump(
        body, *compilation.memberModel(), *compilation.nameLookup(), dump);
    const std::string text = dump.str();
    tests.expect(text.find("ownership-transfers=8") != std::string::npos,
        "method-body dump freezes all eight explicit Stage 50.3 transfer edges");
    tests.expect(text.find("move-local-to-owned-parameter") != std::string::npos &&
            text.find("move-parameter-to-owned-return") != std::string::npos &&
            text.find("owned-return-forward") != std::string::npos,
        "method-body dump exposes the Stage 50.3 ownership transfer categories");

    const auto& members = compilation.memberModel()->members();
    tests.expect(std::ranges::any_of(members, [](const auto& member)
        {
            return member.name == "Create" && member.declaredOwnership == OwnershipKind::Owned;
        }),
        "positive fixture retains an owned return contract in the member model");
    tests.expect(std::ranges::any_of(members, [](const auto& member)
        {
            return member.name == "Consume" && !member.parameters.empty() &&
                member.parameters.front().ownership == OwnershipKind::Owned;
        }),
        "positive fixture retains an owned parameter contract in the member model");
}

void testNegativeTransfers(TestContext& tests, const std::filesystem::path& root)
{
    const std::string prefix = resourcePrefix();

    expectInvalid(tests, root, "ImplicitOwnedArgument", prefix + R"NS(
class Api
{
    public static void Consume(owned Resource value) { }
    public static void Run() { raii Resource owner = new Resource(1); Consume(owner); }
}
)NS", "cannot copy owned value 'owner' into owned parameter; use move(owner)");

    expectInvalid(tests, root, "UseAfterConsume", prefix + R"NS(
class Api
{
    public static void Consume(owned Resource value) { }
    public static int Run()
    {
        raii Resource owner = new Resource(1);
        Consume(move(owner));
        return owner.Read();
    }
}
)NS", "use of moved-from RAII local 'owner'");

    expectInvalid(tests, root, "DoubleConsume", prefix + R"NS(
class Api
{
    public static void Consume(owned Resource value) { }
    public static void Run()
    {
        raii Resource owner = new Resource(1);
        Consume(move(owner));
        Consume(move(owner));
    }
}
)NS", "cannot move from already moved-from RAII local 'owner'");

    expectInvalid(tests, root, "OwnedReturnNeedsMove", prefix + R"NS(
class Api
{
    public static owned Resource Create()
    {
        raii Resource owner = new Resource(1);
        return owner;
    }
}
)NS", "requires return move(owner) to transfer ownership");

    expectInvalid(tests, root, "OwnedParameterReturnNeedsMove", prefix + R"NS(
class Api
{
    public static owned Resource Forward(owned Resource owner)
    {
        return owner;
    }
}
)NS", "returning owned parameter 'owner' requires return move(owner) to transfer ownership");

    expectInvalid(tests, root, "OwnedReturnNeedsOwnedSource", prefix + R"NS(
class Api
{
    public static owned Resource Bad(Resource value)
    {
        return value;
    }
}
)NS", "owned return requires an ownership-transfer expression");

    expectInvalid(tests, root, "OwnedParameterUseAfterForward", prefix + R"NS(
class Api
{
    public static void Consume(owned Resource value) { }
    public static int ForwardThenUse(owned Resource owner)
    {
        Consume(move(owner));
        return owner.Read();
    }
}
)NS", "use of moved-from owned parameter 'owner'");

    expectInvalid(tests, root, "OwnedReturnIntoNonOwner", prefix + R"NS(
class Api
{
    public static owned Resource Create() { return new Resource(1); }
    public static void Run() { Resource value = Create(); }
}
)NS", "owned return value cannot initialize a non-owning local");

    expectInvalid(tests, root, "MoveIntoUnqualifiedParameter", prefix + R"NS(
class Api
{
    public static void Inspect(Resource value) { }
    public static void Run()
    {
        raii Resource owner = new Resource(1);
        Inspect(move(owner));
    }
}
)NS", "ownership-transfer argument requires an owned parameter");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage50_3_owned_parameters_returns";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testPositiveTransfers(tests, root);
    testNegativeTransfers(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
