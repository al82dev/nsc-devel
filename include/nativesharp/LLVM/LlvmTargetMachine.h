#pragma once

#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <filesystem>
#include <memory>
#include <string>

namespace nativesharp::llvm_backend
{
class LlvmModule;
class LlvmOptimizer;

enum class LlvmCodeGenFileType
{
    Assembly,
    Object
};

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

    const std::string& triple() const noexcept;
    const std::string& dataLayout() const noexcept;
    LlvmOptimizationLevel optimizationLevel() const noexcept;

    bool configureModuleForTarget(LlvmModule& module, std::string& error) const;

    bool emit(
        LlvmModule& module,
        const std::filesystem::path& outputPath,
        LlvmCodeGenFileType fileType,
        std::string& error) const;

private:
    friend class LlvmOptimizer;

    LlvmTargetMachine(
        void* targetMachine,
        std::string triple,
        std::string dataLayout,
        LlvmOptimizationLevel optimizationLevel) noexcept;

    void* targetMachine_ = nullptr;
    std::string triple_;
    std::string dataLayout_;
    LlvmOptimizationLevel optimizationLevel_ = LlvmOptimizationLevel::O0;
};
}
