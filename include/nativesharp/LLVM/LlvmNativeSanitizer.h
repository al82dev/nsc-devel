#pragma once

#include "nativesharp/LLVM/NativeSanitizerKind.h"

#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
class LlvmModule;
class LlvmTargetMachine;

std::string_view nativeSanitizerName(NativeSanitizerKind kind) noexcept;
std::string_view nativeSanitizerPipeline(NativeSanitizerKind kind) noexcept;
std::string_view nativeSanitizerWindowsRuntime(NativeSanitizerKind kind) noexcept;

class LlvmNativeSanitizer final
{
public:
    bool instrument(
        LlvmModule& module,
        const LlvmTargetMachine& targetMachine,
        NativeSanitizerKind kind,
        std::string& error) const;
};
}
