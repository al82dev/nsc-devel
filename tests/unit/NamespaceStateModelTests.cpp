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
    std::string_view source)
{
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    writeFile(
        root / "State.nsproj",
        R"(<Project><PropertyGroup><Name>State</Name></PropertyGroup><ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)");
    writeFile(root / "State.ns", source);
    return root / "State.nsproj";
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
        std::filesystem::current_path() / "_namespace_state_model_test";
    std::error_code errorCode;

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "valid",
            "namespace Numeric { public const int Answer = 40 + 2; int Counter; "
            "internal bool Enabled = true; }");
        const bool ok = compilation.compileProjectMembers(project, error);
        tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
            "constant and zero initialized namespace state is accepted");
        tests.expect(compilation.namespaceFunctionModel() != nullptr,
            "namespace model is available");
        if (compilation.namespaceFunctionModel() != nullptr)
        {
            const auto& model = *compilation.namespaceFunctionModel();
            tests.expect(model.states().size() == 3,
                "all namespace state declarations are modeled");
            const NamespaceStateSymbol* answer =
                model.findState(0, "Numeric", "Answer");
            const NamespaceStateSymbol* counter =
                model.findState(0, "Numeric", "Counter");
            tests.expect(answer != nullptr && answer->isConst &&
                    answer->initializerKind == NamespaceStateInitializerKind::Integer &&
                    answer->integerValue == 42,
                "integer constants are folded and retain const identity");
            tests.expect(counter != nullptr && !counter->isConst &&
                    counter->initializerKind == NamespaceStateInitializerKind::Zero,
                "declarations without initializer use zero initialization");
            tests.expect(answer != nullptr &&
                    answer->storageName.starts_with("__nsc_state_"),
                "state receives a deterministic physical storage name");
        }
    }

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "dynamic",
            "namespace Numeric { int Build() { return 7; } int Value = Build(); }");
        const bool ok = compilation.compileProjectMembers(project, error);
        tests.expect(!ok, "dynamic namespace initialization is rejected");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::GlobalRequiresConstant),
            "dynamic initialization emits GlobalRequiresConstant");
    }

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "duplicate",
            "namespace Numeric { int Value; int Value; }");
        const bool ok = compilation.compileProjectMembers(project, error);
        tests.expect(!ok, "duplicate namespace state is rejected");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::DuplicateSymbol),
            "duplicate namespace state emits DuplicateSymbol");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::PreviousDeclaration),
            "duplicate namespace state identifies its prior declaration");
    }

    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}