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

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

bool hasCallTo(std::string_view ir, std::string_view physicalName)
{
    std::size_t begin = 0;
    while (begin < ir.size())
    {
        const std::size_t end = ir.find('\n', begin);
        const std::string_view line = ir.substr(begin, end == std::string_view::npos
            ? ir.size() - begin : end - begin);
        if (line.find("call ") != std::string_view::npos &&
            line.find(physicalName) != std::string_view::npos)
        {
            return true;
        }
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return false;
}
}

int main()
{
    using nativesharp::frontend::InliningPolicy;
    using nativesharp::frontend::ProjectCompilation;
    using nativesharp::llvm_backend::LlvmOptimizationLevel;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() /
        "nativesharp_generic_inlining_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "GenericInlining.nsproj",
        "<Project><PropertyGroup><Name>GenericInlining</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
public class Algorithms
{
    public static inline T Preferred<T>(T value) { return value; }
    public static forceinline T Required<T>(T value) { return value; }
    public static noinline T Forbidden<T>(T value) { return value; }

    public static int Run(int value)
    {
        int preferred = Preferred(value);
        int required = Required<int>(preferred);
        return Forbidden(required);
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(root / "GenericInlining.nsproj", error);
    tests.expect(ok && error.empty(),
        "explicit and inferred generic inline policies compile through OOP MIR");
    if (ok && compilation.oopMirModel() != nullptr)
    {
        std::string preferredName;
        std::string requiredName;
        std::string forbiddenName;
        for (const auto& function : compilation.oopMirModel()->functions())
        {
            if (function.genericSpecializationKey.find("Preferred``1<int>") != std::string::npos)
            {
                tests.expect(function.inliningPolicy == InliningPolicy::Preferred,
                    "inferred Preferred<int> keeps the preferred policy");
                preferredName = function.mangledName;
            }
            if (function.genericSpecializationKey.find("Required``1<int>") != std::string::npos)
            {
                tests.expect(function.inliningPolicy == InliningPolicy::Required,
                    "explicit Required<int> keeps the required policy");
                requiredName = function.mangledName;
            }
            if (function.genericSpecializationKey.find("Forbidden``1<int>") != std::string::npos)
            {
                tests.expect(function.inliningPolicy == InliningPolicy::Forbidden,
                    "inferred Forbidden<int> keeps the forbidden policy");
                forbiddenName = function.mangledName;
            }
        }
        tests.expect(!preferredName.empty() && !requiredName.empty() && !forbiddenName.empty(),
            "all three closed generic methods have physical functions");

        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
        auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
        tests.expect(module != nullptr && lowerer.error().empty(),
            "closed generic policies lower to LLVM");
        if (module != nullptr)
        {
            const std::string raw = module->text();
            tests.expect(raw.find("inlinehint") != std::string::npos &&
                         raw.find("alwaysinline") != std::string::npos &&
                         raw.find("noinline") != std::string::npos,
                "closed specializations emit all three LLVM attributes");
            tests.expect(hasCallTo(raw, preferredName) && hasCallTo(raw, requiredName) &&
                         hasCallTo(raw, forbiddenName),
                "all closed generic calls exist before optimization");

            std::string targetError;
            auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
                LlvmOptimizationLevel::O0, targetError);
            nativesharp::llvm_backend::LlvmOptimizer optimizer;
            std::string optimizationError;
            tests.expect(target != nullptr && optimizer.optimize(
                    *module, *target, LlvmOptimizationLevel::O0, optimizationError),
                "O0 accepts the closed generic inlining policies");
            if (target != nullptr)
            {
                const std::string optimized = module->text();
                tests.expect(hasCallTo(optimized, preferredName),
                    "preferred generic call remains optional at O0");
                tests.expect(!hasCallTo(optimized, requiredName),
                    "required generic call expands at O0");
                tests.expect(hasCallTo(optimized, forbiddenName),
                    "forbidden generic call remains out of line at O0");
            }
        }
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
