#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"
#include "../support/TestContext.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char** argv)
{
    using namespace nativesharp;
    using namespace nativesharp::llvm_backend;
    tests::TestContext tests;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() / ("nsc-asan-unit-" + std::to_string(stamp));
    std::filesystem::create_directories(root);
    std::ofstream(root / "Probe.nsproj") <<
        "<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>";
    const bool safeFixture = argc == 3 && std::string(argv[2]) == "safe";
    std::ofstream(root / "Probe.ns") <<
        (safeFixture
            ? "public class Probe { public static int Main() { int* p = stackalloc int[2]; p[0] = 42; return p[0]; } }"
            : "public class Probe { public static int Main() { int* p = stackalloc int[2]; "
              "int index = 3; p[index] = 42; return p[0]; } }");
    frontend::ProjectCompilation compilation;
    std::string error;
    const bool compiled = compilation.compileProjectOopMir(root / "Probe.nsproj", error);
    tests.expect(compiled && compilation.oopMirModel() != nullptr, "fixture builds MIR: " + error);
    if (!compiled || compilation.oopMirModel() == nullptr) return tests.result();

    CompilationArtifactCache cache;
    cache.setRoot(root / "object-cache");
    cache.setCodegenIdentity("asan-isolation-test");
    CompilationArtifactCacheScope scope(cache);
    ProjectOopObjectGenerator normal;
    ProjectOopObjectGenerator asan;
    asan.setAddressSanitizer(true);
    const std::filesystem::path fixtureOutput = argc >= 2 ? std::filesystem::path(argv[1]) : std::filesystem::path{};
    if (!fixtureOutput.empty()) std::filesystem::create_directories(fixtureOutput);
    const auto generate = [&](ProjectOopObjectGenerator& generator, LlvmOptimizationLevel level)
    {
        error.clear();
        return generator.generate(*compilation.nameLookup(), *compilation.memberModel(),
            *compilation.oopMirModel(), compilation.sourceManager(), DebugInformationKind::None,
            level, 2, {}, error);
    };
    const auto containsAsan = [](const ProjectOopObjectBundle& bundle, bool expected)
    {
        if (bundle.objectFiles().empty()) return false;
        for (const auto& path : bundle.objectFiles())
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream.is_open()) return false;
            const std::string bytes{std::istreambuf_iterator<char>(stream), {}};
            if ((bytes.find("__asan_init") != std::string::npos) != expected) return false;
        }
        return true;
    };
    const auto containsReport = [](const ProjectOopObjectBundle& bundle)
    {
        for (const auto& path : bundle.objectFiles())
        {
            std::ifstream stream(path, std::ios::binary);
            const std::string bytes{std::istreambuf_iterator<char>(stream), {}};
            if (bytes.find("__asan_report") != std::string::npos) return true;
        }
        return false;
    };
    for (const auto level : {LlvmOptimizationLevel::O0, LlvmOptimizationLevel::O2})
    {
        auto plain = generate(normal, level);
        tests.expect(plain != nullptr && containsAsan(*plain, false), "normal objects lack ASan: " + error);
        auto warmed = generate(normal, level);
        tests.expect(warmed != nullptr && warmed->cacheHitCount() == warmed->objectFiles().size(),
            "ordinary objects are served from cache: " + error);
        auto instrumented = generate(asan, level);
        tests.expect(instrumented != nullptr && instrumented->cacheHitCount() == 0 &&
            containsAsan(*instrumented, true), "every ASan object is instrumented and avoids cache: " + error);
        if (!safeFixture)
            tests.expect(instrumented != nullptr && containsReport(*instrumented),
                "out-of-bounds write retains an ASan report after optimization: " + error);
        if (instrumented != nullptr && !fixtureOutput.empty())
        {
            const auto directory = fixtureOutput / (level == LlvmOptimizationLevel::O0 ? "O0" : "O2");
            std::filesystem::create_directories(directory);
            std::size_t index = 0;
            for (const auto& object : instrumented->objectFiles())
                std::filesystem::copy_file(object, directory / ("unit-" + std::to_string(index++) + ".obj"),
                    std::filesystem::copy_options::overwrite_existing);
            std::string mainSymbol;
            for (const auto& function : compilation.oopMirModel()->functions())
                if (function.mangledName.find("Probe__Main") != std::string::npos) mainSymbol = function.mangledName;
            runtime::WindowsRuntimeBuilder builder;
            auto entry = builder.buildEntry(mainSymbol, error);
            auto target = LlvmTargetMachine::createWindowsX64(level, error);
            const bool emitted = !mainSymbol.empty() && entry != nullptr && target != nullptr &&
                target->emit(*entry, directory / "entry.obj", LlvmCodeGenFileType::Object, error);
            tests.expect(emitted, "real NativeSharp entry object emitted: " + error);
        }
        auto after = generate(normal, level);
        tests.expect(after != nullptr && after->cacheHitCount() == after->objectFiles().size() &&
            containsAsan(*after, false), "ASan does not contaminate ordinary object cache: " + error);
    }
    error.clear();
    auto unsupported = asan.generate(*compilation.nameLookup(), *compilation.memberModel(),
        *compilation.oopMirModel(), compilation.sourceManager(), DebugInformationKind::None,
        LlvmOptimizationLevel::O2, 2, {}, LinkTimeOptimizationMode::Thin, error);
    tests.expect(unsupported == nullptr && error.find("AddressSanitizer") != std::string::npos,
        "unsupported ASan and LTO combination is rejected");
    return tests.result();
}
