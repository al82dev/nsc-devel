#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace nsc::test
{
class TestContext;

using TestFunction = std::function<void(TestContext&)>;

struct TestCase
{
    std::string id;
    std::string component;
    std::string name;
    std::vector<std::string> tags;
    std::chrono::milliseconds timeout{60000};
    TestFunction function;
};
}
