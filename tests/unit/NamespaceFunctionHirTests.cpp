#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"

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

const nativesharp::frontend::ProjectOopHirNode* findNamespaceCall(
    const nativesharp::frontend::ProjectOopHirNode* node)
{
    if (node == nullptr) return nullptr;
    if (node->kind == nativesharp::frontend::ProjectOopHirNodeKind::StaticCall &&
        node->namespaceFunctionOrdinal.has_value())
    {
        return node;
    }
    for (const auto& child : node->children)
        if (const auto* found = findNamespaceCall(child.get()); found != nullptr) return found;
    return nullptr;
}
}

int main()
{
    using namespace nativesharp::frontend;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_namespace_function_hir_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);
    writeFile(root / "Hir.nsproj",
        R"(<Project><PropertyGroup><Name>Hir</Name></PropertyGroup><ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)");
    writeFile(root / "Functions.ns",
        "namespace Math { int Add(int left, int right) { return left + right; } } "
        "namespace App { int Run() { return Math.Add(20, 22); } }");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopHir(root / "Hir.nsproj", error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "namespace functions lower through project HIR");
    tests.expect(compilation.namespaceFunctionModel() != nullptr &&
            compilation.oopHirModel() != nullptr,
        "project exposes namespace and HIR models");

    if (compilation.namespaceFunctionModel() != nullptr && compilation.oopHirModel() != nullptr)
    {
        const auto functions = compilation.namespaceFunctionModel()->functions();
        const ProjectOopHirModel& hir = *compilation.oopHirModel();
        tests.expect(functions.size() == 2, "two free functions retain project identities");
        for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
        {
            const ProjectOopHirFunction* lowered = hir.functionForNamespaceFunction(ordinal);
            tests.expect(lowered != nullptr && lowered->namespaceFunctionOrdinal == ordinal,
                "each free function has an addressable HIR function");
            if (lowered != nullptr)
            {
                tests.expect(lowered->mangledName == functions[ordinal].mangledName,
                    "HIR preserves the frozen free-function physical identity");
                tests.expect(lowered->isStatic && lowered->body != nullptr,
                    "free function lowers as a body-bearing static callable");
            }
        }

        std::size_t runOrdinal = functions.size();
        std::size_t addOrdinal = functions.size();
        for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
        {
            if (functions[ordinal].name == "Run") runOrdinal = ordinal;
            if (functions[ordinal].name == "Add") addOrdinal = ordinal;
        }
        const ProjectOopHirFunction* run =
            runOrdinal < functions.size() ? hir.functionForNamespaceFunction(runOrdinal) : nullptr;
        const ProjectOopHirNode* call = run != nullptr ? findNamespaceCall(run->body.get()) : nullptr;
        tests.expect(call != nullptr && call->namespaceFunctionOrdinal == addOrdinal,
            "free-function call targets the selected free-function ordinal");
        if (call != nullptr)
        {
            tests.expect(!call->memberOrdinal.has_value() && call->argumentCount == 2,
                "free-function call has no fake class member or receiver");
            tests.expect(call->stringValue == functions[addOrdinal].mangledName,
                "call site carries the selected frozen symbol identity");
        }
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}