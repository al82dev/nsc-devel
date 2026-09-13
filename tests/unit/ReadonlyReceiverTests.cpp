#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
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
            "</Name></PropertyGroup><ItemGroup><Compile "
            "Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

bool hasDiagnostic(
    const ProjectCompilation& compilation,
    nativesharp::diagnostics::DiagnosticId id)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.id() == id) return true;
    }
    return false;
}

bool compileBodies(const std::filesystem::path& project, ProjectCompilation& compilation)
{
    std::string error;
    return compilation.compileProjectMethodBodies(project, error) && error.empty();
}
}

int main()
{
    using nativesharp::diagnostics::DiagnosticId;

    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_readonly_receiver_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    {
        const auto directory = root / "Positive";
        writeProject(
            directory,
            "ReadonlyReceiverPositive",
            "public struct Counter { public int Value; "
            "public readonly int Read() { return Value; } "
            "public readonly int Current { get { return Value; } } "
            "public readonly int this[int offset] { get { return Value + offset; } } "
            "public void Bump() { Value = Value + 1; } } "
            "public class Use { "
            "public static int Observe(in Counter value) { return value.Read() + value.Current + value[1]; } "
            "public static void Change(ref Counter value) { value.Bump(); value.Value = 4; } }");
        ProjectCompilation compilation;
        tests.expect(
            compileBodies(directory / "ReadonlyReceiverPositive.nsproj", compilation),
            "in receivers call readonly methods, properties and indexers while ref receivers remain mutable");
    }

    {
        const auto directory = root / "DirectMutation";
        writeProject(
            directory,
            "ReadonlyReceiverDirectMutation",
            "public struct Counter { public int Value; "
            "public readonly void Reset() { Value = 0; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectMethodBodies(
                directory / "ReadonlyReceiverDirectMutation.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidAssignmentTarget),
            "a readonly method cannot assign through its implicit this receiver");
    }

    {
        const auto directory = root / "TransitiveMutation";
        writeProject(
            directory,
            "ReadonlyReceiverTransitiveMutation",
            "public struct Counter { public int Value; "
            "public void Bump() { Value = Value + 1; } "
            "public readonly void Invalid() { Bump(); } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectMethodBodies(
                directory / "ReadonlyReceiverTransitiveMutation.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidCall),
            "a readonly method cannot call a mutating member on its implicit receiver");
    }

    {
        const auto directory = root / "InMutation";
        writeProject(
            directory,
            "ReadonlyReceiverInMutation",
            "public struct Counter { public int Value; public void Bump() { Value = Value + 1; } } "
            "public class Use { public static void Invalid(in Counter value) { "
            "value.Value = 1; value.Bump(); } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectMethodBodies(
                directory / "ReadonlyReceiverInMutation.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidAssignmentTarget) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidCall),
            "an in parameter is a readonly receiver for both stores and instance calls");
    }

    {
        const auto directory = root / "ReadonlyField";
        writeProject(
            directory,
            "ReadonlyReceiverField",
            "public struct Counter { public int Value; public void Bump() { Value = Value + 1; } } "
            "public class Holder { public readonly Counter Value; "
            "public Holder(Counter value) { Value = value; } "
            "public void Invalid() { Value.Bump(); } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectMethodBodies(
                directory / "ReadonlyReceiverField.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidCall),
            "a readonly struct field cannot be used as a mutating receiver");
    }

    {
        const auto directory = root / "ReadonlyStructImplicit";
        writeProject(
            directory,
            "ReadonlyStructImplicitReceiver",
            "public struct Counter { public int Value; public void Bump() { Value = Value + 1; } } "
            "public readonly struct Holder { public readonly Counter Value; "
            "public Holder(Counter value) { Value = value; } "
            "public void Invalid() { Value.Bump(); } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectMethodBodies(
                directory / "ReadonlyStructImplicitReceiver.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidCall),
            "instance members of a readonly struct receive readonly this implicitly");
    }

    {
        const auto directory = root / "InlineArray";
        writeProject(
            directory,
            "ReadonlyReceiverInlineArray",
            "public struct Buffer { public int[4] Values; "
            "public readonly void Invalid() { Values[0] = 1; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectMethodBodies(
                directory / "ReadonlyReceiverInlineArray.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidAssignmentTarget),
            "readonly propagation reaches elements of inline fixed-array fields");
    }

    {
        const auto directory = root / "Event";
        writeProject(
            directory,
            "ReadonlyReceiverEvent",
            "public delegate void Changed(); public struct Source { public event Changed Change; "
            "public readonly void Invalid(Changed handler) { Change += handler; } }");
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectMethodBodies(
                directory / "ReadonlyReceiverEvent.nsproj", error) &&
                hasDiagnostic(compilation, DiagnosticId::InvalidAssignmentTarget),
            "readonly receivers cannot mutate event backing storage through subscription");
    }

    for (const auto& invalid : {
             std::pair{"ReadonlyClassMethod", "public class Bad { public readonly int Read() { return 1; } }"},
             std::pair{"ReadonlyStaticMethod", "public struct Bad { public static readonly int Read() { return 1; } }"},
             std::pair{"ReadonlySetter", "public struct Bad { public int X; public readonly int Value { get { return X; } set { X = value; } } }"}})
    {
        const auto directory = root / invalid.first;
        writeProject(directory, invalid.first, invalid.second);
        ProjectCompilation compilation;
        std::string error;
        tests.expect(
            !compilation.compileProjectMembers(directory / (std::string(invalid.first) + ".nsproj"), error) &&
                hasDiagnostic(compilation, DiagnosticId::ExpectedDeclaration),
            std::string(invalid.first) + " rejects an invalid readonly receiver declaration");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
