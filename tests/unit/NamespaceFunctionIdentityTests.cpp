#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <map>
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

std::map<std::string, std::string> compileIdentities(
    const std::filesystem::path& root,
    bool reverse,
    nativesharp::tests::TestContext& tests)
{
    std::error_code errorCode;
    std::filesystem::remove_all(root, errorCode);
    std::filesystem::create_directories(root, errorCode);
    writeFile(root / "Identity.nsproj",
        "<Project><PropertyGroup><Name>Identity</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    const std::string first =
        "namespace One { int Same(int value) { return value; } "
        "long Same(long value) { return value; } }\n";
    const std::string second =
        "namespace Two { int Same(int value) { return value + 1; } }\n";
    writeFile(root / "A.ns", reverse ? second : first);
    writeFile(root / "B.ns", reverse ? first : second);

    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(root / "Identity.nsproj", error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "identity fixture compiles through namespace symbol construction");
    std::map<std::string, std::string> result;
    if (compilation.namespaceFunctionModel() != nullptr)
    {
        for (const auto& function : compilation.namespaceFunctionModel()->functions())
            result.emplace(function.namespaceName + "." + function.signatureKey,
                           function.mangledName);
    }
    return result;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_namespace_function_identity_test";
    const auto normal = compileIdentities(root / "normal", false, tests);
    const auto reversed = compileIdentities(root / "reversed", true, tests);
    tests.expect(normal.size() == 3, "three semantic identities are emitted");
    tests.expect(normal == reversed, "physical names do not depend on source file order");
    if (normal.size() == 3)
    {
        tests.expect(normal.at("One.Same(builtin:int)") != normal.at("One.Same(builtin:long)"),
            "parameter overloads receive different physical names");
        tests.expect(normal.at("One.Same(builtin:int)") != normal.at("Two.Same(builtin:int)"),
            "equal signatures in different namespaces receive different physical names");
        tests.expect(normal.at("One.Same(builtin:int)").starts_with("__nsc_function_"),
            "namespace functions use a physical prefix distinct from methods");
    }
    std::error_code errorCode;
    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}