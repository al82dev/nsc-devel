#include "nativesharp/Driver/CompilationGraphScheduler.h"

#include <algorithm>
#include <exception>
#include <sstream>
#include <stdexcept>

namespace nativesharp::driver
{
std::size_t CompilationGraphScheduler::addTask(
    std::string stableKey, std::span<const std::size_t> dependencies, Work work)
{
    if (stableKey.empty()) throw std::invalid_argument("compilation task key is empty");
    if (!work) throw std::invalid_argument("compilation graph task is empty");
    const std::size_t ordinal = nodes_.size();
    std::vector<std::size_t> copied(dependencies.begin(), dependencies.end());
    std::sort(copied.begin(), copied.end());
    if (std::adjacent_find(copied.begin(), copied.end()) != copied.end())
    {
        throw std::invalid_argument("compilation task has a duplicate dependency");
    }
    if (std::ranges::any_of(copied, [ordinal](std::size_t dependency) {
            return dependency >= ordinal;
        }))
    {
        throw std::invalid_argument(
            "compilation task dependencies must refer to earlier task ordinals");
    }
    nodes_.push_back(Node{std::move(stableKey), std::move(copied), std::move(work)});
    return ordinal;
}

bool CompilationGraphScheduler::run(CompilationWorkerPool& pool, std::string& error)
{
    error.clear();
    submissionOrder_.clear();
    std::vector<std::size_t> remaining(nodes_.size());
    std::vector<std::vector<std::size_t>> dependents(nodes_.size());
    for (std::size_t ordinal = 0; ordinal < nodes_.size(); ++ordinal)
    {
        remaining[ordinal] = nodes_[ordinal].dependencies.size();
        for (const std::size_t dependency : nodes_[ordinal].dependencies)
        {
            dependents[dependency].push_back(ordinal);
        }
    }

    std::vector<bool> submitted(nodes_.size(), false);
    std::size_t completed = 0;
    while (completed != nodes_.size())
    {
        std::vector<std::size_t> ready;
        for (std::size_t ordinal = 0; ordinal < nodes_.size(); ++ordinal)
        {
            if (!submitted[ordinal] && remaining[ordinal] == 0) ready.push_back(ordinal);
        }
        std::ranges::sort(ready, [this](std::size_t left, std::size_t right) {
            if (nodes_[left].stableKey != nodes_[right].stableKey)
                return nodes_[left].stableKey < nodes_[right].stableKey;
            return left < right;
        });
        if (ready.empty())
        {
            error = "compilation task graph contains a dependency cycle";
            return false;
        }
        for (const std::size_t ordinal : ready)
        {
            submitted[ordinal] = true;
            submissionOrder_.push_back(ordinal);
            pool.submit(nodes_[ordinal].work);
        }
        try
        {
            pool.wait();
        }
        catch (const std::exception& exception)
        {
            error = "compilation task wave failed: " + std::string(exception.what());
            return false;
        }
        catch (...)
        {
            error = "compilation task wave failed with a non-standard exception";
            return false;
        }
        for (const std::size_t ordinal : ready)
        {
            ++completed;
            for (const std::size_t dependent : dependents[ordinal]) --remaining[dependent];
        }
    }
    return true;
}

std::span<const std::size_t> CompilationGraphScheduler::submissionOrder() const noexcept
{
    return submissionOrder_;
}

std::size_t CompilationGraphScheduler::taskCount() const noexcept
{
    return nodes_.size();
}
}
