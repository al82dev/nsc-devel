#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"

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

std::filesystem::path createProject(
    const std::filesystem::path& root,
    std::string_view first,
    std::string_view second = {})
{
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    writeFile(
        root / "Functions.nsproj",
        "<Project><PropertyGroup><Name>Functions</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(root / "A.ns", first);
    if (!second.empty()) writeFile(root / "B.ns", second);
    return root / "Functions.nsproj";
}

bool hasDiagnostic(
    const nativesharp::frontend::ProjectCompilation& compilation,
    nativesharp::diagnostics::DiagnosticId id)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.id() == id) return true;
    }
    return false;
}
}

int main()
{
    using nativesharp::diagnostics::DiagnosticId;
    using namespace nativesharp::frontend;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_namespace_function_signature_test";
    std::error_code errorCode;

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "valid",
            "namespace Numeric { public int Sum(int left, int right); }\n",
            "namespace Numeric { public int Sum(int a, int b) { return a + b; } "
            "public long Sum(long a, long b) { return a + b; } }\n");
        const bool ok = compilation.compileProjectMembers(project, error);
        tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
            "compatible declaration and definition merge successfully");
        tests.expect(compilation.namespaceFunctionModel() != nullptr,
            "project compilation exposes the namespace function model");
        if (compilation.namespaceFunctionModel() != nullptr)
        {
            const auto& model = *compilation.namespaceFunctionModel();
            tests.expect(model.isBuilt(), "namespace function model reports a successful build");
            tests.expect(model.functions().size() == 2,
                "different parameter types remain distinct overloads");
            if (!model.functions().empty())
            {
                const NamespaceFunctionSymbol& first = model.functions().front();
                tests.expect(first.signatureKey == "Sum(builtin:int,builtin:int)",
                    "signature identity contains canonical parameter types but not return type");
                tests.expect(first.parts.size() == 2 && first.definitionSyntax != nullptr,
                    "forward declaration and body are retained as one symbol with two parts");
                tests.expect(model.findFunctions(
                        first.projectOrdinal, first.namespaceName, first.name).size() == 2,
                    "name index returns the complete overload set");
            }
        }
    }

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "duplicate",
            "namespace Numeric { int Value() { return 1; } }\n",
            "namespace Numeric { int Value() { return 2; } }\n");
        const bool ok = compilation.compileProjectMembers(project, error);
        tests.expect(!ok, "two namespace function bodies are rejected");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::DuplicateFunctionDefinition),
            "duplicate body emits DuplicateFunctionDefinition");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::PreviousDeclaration),
            "duplicate body points to the previous declaration");
    }

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "conflict",
            "namespace Numeric { public int Convert(int value); }\n",
            "namespace Numeric { public long Convert(int value) { return value; } }\n");
        const bool ok = compilation.compileProjectMembers(project, error);
        tests.expect(!ok, "incompatible return types are rejected");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::ConflictingFunctionDeclaration),
            "return conflict emits ConflictingFunctionDeclaration");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::PreviousDeclaration),
            "return conflict points to the previous declaration");
    }

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "missing",
            "namespace Numeric { int DeclaredOnly(int value); }\n");
        const bool ok = compilation.compileProjectMembers(project, error);
        tests.expect(ok, "an unused namespace-function prototype is a valid declaration");
        tests.expect(!hasDiagnostic(compilation, DiagnosticId::MissingFunctionDefinition),
            "an unused prototype does not demand code generation");
    }

    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}
