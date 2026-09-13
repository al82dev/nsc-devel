#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
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
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
            "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

bool hasDiagnostic(
    const ProjectCompilation& compilation,
    nativesharp::diagnostics::DiagnosticId id,
    std::string_view text)
{
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [&](const auto& diagnostic)
        {
            return diagnostic.id() == id && diagnostic.message().find(text) != std::string::npos;
        });
}
}

int main()
{
    using nativesharp::diagnostics::DiagnosticId;

    TestContext tests;
    const auto root =
        std::filesystem::temp_directory_path() / "nativesharp_generic_alias_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    {
        const auto directory = root / "Positive";
        writeProject(directory, "GenericAliasPositive", R"NS(
public class Box<T>
{
    public T Value;
    public Box(T value) { Value = value; }
    public T Get() { return Value; }
}
public struct Pair<TLeft, TRight>
{
    public TLeft Left;
    public TRight Right;
}
using BoxOf<T> = Box<T>;
using Chain<T> = BoxOf<T>;
using Twice<T> = Pair<T, T>;
using Flip<TLeft, TRight> = Pair<TRight, TLeft>;
using Nested<T> = Box<Box<T>>;
using Pointer<T> = T*;
using Quad<T> = T[4];

public class Program
{
    public Chain<int> Stored;
    public Twice<long> Same;
    public Flip<int, long> Reversed;
    public Nested<int> Deep;

    public static int Run()
    {
        BoxOf<int> box = new BoxOf<int>(40);
        Pointer<int> pointer = null;
        Quad<int> values;
        values[0] = box.Get();
        values[1] = pointer == null ? 2 : 0;
        return values[0] + values[1];
    }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(
            directory / "GenericAliasPositive.nsproj", error);
        if (!ok)
        {
            for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                std::cerr << diagnostic.message() << '\n';
        }
        tests.expect(ok && error.empty(),
            "generic aliases reach concrete OOP MIR with chains, nested targets, pointers, and arrays");
        tests.expect(compilation.genericModel() != nullptr,
            "generic alias compilation exposes the ordinary generic model");
        if (compilation.genericModel() != nullptr)
        {
            const auto specializations = compilation.genericModel()->specializations();
            tests.expect(std::ranges::any_of(specializations, [](const auto& item)
            {
                return item.specializationKey.find("Box`1<int>") != std::string::npos;
            }), "BoxOf<int> and Chain<int> materialize the underlying Box<int>");
            tests.expect(std::ranges::any_of(specializations, [](const auto& item)
            {
                return item.specializationKey.find("Pair`2<long,long>") != std::string::npos;
            }), "Twice<long> duplicates its argument in the underlying specialization");
            tests.expect(std::ranges::any_of(specializations, [](const auto& item)
            {
                return item.specializationKey.find("Pair`2<long,int>") != std::string::npos;
            }), "Flip<int,long> reorders its arguments in the underlying specialization");
            tests.expect(std::ranges::none_of(specializations, [](const auto& item)
            {
                return item.specializationKey.find("BoxOf") != std::string::npos ||
                    item.specializationKey.find("Chain") != std::string::npos ||
                    item.specializationKey.find("Twice") != std::string::npos;
            }), "aliases create no template or ABI identity of their own");
        }
    }

    {
        const auto directory = root / "ParameterShadow";
        writeProject(directory, "GenericAliasParameterShadow", R"NS(
using T = int;
using Identity<T> = T;
public class Holder { public Identity<long> Value; }
)NS");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            compilation.compileProjectNames(
                directory / "GenericAliasParameterShadow.nsproj", error),
            "an alias type parameter shadows a same-named outer alias");
    }

    const auto expectReject = [&](std::string_view name, std::string_view source,
                                  DiagnosticId id, std::string_view message)
    {
        const auto directory = root / std::string(name);
        writeProject(directory, name, source);
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectGenerics(
                directory / (std::string(name) + ".nsproj"), error),
            std::string(name) + " is rejected during generic alias expansion");
        tests.expect(hasDiagnostic(compilation, id, message),
            std::string(name) + " reports the precise generic alias contract");
    };

    expectReject(
        "TooFewAliasArguments",
        "using PairOf<T, U> = Pair<T, U>; struct Pair<T, U> { T A; U B; } "
        "class Holder { PairOf<int> Value; }",
        DiagnosticId::InvalidArgumentCount,
        "expects 2 type argument(s) but received 1");
    expectReject(
        "TooManyAliasArguments",
        "using Pointer<T> = T*; class Holder { Pointer<int, long> Value; }",
        DiagnosticId::InvalidArgumentCount,
        "expects 1 type argument(s) but received 2");
    expectReject(
        "CyclicGenericAlias",
        "using A<T> = B<T>; using B<T> = A<T>; class Holder { A<int> Value; }",
        DiagnosticId::InvalidUsingTarget,
        "expands cyclically");
    expectReject(
        "ConflictingAliasOwnership",
        "using Owner<T> = owned T; class Item { } "
        "class Holder { Owner<borrowed Item> Value; }",
        DiagnosticId::InvalidType,
        "conflicting ownership qualifier");

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
