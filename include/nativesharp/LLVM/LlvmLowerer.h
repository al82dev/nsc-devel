#pragma once

#include <memory>
#include <span>
#include <string>

namespace nativesharp::passes
{
class OptimizationModule;
}

namespace nativesharp::sema
{
class Symbol;
}

namespace nativesharp::llvm_backend
{
class LlvmModule;

class LlvmLowerer final
{
public:
    explicit LlvmLowerer(std::span<sema::Symbol* const> globalSymbols) noexcept;
    LlvmLowerer(const LlvmLowerer&) = delete;
    LlvmLowerer& operator=(const LlvmLowerer&) = delete;
    ~LlvmLowerer();

    std::unique_ptr<LlvmModule> lower(const passes::OptimizationModule* optimizedModule);
    const std::string& error() const noexcept;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
    std::string error_;
};
}
