#pragma once

#include <string>

namespace nativesharp::runtime
{
class WindowsRuntimeBuilder;
}

namespace nativesharp::llvm_backend
{
class LlvmLowerer;
class LlvmModuleInspector;
class LlvmNativeSanitizer;
class ProjectOopLlvmLowerer;
class LlvmOptimizer;
class LlvmTargetMachine;
class ThinLtoBackend;

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
    // EN: Freeze 3.0 DEC-05: read-only CFG, dominance and SSA inspection of the module.
    // ES: Congelamiento 3.0 DEC-05: inspeccion de solo lectura de CFG, dominancia y SSA del modulo.
    friend class LlvmModuleInspector;
    friend class LlvmNativeSanitizer;
    friend class ProjectOopLlvmLowerer;
    friend class LlvmOptimizer;
    friend class LlvmTargetMachine;
    friend class runtime::WindowsRuntimeBuilder;
    friend class ThinLtoBackend;

    LlvmModule(void* context, void* module) noexcept;

    void* context_ = nullptr;
    void* module_ = nullptr;
};
}
