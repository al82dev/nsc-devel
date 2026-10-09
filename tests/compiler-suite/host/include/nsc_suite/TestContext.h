#pragma once

#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
class TestContext
{
public:
    void expect(bool condition, std::string message)
    {
        if (!condition)
        {
            failures_.push_back(std::move(message));
        }
    }

    [[nodiscard]] bool passed() const noexcept
    {
        return failures_.empty();
    }

    [[nodiscard]] const std::vector<std::string>& failures() const noexcept
    {
        return failures_;
    }

private:
    std::vector<std::string> failures_;
};
}
