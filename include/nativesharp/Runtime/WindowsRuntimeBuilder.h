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
class WindowsRuntimeBuilder final
{
public:
    std::unique_ptr<llvm_backend::LlvmModule> build(std::string& error) const;
    std::unique_ptr<llvm_backend::LlvmModule> build(
        std::string_view mainSymbol,
        std::string& error) const;
    std::unique_ptr<llvm_backend::LlvmModule> buildSupport(std::string& error) const;
    std::unique_ptr<llvm_backend::LlvmModule> buildEntry(
        std::string_view mainSymbol,
        std::string& error) const;
    std::unique_ptr<llvm_backend::LlvmModule> buildDynamicLibraryAnchor(
        std::string& error) const;

private:
    std::unique_ptr<llvm_backend::LlvmModule> buildInternal(
        std::string_view mainSymbol,
        bool includeStart,
        std::string& error) const;
};
}
