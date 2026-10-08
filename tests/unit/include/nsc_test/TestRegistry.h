#pragma once

#include "nsc_test/TestCase.h"
#include "nsc_test/TestResult.h"

#include <string_view>
#include <vector>

namespace nsc::test
{
class TestRegistry
{
public:
    void add(TestCase testCase);

    [[nodiscard]] const std::vector<TestCase>& cases() const noexcept;
    [[nodiscard]] std::vector<const TestCase*> all() const;
    [[nodiscard]] std::vector<const TestCase*> byComponent(std::string_view component) const;
    [[nodiscard]] const TestCase* find(std::string_view id) const noexcept;
    [[nodiscard]] std::vector<TestResult> run(const std::vector<const TestCase*>& selected) const;

private:
    std::vector<TestCase> cases_;
};
}
