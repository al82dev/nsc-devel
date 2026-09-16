#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

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
}

int main()
{
    using namespace nativesharp::llvm_backend;
    nativesharp::tests::TestContext tests;
    const auto root = nativesharp::tests::evidenceRoot(
        "nativesharp_item399_llvm_backend_encapsulation_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Item399.nsproj",
        "<Project><PropertyGroup><Name>Item399</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns",
        "class Probe { public static int Run(int value) { return value + 1; } }");

    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(root / "Item399.nsproj", error);
    tests.expect(built && compilation.nameLookup() != nullptr &&
            compilation.memberModel() != nullptr && compilation.oopMirModel() != nullptr,
        "backend fixture reaches physical MIR: " + error);
    if (!built || compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
        return tests.resultKeeping(root);

    ProjectOopLlvmLowerer lowerer;
    const auto noDebug = ProjectOopLlvmLoweringRequest::withoutDebug(
        *compilation.nameLookup(), *compilation.oopMirModel());
    auto module = lowerer.lower(noDebug);
    tests.expect(module != nullptr && lowerer.error().empty(),
        "encapsulated no-debug request lowers through the single backend ingress: " +
            lowerer.error());

    const auto withDebug = ProjectOopLlvmLoweringRequest::withDebug(
        *compilation.nameLookup(), *compilation.memberModel(), *compilation.oopMirModel(),
        compilation.sourceManager(), DebugInformationKind::Full, LlvmOptimizationLevel::O0);
    auto debugModule = lowerer.lower(withDebug);
    tests.expect(debugModule != nullptr && lowerer.error().empty() &&
            debugModule->text().find("!DICompileUnit") != std::string::npos,
        "debug context and options are encapsulated by the request: " + lowerer.error());

    const auto invalidPartition = ProjectOopLlvmLoweringRequest::partition(
        *compilation.nameLookup(), *compilation.memberModel(), *compilation.oopMirModel(),
        compilation.sourceManager(), DebugInformationKind::None, LlvmOptimizationLevel::O0,
        {}, true, "");
    auto rejected = lowerer.lower(invalidPartition);
    tests.expect(rejected == nullptr &&
            lowerer.error().find("requires a stable module name") != std::string::npos,
        "the backend boundary rejects an invalid partition before LLVM state is created");
    return tests.resultKeeping(root);
}
