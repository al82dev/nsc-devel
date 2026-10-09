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
void registerBasicArtifactControlledPreflightModule(TestRegistry& registry);
void registerBasicTransactionalWriterModule(TestRegistry& registry);
void registerBasicArtifactSeedModule(TestRegistry& registry);
void registerSourcePositionModule(TestRegistry& registry);
void registerSourceFileOwnershipModule(TestRegistry& registry);
void registerSourceLineIndexModule(TestRegistry& registry);
void registerSourceOffsetPositionModule(TestRegistry& registry);
void registerSourceLineTextModule(TestRegistry& registry);
void registerSourceFileReadModule(TestRegistry& registry);
void registerSourceManagerRegistryModule(TestRegistry& registry);
void registerSourceManagerQueryModule(TestRegistry& registry);
void registerTargetDomainEnumsModule(TestRegistry& registry);
void registerTargetLayoutValuesModule(TestRegistry& registry);
void registerTargetAggregateRequestModule(TestRegistry& registry);
void registerTargetWindowsFactoriesModule(TestRegistry& registry);
void registerTargetBuiltinLayoutModule(TestRegistry& registry);
void registerTargetCheckedArithmeticModule(TestRegistry& registry);
void registerTargetPowerOfTwoAlignmentModule(TestRegistry& registry);
void registerTargetElementStrideModule(TestRegistry& registry);
void registerTargetArrayLayoutModule(TestRegistry& registry);
void registerTargetPlainStructLayoutModule(TestRegistry& registry);
void registerTargetAggregateOptionsModule(TestRegistry& registry);
void registerTargetPackedFieldPlacementModule(TestRegistry& registry);
void registerTargetMinimumAlignmentModule(TestRegistry& registry);
void registerTargetExplicitOffsetModule(TestRegistry& registry);
void registerTargetUnionLayoutModule(TestRegistry& registry);
void registerTargetFlexibleArrayLayoutModule(TestRegistry& registry);
void registerTargetBitFieldValidationModule(TestRegistry& registry);
void registerTargetSequentialBitFieldModule(TestRegistry& registry);
}
