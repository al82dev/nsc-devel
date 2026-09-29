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
        std::filesystem::temp_directory_path() / "nativesharp_generic_inference_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    {
        const auto directory = root / "Positive";
        writeProject(directory, "GenericInferencePositive", R"NS(
public class Box<T>
{
    public T Value;
    public Box(T value) { Value = value; }
}

public class Item { }

public class Program
{
    public static T Identity<T>(T value) { return value; }
    public static T Make<T>() { return default(T); }
    public static T First<T>(T[] values) { return values[0]; }
    public static T Unbox<T>(Box<T> value) { return value.Value; }
    public static T Named<T>(int ignored, T value) { return value; }
    public static T Optional<T>(T value, int count = 1) { return value; }
    public static int Select<T>(T value) { return 0; }
    public static int Select<T>(Box<T> value) { return 9; }
    public static int Measure<T>(T value) { return 0; }
    public static int Measure<T>(T[] value) { return 10; }
    public static void Assign<T>(ref T target, T value)
    {
        target = value;
    }

    public static int Main()
    {
        int local = 40;
        int fromLocal = Identity(local);
        int fromExpected = Make();
        Item nullFromExpected = Identity(null);
        long fromLiteral = Identity(2L);
        int nested = Identity(Identity(1));
        int named = Named(value: 3, ignored: 0);
        int optional = Optional(4);
        int[] values = new int[1];
        values[0] = 5;
        int first = First(values);
        Box<int> box = new Box<int>(6);
        int boxed = Unbox(box);
        var inferredBox = new Box<int>(11);
        int fromVar = Unbox(inferredBox);
        short narrowed = Identity((short)12);
        int selected = Select(box);
        int measured = Measure(values);
        int left = 7;
        int right = 8;
        Assign(ref left, right);
        delete[] values;
        return fromLocal + (int)fromLiteral + nested + named + optional + first + boxed +
            fromVar + (int)narrowed + selected + measured + left + right - 119;
    }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(
            directory / "GenericInferencePositive.nsproj", error);
        if (!ok)
        {
            for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                std::cerr << diagnostic.message() << '\n';
        }
        tests.expect(ok && error.empty(),
            "local generic inference reaches concrete OOP MIR for literals, locals, arrays, nested generics, named/default arguments, nested calls, and ref arguments");
        tests.expect(compilation.genericModel() != nullptr,
            "inferred calls expose the ordinary monomorphization model");
        if (compilation.genericModel() != nullptr)
        {
            const auto specializations = compilation.genericModel()->specializations();
            tests.expect(std::ranges::any_of(specializations, [](const auto& item)
            {
                return item.specializationKey.find("Identity``1<int>") != std::string::npos;
            }), "int inference materializes Identity<int>");
            tests.expect(std::ranges::any_of(specializations, [](const auto& item)
            {
                return item.specializationKey.find("Identity``1<long>") != std::string::npos;
            }), "literal suffix inference materializes Identity<long>");
            tests.expect(std::ranges::any_of(specializations, [](const auto& item)
            {
                return item.specializationKey.find("First``1<int>") != std::string::npos;
            }), "T[] unification strips the array shape and infers int");
            tests.expect(std::ranges::any_of(specializations, [](const auto& item)
            {
                return item.specializationKey.find("Unbox``1<int>") != std::string::npos;
            }), "nested Box<T> unification infers its inner argument");
            tests.expect(std::ranges::count_if(specializations, [](const auto& item)
            {
                return item.specializationKey.find("Identity``1<int>") != std::string::npos;
            }) == 1, "repeated and nested inferred calls reuse one canonical specialization");
        }
    }

    const auto expectReject = [&](std::string_view name, std::string_view source,
                                  std::string_view message)
    {
        const auto directory = root / std::string(name);
        writeProject(directory, name, source);
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectGenerics(
                directory / (std::string(name) + ".nsproj"), error),
            std::string(name) + " is rejected during local generic inference");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::InvalidCall, message),
            std::string(name) + " reports why explicit type arguments are required");
    };

    expectReject(
        "ConflictingRepeatedParameter",
        "class Program { static T Same<T>(T left, T right) { return left; } "
        "static int Run() { return Same(1, 2L); } }",
        "cannot be inferred");
    expectReject(
        "NoExpectedType",
        "class Program { static T Make<T>() { return default(T); } "
        "static void Run() { var value = Make(); } }",
        "cannot be inferred");

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
