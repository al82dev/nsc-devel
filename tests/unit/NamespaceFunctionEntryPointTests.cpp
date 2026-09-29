#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source)
{
    std::filesystem::create_directories(root);
    writeFile(root / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name></PropertyGroup><ItemGroup><Compile Include='Main.ns' />"
        "</ItemGroup></Project>");
    writeFile(root / "Main.ns", source);
    return root / (std::string(name) + ".nsproj");
}
}

int main()
{
    using namespace nativesharp::frontend;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_namespace_function_entry_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    ProjectCompilation valid;
    std::string validError;
    const bool validBuilt = valid.compileProjectOopMir(
        writeProject(root / "valid", "Entry",
            "namespace App { int Main() { return 42; } }"),
        validError);
    tests.expect(validBuilt && validError.empty() &&
            valid.oopMirModel() != nullptr && valid.namespaceFunctionModel() != nullptr,
        "free Main compiles through executable MIR");
    if (validBuilt && valid.oopMirModel() != nullptr &&
        valid.namespaceFunctionModel() != nullptr)
    {
        bool ambiguous = false;
        const ProjectOopMirFunction* entry = valid.oopMirModel()->executableEntryPoint(
            *valid.namespaceFunctionModel(), ambiguous);
        tests.expect(entry != nullptr && !ambiguous &&
                entry->namespaceFunctionOrdinal.has_value(),
            "free int Main() is the unique executable entry point");
        tests.expect(entry != nullptr &&
                entry->mangledName.starts_with("__nsc_function_"),
            "entry selection returns the frozen namespace-function symbol");
    }

    ProjectCompilation ambiguousCompilation;
    std::string ambiguousError;
    const bool ambiguousBuilt = ambiguousCompilation.compileProjectOopMir(
        writeProject(root / "ambiguous", "Ambiguous",
            "namespace App { int Main() { return 0; } } "
            "public class Program { public static int Main() { return 0; } }"),
        ambiguousError);
    tests.expect(ambiguousBuilt && ambiguousCompilation.oopMirModel() != nullptr &&
            ambiguousCompilation.namespaceFunctionModel() != nullptr,
        "free and class Main both reach entry-point selection");
    if (ambiguousBuilt && ambiguousCompilation.oopMirModel() != nullptr &&
        ambiguousCompilation.namespaceFunctionModel() != nullptr)
    {
        bool ambiguous = false;
        const ProjectOopMirFunction* entry =
            ambiguousCompilation.oopMirModel()->executableEntryPoint(
                *ambiguousCompilation.namespaceFunctionModel(), ambiguous);
        tests.expect(entry == nullptr && ambiguous,
            "free and class Main are rejected as an explicit ambiguity");
    }

    ProjectCompilation invalid;
    std::string invalidError;
    const bool invalidBuilt = invalid.compileProjectOopMir(
        writeProject(root / "invalid", "Invalid",
            "namespace App { long Main() { return 42; } }"),
        invalidError);
    if (invalidBuilt && invalid.oopMirModel() != nullptr &&
        invalid.namespaceFunctionModel() != nullptr)
    {
        bool ambiguous = false;
        tests.expect(invalid.oopMirModel()->executableEntryPoint(
                *invalid.namespaceFunctionModel(), ambiguous) == nullptr && !ambiguous,
            "a free Main with the wrong signature is not an entry point");
    }
    else
    {
        tests.expect(false, "wrong-signature Main still compiles far enough for entry selection");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}