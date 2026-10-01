#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <algorithm>
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

const nativesharp::frontend::ProjectOopMirFunction* findFunction(
    const nativesharp::frontend::ProjectOopMirModel& model, std::string_view fragment)
{
    for (const auto& function : model.functions())
        if (function.mangledName.find(fragment) != std::string::npos) return &function;
    return nullptr;
}

bool hasExit(const nativesharp::frontend::ProjectOopMirFunction* function,
    nativesharp::frontend::ProjectOopMirNonlocalExitKind kind)
{
    return function != nullptr && std::ranges::any_of(function->exceptionFlow.nonlocalExits,
        [kind](const auto& exit) { return exit.kind == kind && exit.node != nullptr; });
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::frontend;
    const auto root = nativesharp::tests::evidenceRoot(
        "nativesharp_item397_mir_exception_flow_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Item397.nsproj",
        "<Project><PropertyGroup><Name>Item397</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
class Problem : Exception { }
class Probe
{
    public static int Run(bool fail)
    {
        try { if (fail) { throw new Problem(); } return 1; }
        catch (Problem problem) { return 2; }
        finally { int completed = 1; }
    }
    public static void Again()
    {
        try { throw new Problem(); }
        catch (Problem problem) { throw; }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(root / "Item397.nsproj", error);
    tests.expect(built && compilation.oopMirModel() != nullptr,
        "exception-flow fixture reaches project MIR: " + error);
    if (!built || compilation.oopMirModel() == nullptr) return tests.resultKeeping(root);
    const ProjectOopMirModel& model = *compilation.oopMirModel();
    const ProjectOopMirFunction* run = findFunction(model, "Run");
    const ProjectOopMirFunction* again = findFunction(model, "Again");

    tests.expect(run != nullptr && run->exceptionFlow.requiresPersonality &&
            !run->exceptionFlow.protectedRegionOrdinals.empty() &&
            run->exceptionFlow.catchRegionCount == 1 &&
            run->exceptionFlow.finallyRegionCount == 1,
        "MIR freezes SEH requirements and exception-region topology");
    tests.expect(hasExit(run, ProjectOopMirNonlocalExitKind::Throw) &&
            hasExit(run, ProjectOopMirNonlocalExitKind::Return),
        "MIR consolidates throwing and structured return exits");
    tests.expect(hasExit(again, ProjectOopMirNonlocalExitKind::Rethrow),
        "MIR preserves rethrow as a distinct nonlocal exit");

    const std::string dump = ProjectOopMirModelDumper::dump(model);
    tests.expect(dump.find("schema=" + std::to_string(kProjectOopMirSchemaMajor) + "." +
                     std::to_string(kProjectOopMirSchemaMinor)) != std::string::npos &&
            dump.find("exception-flow personality=true") != std::string::npos &&
            dump.find("nonlocal-exit kind=rethrow") != std::string::npos,
        "versioned MIR dump exposes exception and nonlocal-flow contracts");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), model);
    tests.expect(module != nullptr,
        "LLVM consumes the MIR SEH requirement without rediscovery: " + lowerer.error());
    return tests.resultKeeping(root);
}
