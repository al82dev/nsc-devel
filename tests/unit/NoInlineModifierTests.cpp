#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/InliningPolicy.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <tuple>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& root, std::string_view name, std::string_view source)
{
    const auto directory = root / name;
    writeFile(directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
}

bool diagnosticContains(
    const nativesharp::frontend::ProjectCompilation& compilation, std::string_view text)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        if (diagnostic.message().find(text) != std::string::npos) return true;
    return false;
}
}

int main()
{
    using nativesharp::frontend::ClassMemberKind;
    using nativesharp::frontend::InliningPolicy;
    using nativesharp::frontend::ProjectCompilation;
    using nativesharp::llvm_backend::LlvmOptimizationLevel;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() /
        "nativesharp_noinline_modifier_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    const auto positive = writeProject(root, "NoInlinePositive", R"NS(
public class Math
{
    public static noinline int AddOne(int value) { return value + 1; }
    public static int Use(int value) { return AddOne(value); }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(positive, error);
    tests.expect(ok && error.empty(), "noinline compiles through physical OOP MIR");
    if (ok && compilation.memberModel() != nullptr)
    {
        bool forbidden = false;
        for (const auto& member : compilation.memberModel()->members())
            if (member.kind == ClassMemberKind::Method && member.name == "AddOne")
                forbidden = member.inliningPolicy == InliningPolicy::Forbidden;
        tests.expect(forbidden, "noinline becomes the forbidden inlining policy");

        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
        auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
        tests.expect(module != nullptr && lowerer.error().empty(),
            "noinline lowers to LLVM without backend errors");
        if (module != nullptr)
        {
            tests.expect(module->text().find("noinline") != std::string::npos,
                "forbidden policy emits the LLVM noinline attribute");
            std::string targetError;
            auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
                LlvmOptimizationLevel::O2, targetError);
            nativesharp::llvm_backend::LlvmOptimizer optimizer;
            std::string optimizationError;
            tests.expect(target != nullptr && optimizer.optimize(
                    *module, *target, LlvmOptimizationLevel::O2, optimizationError),
                "the O2 optimization pipeline accepts noinline IR");
            if (target != nullptr)
                tests.expect(module->text().find("call i32") != std::string::npos,
                    "noinline preserves an out-of-line integer call at O2");
        }
    }

    for (const auto& [name, source, message] : {
        std::tuple{"NoInlineAbstract",
            "public abstract class Bad { public noinline abstract int Read(); }",
            "noinline"},
        std::tuple{"NoInlineConflict",
            "public class Bad { public forceinline noinline int Read() { return 1; } }",
            "contradictory"},
        std::tuple{"NoInlineDuplicate",
            "public class Bad { public noinline noinline int Read() { return 1; } }",
            "duplicate noinline"}})
    {
        ProjectCompilation invalid;
        std::string invalidError;
        tests.expect(!invalid.compileProjectMembers(writeProject(root, name, source), invalidError),
            "an impossible or contradictory noinline declaration is rejected");
        tests.expect(diagnosticContains(invalid, message),
            "noinline rejection reports the precise policy error");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
