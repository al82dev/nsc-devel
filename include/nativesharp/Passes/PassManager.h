#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::passes
{
class MirPass;
class OptimizationArena;
class OptimizationModule;

class PassManager final
{
public:
    explicit PassManager(OptimizationArena& arena);
    PassManager(const PassManager&) = delete;
    PassManager& operator=(const PassManager&) = delete;
    ~PassManager();

    void addPass(MirPass* pass);
    void addDefaultPipeline();
    void setMaximumIterations(std::size_t value) noexcept;

    bool run(OptimizationModule* module);
    const std::string& error() const noexcept;

private:
    bool fail(std::string message);

    OptimizationArena& arena_;
    std::vector<MirPass*> passes_;
    std::size_t maximumIterations_ = 8;
    std::string error_;
};
}
