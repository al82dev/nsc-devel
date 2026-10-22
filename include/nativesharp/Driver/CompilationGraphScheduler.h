#pragma once

#include "nativesharp/Driver/CompilationWorkerPool.h"

#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::driver
{
class CompilationGraphScheduler final
{
public:
    using Work = std::function<void(std::size_t workerIndex)>;

    std::size_t addTask(
        std::string stableKey, std::span<const std::size_t> dependencies, Work work);
    bool run(CompilationWorkerPool& pool, std::string& error);
    void clear() noexcept;

    std::span<const std::size_t> submissionOrder() const noexcept;
    std::size_t taskCount() const noexcept;

private:
    struct Node final
    {
        std::string stableKey;
        std::vector<std::size_t> dependencies;
        Work work;
    };

    std::vector<Node> nodes_;
    std::vector<std::size_t> submissionOrder_;
};
}
