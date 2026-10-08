#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace nsc::test
{
struct TestResult
{
    std::string id;
    std::string component;
    bool passed = false;
    std::chrono::microseconds duration{0};
    std::vector<std::string> failures;
};
}
