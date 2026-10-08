#include "nsc_test/TestRegistry.h"

#include "nsc_test/TestContext.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace nsc::test
{
void TestRegistry::add(TestCase testCase)
{
    if (testCase.id.empty() || testCase.component.empty() || !testCase.function)
    {
        throw std::invalid_argument("test case requires id, component, and function");
    }
    if (find(testCase.id) != nullptr)
    {
        throw std::invalid_argument("duplicate test id: " + testCase.id);
    }
    cases_.push_back(std::move(testCase));
    std::sort(cases_.begin(), cases_.end(), [](const TestCase& left, const TestCase& right) {
        return left.id < right.id;
    });
}

const std::vector<TestCase>& TestRegistry::cases() const noexcept
{
    return cases_;
}

std::vector<const TestCase*> TestRegistry::all() const
{
    std::vector<const TestCase*> selected;
    selected.reserve(cases_.size());
    for (const TestCase& testCase : cases_)
    {
        selected.push_back(&testCase);
    }
    return selected;
}

std::vector<const TestCase*> TestRegistry::byComponent(std::string_view component) const
{
    std::vector<const TestCase*> selected;
    for (const TestCase& testCase : cases_)
    {
        if (testCase.component == component)
        {
            selected.push_back(&testCase);
        }
    }
    return selected;
}

const TestCase* TestRegistry::find(std::string_view id) const noexcept
{
    const auto found = std::find_if(cases_.begin(), cases_.end(), [id](const TestCase& testCase) {
        return testCase.id == id;
    });
    return found == cases_.end() ? nullptr : &*found;
}

std::vector<TestResult> TestRegistry::run(const std::vector<const TestCase*>& selected) const
{
    std::vector<TestResult> results;
    results.reserve(selected.size());
    for (const TestCase* testCase : selected)
    {
        TestContext context;
        const auto started = std::chrono::steady_clock::now();
        try
        {
            testCase->function(context);
        }
        catch (const std::exception& error)
        {
            context.fail(std::string("uncaught exception: ") + error.what());
        }
        catch (...)
        {
            context.fail("uncaught non-standard exception");
        }
        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started);
        results.push_back(TestResult{
            testCase->id,
            testCase->component,
            context.passed(),
            duration,
            context.failures(),
        });
    }
    return results;
}
}
