#pragma once

#include <string>

namespace nativesharp::runtime
{
class WindowsRuntimeBuilder;
}

namespace nativesharp::llvm_backend
{
class LlvmLowerer;
class ProjectOopLlvmLowerer;
class LlvmOptimizer;
class LlvmTargetMachine;

class LlvmModule final
{
public:
    LlvmModule(const LlvmModule&) = delete;
    LlvmModule& operator=(const LlvmModule&) = delete;
    ~LlvmModule();

    bool verify(std::string& error) const;
    std::string text() const;

private:
    friend class LlvmLowerer;
    friend class ProjectOopLlvmLowerer;
class ProjectOopLlvmLowerer;
    friend class LlvmOptimizer;
    friend class LlvmTargetMachine;
    friend class runtime::WindowsRuntimeBuilder;

    LlvmModule(void* context, void* module) noexcept;

    void* context_ = nullptr;
    void* module_ = nullptr;
};
}
