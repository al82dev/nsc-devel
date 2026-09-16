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
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::frontend;
    const auto root = nativesharp::tests::evidenceRoot(
        "nativesharp_item396_mir_ownership_cleanup_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Item396.nsproj",
        "<Project><PropertyGroup><Name>Item396</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
class Resource
{
    public int Value;
    public Resource(int value) { Value = value; }
    public int Read() { return Value; }
    ~Resource() { Value = 0; }
}
class Api
{
    public static void Consume(owned Resource value) { int seen = value.Read(); }
    public static int Run()
    {
        raii Resource local = new Resource(42);
        try { return local.Read(); }
        finally { int completed = 1; }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(root / "Item396.nsproj", error);
    tests.expect(built && compilation.oopMirModel() != nullptr,
        "ownership/cleanup fixture reaches project MIR: " + error);
    if (!built || compilation.oopMirModel() == nullptr) return tests.resultKeeping(root);
    const ProjectOopMirModel& model = *compilation.oopMirModel();
    const ProjectOopMirFunction* consume = findFunction(model, "Consume");
    const ProjectOopMirFunction* run = findFunction(model, "Run");

    tests.expect(consume != nullptr && consume->cleanupRegions.size() == 1 &&
            consume->cleanupRegions[0].kind == ProjectOopMirCleanupKind::OwnedParameter &&
            consume->cleanupRegions[0].parameterOrdinal == 0 &&
            consume->cleanupRegions[0].action != nullptr,
        "MIR owns the resolved cleanup action for an owned parameter");
    const bool hasLocal = run != nullptr && std::ranges::any_of(run->cleanupRegions,
        [](const ProjectOopMirCleanupRegion& region) {
            return region.kind == ProjectOopMirCleanupKind::LocalOwner &&
                region.localOrdinal.has_value() && region.action != nullptr;
        });
    const bool hasFinally = run != nullptr && std::ranges::any_of(run->cleanupRegions,
        [](const ProjectOopMirCleanupRegion& region) {
            return region.kind == ProjectOopMirCleanupKind::Finally && region.action != nullptr;
        });
    tests.expect(hasLocal && hasFinally,
        "MIR consolidates local ownership and finally cleanup in one ordered contract");
    tests.expect(run != nullptr && std::ranges::is_sorted(run->cleanupRegions, {},
            &ProjectOopMirCleanupRegion::ordinal),
        "MIR cleanup regions have deterministic ordinal order");

    const std::string dump = ProjectOopMirModelDumper::dump(model);
    tests.expect(dump.find("schema=1.10") != std::string::npos &&
            dump.find("kind=owned-parameter") != std::string::npos &&
            dump.find("kind=finally") != std::string::npos,
        "versioned MIR dump exposes ownership and cleanup actions");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), model);
    tests.expect(module != nullptr,
        "LLVM consumes the pre-resolved MIR cleanup contract: " + lowerer.error());
    return tests.resultKeeping(root);
}
