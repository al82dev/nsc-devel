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
            line.find(physicalName) != std::string_view::npos) return true;
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
        "nativesharp_struct_interface_inlining_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "StructInterfaceInlining.nsproj",
        "<Project><PropertyGroup><Name>StructInterfaceInlining</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
public struct Pair
{
    public int Left;
    public int Right;
    public Pair(int left, int right) { Left = left; Right = right; }
    public forceinline int Sum() { return Left + Right; }
}

public interface IValue
{
    int Read();
}

public class Counter : IValue
{
    public forceinline int Read() { return 7; }
}

public class Program
{
    public static int Run(int value)
    {
        Pair pair = new Pair(value, 1);
        return pair.Sum() + ((IValue)new Counter()).Read();
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(
        root / "StructInterfaceInlining.nsproj", error);
    tests.expect(ok && error.empty(),
        "struct and exact interface calls compile through OOP MIR");
    if (ok && compilation.oopMirModel() != nullptr)
    {
        std::string structMethod;
        std::string interfaceImplementation;
        // Both methods are instance methods and therefore carry the implicit receiver outside the
        // explicit parameter list. Distinguish them by their physical declaring type names.
        for (const auto& function : compilation.oopMirModel()->functions())
        {
            if (function.inliningPolicy != InliningPolicy::Required) continue;
            if (function.mangledName.find("Pair") != std::string::npos)
                structMethod = function.mangledName;
            if (function.mangledName.find("Counter") != std::string::npos)
                interfaceImplementation = function.mangledName;
        }
        tests.expect(!structMethod.empty() && !interfaceImplementation.empty(),
            "both concrete forceinline implementations have physical functions");

        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
        auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
        tests.expect(module != nullptr && lowerer.error().empty(),
            "struct and exact interface calls lower to LLVM");
        if (module != nullptr)
        {
            const std::string raw = module->text();
            tests.expect(hasCallTo(raw, structMethod),
                "the struct call targets its concrete implementation before optimization");
            tests.expect(hasCallTo(raw, interfaceImplementation),
                "an interface cast of a fresh object devirtualizes to its concrete implementation");
            tests.expect(raw.find("interface.map.inrange") == std::string::npos,
                "exact construction avoids runtime interface-map dispatch");

            std::string targetError;
            auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
                LlvmOptimizationLevel::O0, targetError);
            nativesharp::llvm_backend::LlvmOptimizer optimizer;
            std::string optimizationError;
            tests.expect(target != nullptr && optimizer.optimize(
                    *module, *target, LlvmOptimizationLevel::O0, optimizationError),
                "O0 accepts mandatory struct and exact-interface inlining");
            if (target != nullptr)
            {
                const std::string optimized = module->text();
                tests.expect(!hasCallTo(optimized, structMethod) &&
                             !hasCallTo(optimized, interfaceImplementation),
                    "both concrete forceinline calls expand at O0");
            }
        }
    }

    const auto dynamicRoot = root / "Dynamic";
    writeFile(dynamicRoot / "DynamicInterface.nsproj",
        "<Project><PropertyGroup><Name>DynamicInterface</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(dynamicRoot / "Main.ns", R"NS(
public interface IValue { int Read(); }
public class First : IValue { public forceinline int Read() { return 1; } }
public class Second : IValue { public forceinline int Read() { return 2; } }
public class Program
{
    public static int Dynamic(IValue value) { return value.Read(); }
}
)NS");
    ProjectCompilation dynamicCompilation;
    std::string dynamicError;
    const bool dynamicOk = dynamicCompilation.compileProjectOopMir(
        dynamicRoot / "DynamicInterface.nsproj", dynamicError);
    tests.expect(dynamicOk && dynamicError.empty(),
        "a polymorphic interface receiver remains a valid dynamic call");
    if (dynamicOk)
    {
        nativesharp::llvm_backend::ProjectOopLlvmLowerer dynamicLowerer;
        auto dynamicModule = dynamicLowerer.lower(
            *dynamicCompilation.nameLookup(), *dynamicCompilation.oopMirModel());
        tests.expect(dynamicModule != nullptr && dynamicLowerer.error().empty() &&
                     dynamicModule->text().find("interface.map.inrange") != std::string::npos,
            "an interface parameter retains runtime interface-map dispatch");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
