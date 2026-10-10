#include "nsc_suite/TestRegistry.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace nsc::suite
{
void TestRegistry::add(TestCase testCase)
{
    if (testCase.id.empty() || testCase.contractId.empty() || testCase.component.empty() ||
        testCase.layer.empty() || testCase.profile.empty() || !testCase.function)
    {
        throw std::invalid_argument("test case metadata is incomplete");
    }
    // Keep the externally visible ID order while locating only this insertion point.
    // Re-sorting the complete registry after every add makes full registration superlinear by an extra log factor.
    const auto insertion = std::lower_bound(
        cases_.begin(), cases_.end(), testCase.id,
        [](const TestCase& candidate, std::string_view id) { return candidate.id < id; });
    if (insertion != cases_.end() && insertion->id == testCase.id)
    {
        throw std::invalid_argument("duplicate test case id: " + testCase.id);
    }
    cases_.insert(insertion, std::move(testCase));
}

const std::vector<TestCase>& TestRegistry::cases() const noexcept
{
    return cases_;
}

const TestCase* TestRegistry::find(std::string_view id) const noexcept
{
    const auto found = std::find_if(cases_.begin(), cases_.end(), [id](const TestCase& testCase) {
        return testCase.id == id;
    });
    return found == cases_.end() ? nullptr : &*found;
}

std::vector<const TestCase*> TestRegistry::all() const
{
    std::vector<const TestCase*> selected;
    selected.reserve(cases_.size());
    for (const TestCase& testCase : cases_)
    {
        if (testCase.disposition == TestDisposition::Required)
        {
            selected.push_back(&testCase);
        }
    }
    return selected;
}

std::vector<const TestCase*> TestRegistry::byComponent(std::string_view component) const
{
    std::vector<const TestCase*> selected;
    for (const TestCase& testCase : cases_)
    {
        if (testCase.component == component &&
            testCase.disposition == TestDisposition::Required)
        {
            selected.push_back(&testCase);
        }
    }
    return selected;
}

std::vector<TestResult> TestRegistry::run(const std::vector<const TestCase*>& selected) const
{
    std::vector<TestResult> results;
    results.reserve(selected.size());
    for (const TestCase* testCase : selected)
    {
        if (testCase == nullptr)
        {
            throw std::invalid_argument("selected test case cannot be null");
        }
        TestContext context;
        const auto started = std::chrono::steady_clock::now();
        try
        {
            testCase->function(context);
        }
        catch (const std::exception& error)
        {
            context.expect(false, std::string("uncaught exception: ") + error.what());
        }
        catch (...)
        {
            context.expect(false, "uncaught non-standard exception");
        }
        results.push_back(TestResult{
            testCase->id,
            testCase->contractId,
            testCase->component,
            context.passed(),
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started),
            context.failures(),
        });
    }
    return results;
}

}
