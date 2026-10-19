#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::language
{
class InstantiationBudget final
{
public:
    InstantiationBudget(std::size_t maximumDepth, std::size_t maximumCount) noexcept;
    // NSC305-G08. `logicalDepth` is the caller-supplied instantiation depth (0 = derive from the
    // internal stack, the recursion-based default). The wave scheduler passes the demand's depth so
    // the depth limit and maximumObservedDepth survive the two-pass materialization.
    bool enter(std::string identity, std::size_t logicalDepth, std::string& error);
    bool enter(std::string identity, std::string& error);
    void leave() noexcept;
    std::size_t count() const noexcept { return count_; }
    std::size_t maximumDepth() const noexcept { return maximumDepth_; }
    std::size_t maximumCount() const noexcept { return maximumCount_; }
    std::size_t maximumObservedDepth() const noexcept { return maximumObservedDepth_; }
    std::span<const std::string> activeStack() const noexcept { return stack_; }
private:
    std::size_t maximumDepth_;
    std::size_t maximumCount_;
    std::size_t count_ = 0;
    std::size_t maximumObservedDepth_ = 0;
    std::vector<std::string> stack_;
};
}
