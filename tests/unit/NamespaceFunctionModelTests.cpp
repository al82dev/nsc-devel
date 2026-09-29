#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

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

const nativesharp::frontend::NamespaceFunctionDeclaration* findFunction(
    const nativesharp::frontend::ProjectNameLookup& lookup,
    std::string_view space,
    std::string_view name)
{
    for (const auto& function : lookup.namespaceFunctions())
    {
        if (function.namespaceName == space && function.name == name) return &function;
    }
    return nullptr;
}
}

int main()
{
    using namespace nativesharp::frontend;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_namespace_function_model";
    std::error_code errorCode;
    std::filesystem::remove_all(root, errorCode);
    std::filesystem::create_directories(root, errorCode);

    writeFile(
        root / "Functions.nsproj",
        "<Project><PropertyGroup><Name>Functions</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(
        root / "A.ns",
        "namespace Numeric { public int Add(int a, int b) { return a + b; } }\n");
    writeFile(
        root / "B.ns",
        "namespace Numeric { int Power(int b, int e); }\n"
        "namespace Text { internal int Length(byte* value) { return 0; } }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(root / "Functions.nsproj", error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "named namespace functions are indexed without semantic diagnostics");
    tests.expect(compilation.nameLookup() != nullptr, "project exposes name lookup");
    if (compilation.nameLookup() != nullptr)
    {
        const ProjectNameLookup& lookup = *compilation.nameLookup();
        tests.expect(lookup.namespaceFunctions().size() == 3,
            "all functions from all source files are indexed");
        const auto* add = findFunction(lookup, "Numeric", "Add");
        const auto* power = findFunction(lookup, "Numeric", "Power");
        const auto* length = findFunction(lookup, "Text", "Length");
        tests.expect(add != nullptr && add->accessibility == NamespaceFunctionAccessibility::Public &&
                add->hasBody,
            "public definition preserves accessibility and body state");
        tests.expect(power != nullptr && power->accessibility == NamespaceFunctionAccessibility::Internal &&
                !power->hasBody,
            "unmodified declaration defaults to internal and remains body-less");
        tests.expect(length != nullptr && length->moduleName == "Functions" && length->hasBody,
            "a function retains module, namespace, and definition identity");

        const std::vector<std::string> numericSegments{"Numeric"};
        const NamespaceSymbol* numeric =
            lookup.modules().back()->globalNamespace().findNamespace(numericSegments);
        tests.expect(numeric != nullptr && numeric->functionOrdinals().size() == 2,
            "namespace tree owns the two Numeric function ordinals");
    }

    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}
