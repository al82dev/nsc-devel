#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
class LlvmModule;
}

namespace nativesharp::runtime
{
namespace internal
{
struct RuntimeModuleState;
}

class WindowsRuntimeBuilder final
{
public:
    void setMainArguments(bool enabled) noexcept { mainArguments_ = enabled; }
    std::unique_ptr<llvm_backend::LlvmModule> build(std::string& error) const;
    std::unique_ptr<llvm_backend::LlvmModule> build(
        std::string_view mainSymbol,
        std::string& error) const;
    std::unique_ptr<llvm_backend::LlvmModule> buildSupport(std::string& error) const;
    std::unique_ptr<llvm_backend::LlvmModule> buildEntry(
        std::string_view mainSymbol,
        std::string& error) const;
    // Adapter used when the CRT must initialize a sanitizer before NativeSharp starts.
    std::unique_ptr<llvm_backend::LlvmModule> buildSanitizerEntryAdapter(std::string& error) const;
    // Heap adapter for an explicitly sanitized runtime; ordinary runtime allocation is unchanged.
    std::unique_ptr<llvm_backend::LlvmModule> buildSanitizerHeapAdapter(std::string& error) const;
    std::unique_ptr<llvm_backend::LlvmModule> buildDynamicLibraryAnchor(
        std::string& error) const;

private:
    bool mainArguments_ = false;
    std::unique_ptr<llvm_backend::LlvmModule> finalizeRuntimeModule(
        internal::RuntimeModuleState& state,
        bool includeStart,
        std::string& error) const;

    std::unique_ptr<llvm_backend::LlvmModule> buildInternal(
        std::string_view mainSymbol,
        bool includeStart,
        std::string& error) const;
};
}
