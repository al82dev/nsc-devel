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
        "nativesharp_forceinline_modifier_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    const auto positive = writeProject(root, "ForceInlinePositive", R"NS(
public class Math
{
    public static forceinline int AddOne(int value) { return value + 1; }
    public static int Main() { return AddOne(41) - 42; }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(positive, error);
    tests.expect(ok && error.empty(), "forceinline compiles through physical OOP MIR");
    if (ok && compilation.memberModel() != nullptr)
    {
        bool required = false;
        for (const auto& member : compilation.memberModel()->members())
            if (member.kind == ClassMemberKind::Method && member.name == "AddOne")
                required = member.inliningPolicy == InliningPolicy::Required;
        tests.expect(required, "forceinline becomes the required inlining policy");

        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
        auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
        tests.expect(module != nullptr && lowerer.error().empty(),
            "forceinline lowers to LLVM without backend errors");
        if (module != nullptr)
        {
            tests.expect(module->text().find("alwaysinline") != std::string::npos,
                "required policy emits the LLVM alwaysinline attribute");
            std::string targetError;
            auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
                LlvmOptimizationLevel::O0, targetError);
            nativesharp::llvm_backend::LlvmOptimizer optimizer;
            std::string optimizationError;
            tests.expect(target != nullptr && optimizer.optimize(
                    *module, *target, LlvmOptimizationLevel::O0, optimizationError),
                "the O0 mandatory-inlining pipeline accepts forceinline IR");
            if (target != nullptr)
                tests.expect(module->text().find("call i32") == std::string::npos,
                    "forceinline expands the integer call even when automatic O0 inlining is disabled");
        }
    }

    for (const auto& [name, source, message] : {
        std::tuple{"ForceInlineAbstract",
            "public abstract class Bad { public forceinline abstract int Read(); }",
            "forceinline"},
        std::tuple{"ForceInlineConflict",
            "public class Bad { public inline forceinline int Read() { return 1; } }",
            "contradictory"},
        std::tuple{"ForceInlineDirectRecursion",
            "public class Bad { public static forceinline int Read(int n) { "
            "return n == 0 ? 0 : Read(n - 1); } }",
            "recursive forceinline call cycle"},
        std::tuple{"ForceInlineMutualRecursion",
            "public class Bad { "
            "public static forceinline int Left(int n) { return n == 0 ? 0 : Right(n - 1); } "
            "public static forceinline int Right(int n) { return n == 0 ? 0 : Left(n - 1); } }",
            "recursive forceinline call cycle"}})
    {
        ProjectCompilation invalid;
        std::string invalidError;
        tests.expect(!invalid.compileProjectOopMir(writeProject(root, name, source), invalidError),
            "an impossible or contradictory forceinline declaration is rejected");
        tests.expect(diagnosticContains(invalid, message),
            "forceinline rejection reports the precise policy error");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
