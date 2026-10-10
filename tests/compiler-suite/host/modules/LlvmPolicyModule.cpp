#include "LlvmTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace llvm_support;
using namespace nativesharp::llvm_backend;

void floatingContract(TestContext& context, std::size_t variant)
{
    const std::array<std::pair<std::string_view, FloatingPointMode>, 4> modes = {{
        {"strict", FloatingPointMode::Strict}, {"precise", FloatingPointMode::Precise},
        {"fast", FloatingPointMode::Fast}, {"deterministic", FloatingPointMode::Deterministic}}};
    for (const auto& [name, value] : modes)
    {
        context.expect(parseFloatingPointMode(name) == value, "floating mode parser must accept each canonical name");
        context.expect(floatingPointModeName(value) == name, "floating mode formatter must invert its parser");
    }
    context.expect(!parseFloatingPointMode("FAST").has_value(), "floating mode parsing must remain exact and case-sensitive");
    const FloatingPointState before = captureFloatingPointState();
    {
        FloatingPointModeScope scope;
        scope.set(FloatingPointMode::Deterministic);
        scope.setFmaContractPolicy(FmaContractPolicy::Fast);
        scope.setRoundingMode(FloatingPointRoundingMode::TowardZero);
        scope.setDenormalMode(FloatingPointDenormalMode::FlushToZeroAndDenormalsAreZero);
        context.expect(activeFloatingPointMode() == FloatingPointMode::Deterministic,
            "floating scope must install mode in current thread");
        context.expect(activeFmaContractPolicy() == FmaContractPolicy::Fast,
            "floating scope must install FMA policy in current thread");
    }
    context.expect(captureFloatingPointState() == before, "floating scope must restore the complete previous state");
    if (variant == 8)
        for (std::size_t index = 0; index < 2048; ++index)
            context.expect(parseFloatingPointRoundingMode("nearest-even").has_value(),
                "floating policy parsing must remain deterministic at scale");
}

void ltoContract(TestContext& context, std::size_t variant)
{
    context.expect(linkTimeOptimizationModeName(LinkTimeOptimizationMode::None) == "none", "LTO none name must be exact");
    context.expect(linkTimeOptimizationModeName(LinkTimeOptimizationMode::Full) == "full", "full LTO name must be exact");
    context.expect(linkTimeOptimizationModeName(LinkTimeOptimizationMode::Thin) == "thin", "ThinLTO name must be exact");
    if (variant >= 3)
        context.expect(linkTimeOptimizationModeName(static_cast<LinkTimeOptimizationMode>(255)).empty(),
            "unknown LTO mode must not acquire a name");
}

void optimizationLevelContract(TestContext& context, std::size_t variant)
{
    const std::array<LlvmOptimizationLevel, 4> levels = {LlvmOptimizationLevel::O0, LlvmOptimizationLevel::O1,
        LlvmOptimizationLevel::O2, LlvmOptimizationLevel::O3};
    const std::array<std::string_view, 4> names = {"O0", "O1", "O2", "O3"};
    const std::array<std::string_view, 4> pipelines = {"default<O0>", "default<O1>", "default<O2>", "default<O3>"};
    for (std::size_t index = 0; index < levels.size(); ++index)
    {
        context.expect(llvmOptimizationLevelName(levels[index]) == names[index], "optimization name must be exact");
        context.expect(llvmOptimizationPipelineName(levels[index]) == pipelines[index], "pipeline spelling must be exact");
        context.expect(llvmOptimizationPipelineVersion(levels[index]) == 1, "every current pipeline version must be one");
    }
    const std::string manifest = llvmOptimizationContractManifest();
    context.expect(manifest.starts_with("NSOPTIMIZATION 1.0\n"), "optimization manifest version must be exact");
    context.expect(manifest.find("O3 pipeline=default<O3> version=1") != std::string::npos,
        "manifest must publish the complete O3 contract");
    if (variant == 8) context.expect(manifest == llvmOptimizationContractManifest(),
        "optimization manifest must be byte-deterministic");
}
}

void registerLlvmPolicyModule(TestRegistry& registry)
{
    addLlvmContract(registry, 9, "unit", floatingContract);
    addLlvmContract(registry, 10, "unit", ltoContract);
    addLlvmContract(registry, 11, "unit", optimizationLevelContract);
    addInfrastructureContract(registry, 12, "integration", "loaded-llvm-23-library");
    addInfrastructureContract(registry, 13, "integration", "llvm-context-module-and-verifier");
    addInfrastructureContract(registry, 14, "integration", "llvm-target-initialization-and-machine");
    addInfrastructureContract(registry, 15, "integration", "llvm-object-emission-and-windows-file-lock-injection");
    addInfrastructureContract(registry, 16, "integration", "llvm-address-sanitizer-runtime-and-pipeline");
}
}
