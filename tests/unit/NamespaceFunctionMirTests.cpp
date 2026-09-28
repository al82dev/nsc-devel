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

const nativesharp::frontend::ProjectOopMirNode* findNamespaceCall(
    const nativesharp::frontend::ProjectOopMirNode* node)
{
    if (node == nullptr) return nullptr;
    if (node->kind == nativesharp::frontend::ProjectOopMirNodeKind::DirectStaticCall &&
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
        std::filesystem::current_path() / "_namespace_function_mir_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);
    writeFile(root / "Mir.nsproj",
        R"(<Project><PropertyGroup><Name>Mir</Name></PropertyGroup><ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)");
    writeFile(root / "Functions.ns",
        "namespace Math { int Add(int left, int right) { return left + right; } } "
        "namespace App { int Run() { return Math.Add(20, 22); } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(root / "Mir.nsproj", error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "namespace functions lower through project MIR");
    tests.expect(compilation.namespaceFunctionModel() != nullptr &&
            compilation.oopMirModel() != nullptr,
        "project exposes namespace and MIR models");

    if (compilation.namespaceFunctionModel() != nullptr && compilation.oopMirModel() != nullptr)
    {
        const auto symbols = compilation.namespaceFunctionModel()->functions();
        const ProjectOopMirModel& mir = *compilation.oopMirModel();
        tests.expect(symbols.size() == 2, "two namespace functions reach MIR");
        std::size_t runOrdinal = symbols.size();
        std::size_t addOrdinal = symbols.size();
        for (std::size_t ordinal = 0; ordinal < symbols.size(); ++ordinal)
        {
            const ProjectOopMirFunction* function =
                mir.functionForNamespaceFunction(ordinal);
            tests.expect(function != nullptr &&
                    function->namespaceFunctionOrdinal == ordinal,
                "MIR indexes each namespace function independently");
            if (function != nullptr)
            {
                tests.expect(function->mangledName == symbols[ordinal].mangledName,
                    "MIR retains the frozen physical name");
                tests.expect(function->isStatic && function->body != nullptr,
                    "MIR retains a static executable body");
            }
            if (symbols[ordinal].name == "Run") runOrdinal = ordinal;
            if (symbols[ordinal].name == "Add") addOrdinal = ordinal;
        }

        const ProjectOopMirFunction* run =
            runOrdinal < symbols.size() ? mir.functionForNamespaceFunction(runOrdinal) : nullptr;
        const ProjectOopMirNode* call =
            run != nullptr ? findNamespaceCall(run->body.get()) : nullptr;
        tests.expect(call != nullptr && call->namespaceFunctionOrdinal == addOrdinal,
            "MIR direct call retains the selected namespace ordinal");
        if (call != nullptr)
        {
            tests.expect(!call->memberOrdinal.has_value() && call->argumentCount == 2,
                "MIR call has no synthetic class member or receiver");
            tests.expect(call->calleeName == symbols[addOrdinal].mangledName,
                "MIR direct call names the selected physical symbol");
        }
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}