#pragma once

#include "nsc_suite/TestContext.h"

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace nsc::suite
{
using TestFunction = std::function<void(TestContext&)>;

struct TestCase
{
    std::string id;
    std::string contractId;
    std::string component;
    std::string layer;
    std::string profile;
    std::vector<std::string> tags;
    TestFunction function;
};

struct TestResult
{
    std::string id;
    std::string contractId;
    std::string component;
    bool passed = false;
    std::chrono::microseconds duration{};
    std::vector<std::string> failures;
};

class TestRegistry
{
public:
    void add(TestCase testCase);

    [[nodiscard]] const std::vector<TestCase>& cases() const noexcept;
    [[nodiscard]] const TestCase* find(std::string_view id) const noexcept;
    [[nodiscard]] std::vector<const TestCase*> all() const;
    [[nodiscard]] std::vector<const TestCase*> byComponent(std::string_view component) const;
    [[nodiscard]] std::vector<TestResult> run(const std::vector<const TestCase*>& selected) const;

private:
    std::vector<TestCase> cases_;
};

void registerInfrastructureModule(TestRegistry& registry);
void registerBasicValuesModule(TestRegistry& registry);
void registerBasicSha256Module(TestRegistry& registry);
void registerBasicSha256FileModule(TestRegistry& registry);
void registerBasicOutputFailureModule(TestRegistry& registry);
void registerBasicArtifactStagingModule(TestRegistry& registry);
void registerBasicArtifactPreflightModule(TestRegistry& registry);
}
