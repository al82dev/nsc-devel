#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::language
{
class ComptimeBudget final
{
public:
    ComptimeBudget(std::size_t steps, std::size_t bytes, std::size_t depth) noexcept;
    bool consume(std::size_t steps, std::size_t bytes, std::size_t depth, std::string& error);
private:
    std::size_t steps_;
    std::size_t bytes_;
    std::size_t depth_;
};
}
