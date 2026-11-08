#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopCodePartition.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/ProjectOopMirCacheIdentity.h"

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
}

int main()
{
    using namespace nativesharp::frontend;
    using namespace nativesharp::llvm_backend;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_namespace_function_llvm_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);
    writeFile(root / "Llvm.nsproj",
        R"(<Project><PropertyGroup><Name>Llvm</Name></PropertyGroup><ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)");
    writeFile(root / "Functions.ns",
        "namespace Math { bool CacheFlag = false; int Add(int left, int right) { return left + right; } } "
        "namespace App { int Run() { return Math.Add(20, 22); } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(root / "Llvm.nsproj", error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "namespace functions compile through MIR for LLVM");
    if (ok && compilation.nameLookup() != nullptr &&
        compilation.memberModel() != nullptr && compilation.oopMirModel() != nullptr)
    {
        const ProjectOopMirModel& mir = *compilation.oopMirModel();
        ProjectOopLlvmLowerer lowerer;
        auto module = lowerer.lower(*compilation.nameLookup(), mir);
        tests.expect(module != nullptr && lowerer.error().empty(),
            "namespace functions lower to LLVM without a declaring type");
        if (module != nullptr)
        {
            std::string verifyError;
            tests.expect(module->verify(verifyError),
                "LLVM verifies namespace function definitions and calls");
            const std::string ir = module->text();
            bool allSymbolsPresent = true;
            for (const ProjectOopMirFunction& function : mir.functions())
            {
                if (!function.namespaceFunctionOrdinal.has_value()) continue;
                allSymbolsPresent =
                    allSymbolsPresent && ir.find(function.mangledName) != std::string::npos;
            }
            tests.expect(allSymbolsPresent,
                "LLVM IR uses every frozen namespace function symbol");
            tests.expect(ir.find("call i32") != std::string::npos,
                "LLVM emits the direct call between namespace functions");
        }

        ProjectOopLlvmLowerer debugLowerer;
        auto debugModule = debugLowerer.lower(
            *compilation.nameLookup(),
            *compilation.memberModel(),
            mir,
            compilation.sourceManager(),
            DebugInformationKind::Full,
            LlvmOptimizationLevel::O0);
        tests.expect(debugModule != nullptr && debugLowerer.error().empty(),
            "full CodeView lowering accepts namespace functions");
        if (debugModule != nullptr)
        {
            const std::string debugIr = debugModule->text();
            tests.expect(debugIr.find("Math.Add") != std::string::npos &&
                    debugIr.find("App.Run") != std::string::npos,
                "debug metadata keeps source-facing namespace names");
        }

        ProjectOopCodePartition partition;
        std::string partitionError;
        tests.expect(partition.build(
                mir, 2, partitionError, ProjectOopCodePartitionPolicy::StableIncremental),
            "code partitioning accepts namespace functions");
        bool dependencyFound = false;
        for (const ProjectOopCodeUnit& unit : partition.units())
        {
            if (unit.declarationOrdinals.size() > unit.functionOrdinals.size())
                dependencyFound = true;
        }
        tests.expect(dependencyFound,
            "partition declarations retain cross-function call dependencies");

        std::vector<std::size_t> ordinals(mir.functions().size());
        for (std::size_t index = 0; index < ordinals.size(); ++index) ordinals[index] = index;
        const std::string identity = projectOopMirCacheIdentity(mir, ordinals, ordinals);
        const ProjectOopMirCacheIdentityIndex identityIndex(mir);
        tests.expect(!identity.empty() && identity == identityIndex.identity(ordinals, ordinals),
            "cache identity index preserves namespace function and call ordinals");

        const auto staticFields = mir.staticFields();
        tests.expect(!staticFields.empty(), "cache identity fixture has namespace state");
        if (!staticFields.empty())
        {
            auto& field = const_cast<ProjectOopMirStaticField&>(staticFields.front());
            const bool saved = field.cppCBoolStorage;
            field.cppCBoolStorage = false;
            const auto ordinary = projectOopMirCacheIdentity(mir, ordinals, ordinals);
            field.cppCBoolStorage = true;
            const auto cByte = projectOopMirCacheIdentity(mir, ordinals, ordinals);
            const ProjectOopMirCacheIdentityIndex cByteIndex(mir);
            tests.expect(ordinary != cByte && cByte == cByteIndex.identity(ordinals, ordinals),
                "cache identity distinguishes C volatile bool byte storage");
            field.cppCBoolStorage = saved;
        }

        const auto functions = mir.functions();
        ProjectOopMirNode* node = functions.empty() ? nullptr : functions.front().body.get();
        tests.expect(node != nullptr, "cache identity test has a MIR body");
        if (node != nullptr)
        {
            const auto savedMode = node->cppMicrosoftVolatileSemantics;
            const auto savedAlignment = node->cppAtomicAlignmentBytes;
            node->cppMicrosoftVolatileSemantics.reset();
            node->cppAtomicAlignmentBytes.reset();
            const auto unknown = projectOopMirCacheIdentity(mir, ordinals, ordinals);
            node->cppMicrosoftVolatileSemantics = false;
            const auto disabled = projectOopMirCacheIdentity(mir, ordinals, ordinals);
            node->cppMicrosoftVolatileSemantics = true;
            const auto enabled = projectOopMirCacheIdentity(mir, ordinals, ordinals);
            node->cppAtomicAlignmentBytes = 1;
            const auto alignedOne = projectOopMirCacheIdentity(mir, ordinals, ordinals);
            node->cppAtomicAlignmentBytes = 4;
            const auto alignedFour = projectOopMirCacheIdentity(mir, ordinals, ordinals);
            const auto savedAtomic = node->cppAtomicAccess;
            node->cppAtomicAccess = !savedAtomic;
            const auto changedAtomic = projectOopMirCacheIdentity(mir, ordinals, ordinals);
            tests.expect(unknown != disabled && unknown != enabled && disabled != enabled &&
                    enabled != alignedOne && alignedOne != alignedFour && alignedFour != changedAtomic,
                "code cache identity distinguishes volatile mode, proven alignment and C atomic access");
            node->cppAtomicAccess = savedAtomic;
            node->cppMicrosoftVolatileSemantics = savedMode;
            node->cppAtomicAlignmentBytes = savedAlignment;
        }
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
