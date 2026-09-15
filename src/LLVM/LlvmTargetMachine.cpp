#include "nativesharp/LLVM/LlvmTargetMachine.h"

#include "LlvmCApi.h"

#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/Target/TargetInfo.h"

#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace nativesharp::llvm_backend
{
namespace
{
std::once_flag gX86Initialization;
std::once_flag gAArch64Initialization;

void initializeX86Backend()
{
    llvm_api::LLVMInitializeX86TargetInfo();
    llvm_api::LLVMInitializeX86Target();
    llvm_api::LLVMInitializeX86TargetMC();
    llvm_api::LLVMInitializeX86AsmParser();
    llvm_api::LLVMInitializeX86AsmPrinter();
}

void initializeAArch64Backend()
{
    llvm_api::LLVMInitializeAArch64TargetInfo();
    llvm_api::LLVMInitializeAArch64Target();
    llvm_api::LLVMInitializeAArch64TargetMC();
    llvm_api::LLVMInitializeAArch64AsmParser();
    llvm_api::LLVMInitializeAArch64AsmPrinter();
}

llvm_api::LLVMCodeGenOptLevel codeGenerationLevel(LlvmOptimizationLevel level)
{
    switch (level)
    {
        case LlvmOptimizationLevel::O0:
            return llvm_api::LLVMCodeGenLevelNone;
        case LlvmOptimizationLevel::O1:
            return llvm_api::LLVMCodeGenLevelLess;
        case LlvmOptimizationLevel::O2:
            return llvm_api::LLVMCodeGenLevelDefault;
        case LlvmOptimizationLevel::O3:
            return llvm_api::LLVMCodeGenLevelAggressive;
    }
    return llvm_api::LLVMCodeGenLevelNone;
}
}

LlvmTargetMachine::LlvmTargetMachine(
    void* targetMachine,
    std::string triple,
    std::string dataLayout,
    std::string cpuName,
    std::string featureString,
    bool usedCpuFeatureFallback,
    LlvmOptimizationLevel optimizationLevel) noexcept
    : targetMachine_(targetMachine),
      triple_(std::move(triple)),
      dataLayout_(std::move(dataLayout)),
      cpuName_(std::move(cpuName)),
      featureString_(std::move(featureString)),
      usedCpuFeatureFallback_(usedCpuFeatureFallback),
      optimizationLevel_(optimizationLevel)
{
}

LlvmTargetMachine::~LlvmTargetMachine()
{
    if (targetMachine_ != nullptr)
    {
        llvm_api::LLVMDisposeTargetMachine(
            static_cast<llvm_api::LLVMTargetMachineRef>(targetMachine_));
    }
}

std::unique_ptr<LlvmTargetMachine> LlvmTargetMachine::createWindowsX64(std::string& error)
{
    return createWindowsX64(LlvmOptimizationLevel::O0, error);
}

std::unique_ptr<LlvmTargetMachine> LlvmTargetMachine::createWindowsX64(
    LlvmOptimizationLevel level,
    std::string& error)
{
    return createWindows(target::TargetInfo::WindowsX64(), level, error);
}

std::unique_ptr<LlvmTargetMachine> LlvmTargetMachine::createWindowsX64(
    LlvmOptimizationLevel level,
    target::CpuFeatureLevel preferred,
    target::CpuFeatureLevel available,
    std::string& error)
{
    return createWindows(target::TargetInfo::WindowsX64(preferred, available), level, error);
}

std::unique_ptr<LlvmTargetMachine> LlvmTargetMachine::createWindowsArm64(std::string& error)
{
    return createWindowsArm64(LlvmOptimizationLevel::O0, error);
}

std::unique_ptr<LlvmTargetMachine> LlvmTargetMachine::createWindowsArm64(
    LlvmOptimizationLevel level,
    std::string& error)
{
    return createWindows(target::TargetInfo::WindowsArm64(), level, error);
}

std::unique_ptr<LlvmTargetMachine> LlvmTargetMachine::createWindows(
    const target::TargetInfo& targetInfo,
    LlvmOptimizationLevel level,
    std::string& error)
{
    error.clear();
    if (llvmOptimizationLevelName(level).empty())
    {
        error = "unsupported LLVM optimization level for Windows " +
            std::string(targetInfo.architectureName()) + " TargetMachine";
        return nullptr;
    }

    if (targetInfo.architecture() == target::Architecture::Arm64)
        std::call_once(gAArch64Initialization, initializeAArch64Backend);
    else
        std::call_once(gX86Initialization, initializeX86Backend);
    const std::string triple(targetInfo.llvmTriple());

    llvm_api::LLVMTargetRef target = nullptr;
    char* message = nullptr;
    if (llvm_api::LLVMGetTargetFromTriple(triple.c_str(), &target, &message) != 0 || target == nullptr)
    {
        error = message != nullptr ? message :
            "LLVM could not resolve the Windows " + std::string(targetInfo.architectureName()) + " target";
        if (message != nullptr)
        {
            llvm_api::LLVMDisposeMessage(message);
        }
        return nullptr;
    }
    if (message != nullptr)
    {
        llvm_api::LLVMDisposeMessage(message);
    }

    const std::string cpuName(targetInfo.llvmCpuName());
    const std::string featureString(targetInfo.llvmFeatureString());
    llvm_api::LLVMTargetMachineRef machine = llvm_api::LLVMCreateTargetMachine(
        target,
        triple.c_str(),
        cpuName.c_str(),
        featureString.c_str(),
        codeGenerationLevel(level),
        llvm_api::LLVMRelocDefault,
        llvm_api::LLVMCodeModelDefault);
    if (machine == nullptr)
    {
        error = "LLVMCreateTargetMachine failed for Windows " +
            std::string(targetInfo.architectureName());
        return nullptr;
    }

    llvm_api::LLVMTargetDataRef targetData = llvm_api::LLVMCreateTargetDataLayout(machine);
    if (targetData == nullptr)
    {
        llvm_api::LLVMDisposeTargetMachine(machine);
        error = "LLVMCreateTargetDataLayout failed for Windows " +
            std::string(targetInfo.architectureName());
        return nullptr;
    }

    char* layoutText = llvm_api::LLVMCopyStringRepOfTargetData(targetData);
    llvm_api::LLVMDisposeTargetData(targetData);
    if (layoutText == nullptr)
    {
        llvm_api::LLVMDisposeTargetMachine(machine);
        error = "LLVM did not provide a target data layout";
        return nullptr;
    }

    std::string dataLayout(layoutText);
    llvm_api::LLVMDisposeMessage(layoutText);
    return std::unique_ptr<LlvmTargetMachine>(
        new LlvmTargetMachine(
            machine, triple, std::move(dataLayout), cpuName, featureString,
            targetInfo.usedCpuFeatureFallback(), level));
}

const std::string& LlvmTargetMachine::triple() const noexcept
{
    return triple_;
}

const std::string& LlvmTargetMachine::dataLayout() const noexcept
{
    return dataLayout_;
}

LlvmOptimizationLevel LlvmTargetMachine::optimizationLevel() const noexcept
{
    return optimizationLevel_;
}

bool LlvmTargetMachine::configureModuleForTarget(LlvmModule& module, std::string& error) const
{
    error.clear();
    if (targetMachine_ == nullptr)
    {
        error = "LLVM target machine is null";
        return false;
    }
    if (module.module_ == nullptr)
    {
        error = "LLVM module is null";
        return false;
    }

    llvm_api::LLVMSetTarget(
        static_cast<llvm_api::LLVMModuleRef>(module.module_),
        triple_.c_str());
    llvm_api::LLVMTargetDataRef targetData = llvm_api::LLVMCreateTargetDataLayout(
        static_cast<llvm_api::LLVMTargetMachineRef>(targetMachine_));
    if (targetData == nullptr)
    {
        error = "LLVMCreateTargetDataLayout failed while configuring module";
        return false;
    }
    llvm_api::LLVMSetModuleDataLayout(
        static_cast<llvm_api::LLVMModuleRef>(module.module_),
        targetData);
    llvm_api::LLVMDisposeTargetData(targetData);
    return true;
}

bool LlvmTargetMachine::emit(
    LlvmModule& module,
    const std::filesystem::path& outputPath,
    LlvmCodeGenFileType fileType,
    std::string& error) const
{
    error.clear();
    if (outputPath.empty())
    {
        error = "code generation output path is empty";
        return false;
    }

    std::string verificationError;
    if (!module.verify(verificationError))
    {
        error = "refusing to emit an invalid LLVM module: " + verificationError;
        return false;
    }

    std::string configurationError;
    if (!configureModuleForTarget(module, configurationError))
    {
        error = configurationError;
        return false;
    }

    std::string mutablePath = outputPath.string();
    const llvm_api::LLVMCodeGenFileType llvmFileType =
        fileType == LlvmCodeGenFileType::Object
            ? llvm_api::LLVMObjectFile
            : llvm_api::LLVMAssemblyFile;

    // Windows can leave a just-verified COFF file mapped for a few milliseconds. Re-emitting
    // the same deterministic fixture then fails in LLVM before code generation with
    // ERROR_USER_MAPPED_FILE. This is a transient output-handle condition, not an IR error;
    // retry only that exact family of sharing diagnostics and preserve every other failure.
    constexpr int kMaximumOutputLockAttempts = 100;
    for (int attempt = 0; attempt != kMaximumOutputLockAttempts; ++attempt)
    {
        char* message = nullptr;
        const int failed = llvm_api::LLVMTargetMachineEmitToFile(
            static_cast<llvm_api::LLVMTargetMachineRef>(targetMachine_),
            static_cast<llvm_api::LLVMModuleRef>(module.module_),
            mutablePath.data(),
            llvmFileType,
            &message);
        const std::string attemptError =
            message != nullptr ? message : "LLVM target emission failed";
        if (message != nullptr) llvm_api::LLVMDisposeMessage(message);
        if (failed == 0) return true;

        const bool transientOutputLock =
            attemptError.find("user-mapped section open") != std::string::npos ||
            attemptError.find("sharing violation") != std::string::npos;
        if (!transientOutputLock || attempt + 1 == kMaximumOutputLockAttempts)
        {
            error = attemptError;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    error = "LLVM target emission exhausted its transient output-lock retry budget";
    return false;
}

const std::string& LlvmTargetMachine::cpuName() const noexcept
{
    return cpuName_;
}

const std::string& LlvmTargetMachine::featureString() const noexcept
{
    return featureString_;
}

bool LlvmTargetMachine::usedCpuFeatureFallback() const noexcept
{
    return usedCpuFeatureFallback_;
}
}
