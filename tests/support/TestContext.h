#pragma once

#include <iostream>
#include <string_view>

namespace nativesharp::tests
{
class TestContext final
{
public:
    void expect(bool condition, std::string_view message)
    {
        ++checks_;
        if (!condition)
        {
            ++failures_;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    int result() const noexcept
    {
        if (failures_ == 0)
        {
            std::cout << "PASS: " << checks_ << " checks\n";
            return 0;
        }

        std::cerr << "FAILED: " << failures_ << " of " << checks_ << " checks\n";
        return 1;
    }

private:
    int checks_ = 0;
    int failures_ = 0;
};
}
