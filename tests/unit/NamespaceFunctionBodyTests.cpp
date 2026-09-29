#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

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
    std::string_view second)
{
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    writeFile(root / "Bodies.nsproj",
        "<Project><PropertyGroup><Name>Bodies</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(root / "A.ns", first);
    writeFile(root / "B.ns", second);
    return root / "Bodies.nsproj";
}

bool hasDiagnostic(
    const nativesharp::frontend::ProjectCompilation& compilation,
    nativesharp::diagnostics::DiagnosticId id)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        if (diagnostic.id() == id) return true;
    return false;
}
}

int main()
{
    using namespace nativesharp::frontend;
    using nativesharp::diagnostics::DiagnosticId;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_namespace_function_body_test";

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "valid",
            "namespace Math { int Add(int left, int right) { return left + right; } "
            "long Add(long left, long right) { return left + right; } }\n",
            "namespace App { int Run() { int value = Math.Add(20, 22); return value; } }\n");
        const bool ok = compilation.compileProjectMethodBodies(project, error);
        tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
            "cross-file namespace call and free-function body type-check successfully");
        tests.expect(compilation.methodBodyModel() != nullptr,
            "project exposes the shared body model");
        if (compilation.methodBodyModel() != nullptr)
        {
            const ProjectMethodBodyModel& bodies = *compilation.methodBodyModel();
            tests.expect(bodies.namespaceFunctionCalls().size() == 1,
                "one namespace call receives semantic overload resolution");
            if (!bodies.namespaceFunctionCalls().empty())
            {
                const auto& call = bodies.namespaceFunctionCalls().front();
                tests.expect(call.resolution == MethodCallResolutionKind::Resolved &&
                        call.selectedFunctionOrdinal.has_value(),
                    "integer arguments select one overload");
                tests.expect(call.resultTypeKey == "builtin:int",
                    "selected namespace call propagates its return type");
            }
            tests.expect(bodies.namespaceFunctionBodyAnalyzed(0) &&
                    bodies.namespaceFunctionBodyAnalyzed(1) &&
                    bodies.namespaceFunctionBodyAnalyzed(2),
                "all three namespace function bodies enter the shared analyzer");
        }
    }

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "ambiguous",
            "namespace A { int Pick(int value) { return value; } } "
            "namespace B { int Pick(int value) { return value; } }\n",
            "using A; using B; namespace App { int Run() { return Pick(1); } }\n");
        const bool ok = compilation.compileProjectMethodBodies(project, error);
        tests.expect(!ok, "indistinguishable imported namespace functions are rejected");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::InvalidCall),
            "ambiguous namespace call emits InvalidCall");
    }

    {
        ProjectCompilation compilation;
        std::string error;
        const auto project = createProject(
            root / "conversion",
            "namespace Pointers { int Read(byte* value) { return 0; } }\n",
            "namespace App { int Run() { return Pointers.Read(17); } }\n");
        const bool ok = compilation.compileProjectMethodBodies(project, error);
        tests.expect(!ok, "incompatible namespace function argument is rejected");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::InvalidConversion),
            "incompatible argument emits InvalidConversion");
    }

    std::error_code errorCode;
    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}