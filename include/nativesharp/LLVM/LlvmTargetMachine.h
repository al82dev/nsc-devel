#pragma once

#include "nativesharp/LLVM/LlvmCodeGenFileType.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <filesystem>
#include <memory>
#include <string>

namespace nativesharp::target
{
enum class CpuFeatureLevel;
class TargetInfo;
}

namespace nativesharp::llvm_backend
{
class LlvmModule;
class LlvmNativeSanitizer;
class LlvmOptimizer;
class ProjectOopObjectGenerator;

class LlvmTargetMachine final
{
public:
    LlvmTargetMachine(const LlvmTargetMachine&) = delete;
    LlvmTargetMachine& operator=(const LlvmTargetMachine&) = delete;
    ~LlvmTargetMachine();

    static std::unique_ptr<LlvmTargetMachine> createWindowsX64(std::string& error);
    static std::unique_ptr<LlvmTargetMachine> createWindowsX64(
        LlvmOptimizationLevel level,
        std::string& error);
    static std::unique_ptr<LlvmTargetMachine> createWindowsX64(
        LlvmOptimizationLevel level,
        target::CpuFeatureLevel preferred,
        target::CpuFeatureLevel available,
        std::string& error);
    static std::unique_ptr<LlvmTargetMachine> createWindowsArm64(std::string& error);
    static std::unique_ptr<LlvmTargetMachine> createWindowsArm64(
        LlvmOptimizationLevel level,
        std::string& error);

    const std::string& triple() const noexcept;
    const std::string& dataLayout() const noexcept;
    const std::string& cpuName() const noexcept;
    const std::string& featureString() const noexcept;
    bool usedCpuFeatureFallback() const noexcept;
    LlvmOptimizationLevel optimizationLevel() const noexcept;

    bool configureModuleForTarget(LlvmModule& module, std::string& error) const;

    bool emit(
        LlvmModule& module,
        const std::filesystem::path& outputPath,
        LlvmCodeGenFileType fileType,
        std::string& error) const;

private:
    friend class LlvmOptimizer;
    friend class LlvmNativeSanitizer;
    friend class ProjectOopObjectGenerator;

    LlvmTargetMachine(
        void* targetMachine,
        std::string triple,
        std::string dataLayout,
        std::string cpuName,
        std::string featureString,
        bool usedCpuFeatureFallback,
        LlvmOptimizationLevel optimizationLevel) noexcept;

    static std::unique_ptr<LlvmTargetMachine> createWindows(
        const target::TargetInfo& targetInfo,
        LlvmOptimizationLevel level,
        std::string& error);

    bool emitPrepared(
        LlvmModule& module,
        const std::filesystem::path& outputPath,
        LlvmCodeGenFileType fileType,
        std::string& error) const;

    bool emitImpl(
        LlvmModule& module,
        const std::filesystem::path& outputPath,
        LlvmCodeGenFileType fileType,
        bool prepareModule,
        std::string& error) const;

    void* targetMachine_ = nullptr;
    std::string triple_;
    std::string dataLayout_;
    std::string cpuName_;
    std::string featureString_;
    bool usedCpuFeatureFallback_ = false;
    LlvmOptimizationLevel optimizationLevel_ = LlvmOptimizationLevel::O0;
};
}
