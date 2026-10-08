#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace nsc::test
{
class TestContext
{
public:
    void fail(std::string message)
    {
        failures_.push_back(std::move(message));
    }

    void expect(bool condition, std::string message)
    {
        if (!condition)
        {
            fail(std::move(message));
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
