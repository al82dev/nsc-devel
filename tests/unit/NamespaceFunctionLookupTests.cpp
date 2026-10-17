#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

const nativesharp::frontend::NamespaceFunctionReferenceBinding* findBinding(
    const nativesharp::frontend::ProjectNamespaceFunctionModel& model,
    std::string_view writtenName)
{
    for (const auto& binding : model.referenceBindings())
        if (binding.writtenName == writtenName) return &binding;
    return nullptr;
}

void testDeepAstWalk(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "DeepAstWalk";
    writeFile(
        directory / "DeepAstWalk.nsproj",
        "<Project><PropertyGroup><Name>DeepAstWalk</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");

    constexpr std::size_t depth = 1024;
    std::string source = "class Box { int Run() {\n";
    for (std::size_t index = 0; index < depth; ++index)
    {
        source += "if (true) {\n";
    }
    source += "return 0;\n";
    for (std::size_t index = 0; index < depth; ++index)
    {
        source += "}\n";
    }
    source += "} }\n";
    writeFile(directory / "Main.ns", source);

    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(
        directory / "DeepAstWalk.nsproj",
        error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "namespace-function lookup walks 1,024 nested controls without using the native stack");
    tests.expect(compilation.namespaceFunctionModel() != nullptr,
        "the deep AST publishes its namespace-function model");
}
}

int main()
{
    using namespace nativesharp::frontend;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_namespace_function_lookup_test";
    std::error_code errorCode;
    std::filesystem::remove_all(root, errorCode);
    std::filesystem::create_directories(root, errorCode);

    writeFile(
        root / "Lookup.nsproj",
        "<Project><PropertyGroup><Name>Lookup</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(
        root / "Functions.ns",
        "namespace Math { public int Twice(int value) { return value * 2; } "
        "public long Twice(long value) { return value * 2; } }\n"
        "namespace Alt { public int Twice(int value) { return value + value; } }\n");
    writeFile(
        root / "Calls.ns",
        "using Math; using A = Alt;\n"
        "namespace App { int Current(int value) { return value; } "
        "int Run() { return Twice(2) + Math.Twice(3) + A.Twice(4) + Current(5); } }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(root / "Lookup.nsproj", error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "namespace call candidates bind without body analysis");
    tests.expect(compilation.namespaceFunctionModel() != nullptr,
        "project exposes lookup bindings through the namespace function model");
    if (compilation.namespaceFunctionModel() != nullptr)
    {
        const ProjectNamespaceFunctionModel& model = *compilation.namespaceFunctionModel();
        tests.expect(model.referenceBindings().size() == 4,
            "all four namespace-shaped calls receive a binding");
        const auto* imported = findBinding(model, "Twice");
        const auto* qualified = findBinding(model, "Math.Twice");
        const auto* aliased = findBinding(model, "A.Twice");
        const auto* current = findBinding(model, "Current");
        tests.expect(imported != nullptr && imported->candidateFunctionOrdinals.size() == 2,
            "using namespace exposes the complete overload set");
        tests.expect(qualified != nullptr && qualified->qualified &&
                qualified->candidateFunctionOrdinals.size() == 2,
            "qualified namespace lookup exposes the same overload set");
        tests.expect(aliased != nullptr && aliased->qualified &&
                aliased->candidateFunctionOrdinals.size() == 1,
            "namespace alias resolves to its target function");
        tests.expect(current != nullptr && !current->qualified &&
                current->candidateFunctionOrdinals.size() == 1,
            "current namespace lookup takes precedence over imported namespaces");
        if (aliased != nullptr && !aliased->candidateFunctionOrdinals.empty())
        {
            const auto ordinal = aliased->candidateFunctionOrdinals.front();
            tests.expect(ordinal < model.functions().size() &&
                    model.functions()[ordinal].namespaceName == "Alt",
                "alias binding retains the semantic namespace owner");
            tests.expect(model.referenceBinding(aliased->callSyntax) == aliased,
                "call syntax maps back to its stable binding");
        }
    }

    testDeepAstWalk(tests, root);

    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}
