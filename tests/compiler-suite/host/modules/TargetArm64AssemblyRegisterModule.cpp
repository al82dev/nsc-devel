#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::target::TargetInfo;

constexpr std::array kAliases{"sp", "fp", "lr", "xzr", "wzr"};

constexpr std::array kInvalidRegisters{
    "", "SP", "X0", "x", "w", "v", "x31", "w31", "v32", "x-1", "w+1", "v1x",
    "x0d", "x0 ", "rax", "q0", "x18446744073709551615", "v18446744073709551615",
};

// Registers one case under the fixed Host.Target contract without shared mutable state. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-029", "Host.Target", "unit",
        std::move(profile), {"target", "arm64-assembly-register"}, std::move(function)});
}

// Builds one canonical numbered register spelling. The returned string owns its storage. Complexity is O(1).
std::string numbered(std::string_view prefix, unsigned ordinal)
{
    return std::string(prefix) + std::to_string(ordinal);
}

// Registers the minimum, representative, and configuration-independent valid cases. Complexity is O(R).
void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-225", "PR", [](TestContext& context) {
        context.expect(TargetInfo::WindowsArm64().isAssemblyRegister("sp"),
            "TF-TGT-225 sp must be recognized by the ARM64 catalog");
    });
    addCase(registry, "TF-TGT-226", "PR", [](TestContext& context) {
        constexpr std::array representatives{
            "sp", "fp", "lr", "xzr", "wzr", "x0", "x30", "w0", "w30", "v0", "v31",
        };
        const TargetInfo target = TargetInfo::WindowsArm64();
        for (const std::string_view name : representatives)
            context.expect(target.isAssemblyRegister(name),
                "TF-TGT-226 every representative ARM64 register must be recognized");
    });
    addCase(registry, "TF-TGT-227", "PR", [](TestContext& context) {
        const TargetInfo fresh = TargetInfo::WindowsArm64();
        context.expect(fresh.isAssemblyRegister("x0") && fresh.isAssemblyRegister("v31"),
            "TF-TGT-227 the factory must expose the register catalog without metadata");
    });
}

// Registers malformed and exact family-boundary cases. Complexity is O(I), with a fixed invalid corpus.
void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-228", "PR", [](TestContext& context) {
        constexpr std::array malformed{
            "SP", "X0", "x", "w", "v", "x-1", "w+1", "v1x", "rax", "q0",
            "x18446744073709551615",
        };
        const TargetInfo target = TargetInfo::WindowsArm64();
        for (const std::string_view name : malformed)
            context.expect(!target.isAssemblyRegister(name),
                "TF-TGT-228 missing digits, trailing text, uppercase, or unsupported names must be rejected");
    });
    addCase(registry, "TF-TGT-229", "PR", [](TestContext& context) {
        const TargetInfo target = TargetInfo::WindowsArm64();
        context.expect(!target.isAssemblyRegister("") && target.isAssemblyRegister("x0") &&
            target.isAssemblyRegister("x30") && !target.isAssemblyRegister("x31"),
            "TF-TGT-229 x-register boundaries must be exact");
        context.expect(target.isAssemblyRegister("w0") && target.isAssemblyRegister("w30") &&
            !target.isAssemblyRegister("w31"), "TF-TGT-229 w-register boundaries must be exact");
        context.expect(target.isAssemblyRegister("v0") && target.isAssemblyRegister("v31") &&
            !target.isAssemblyRegister("v32"), "TF-TGT-229 vector-register boundaries must be exact");
    });
}

// Verifies all 99 canonical names and the independent invalid corpus. Complexity is O(V + I).
void registerCorpusCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-230", "PR", [](TestContext& context) {
        const TargetInfo target = TargetInfo::WindowsArm64();
        std::size_t validCount = 0;
        for (const std::string_view name : kAliases)
        {
            context.expect(target.isAssemblyRegister(name),
                "TF-TGT-230 every ARM64 alias must be recognized");
            ++validCount;
        }
        for (unsigned ordinal = 0; ordinal <= 30; ++ordinal)
        {
            context.expect(target.isAssemblyRegister(numbered("x", ordinal)) &&
                target.isAssemblyRegister(numbered("w", ordinal)),
                "TF-TGT-230 every x0-x30 and w0-w30 register must be recognized");
            validCount += 2;
        }
        for (unsigned ordinal = 0; ordinal <= 31; ++ordinal)
        {
            context.expect(target.isAssemblyRegister(numbered("v", ordinal)),
                "TF-TGT-230 every v0-v31 register must be recognized");
            ++validCount;
        }
        context.expect(validCount == 99, "TF-TGT-230 the canonical valid corpus must contain 99 names");
        for (const std::string_view name : kInvalidRegisters)
            context.expect(!target.isAssemblyRegister(name),
                "TF-TGT-230 every enumerated invalid spelling must be rejected");
    });
}

// Verifies that order and repetition cannot affect the stateless query. Complexity is O(R).
void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-231", "PR", [](TestContext& context) {
        constexpr std::array corpus{
            "sp", "fp", "x0", "x30", "w0", "w30", "v0", "v31", "SP", "x31", "v32", "",
        };
        std::array<bool, corpus.size()> forward{};
        std::array<bool, corpus.size()> reverse{};
        const TargetInfo target = TargetInfo::WindowsArm64();
        for (std::size_t index = 0; index < corpus.size(); ++index)
            forward[index] = target.isAssemblyRegister(corpus[index]);
        for (std::size_t index = corpus.size(); index-- > 0;)
            reverse[index] = target.isAssemblyRegister(corpus[index]);
        for (std::size_t index = 0; index < corpus.size(); ++index)
            context.expect(forward[index] == reverse[index] &&
                forward[index] == target.isAssemblyRegister(corpus[index]),
                "TF-TGT-231 verdicts must be independent of order and repetition");
    });
}

// Exercises 65,536 iterative queries across aliases, three numbered families, and invalids. Complexity is O(N).
void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-232", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t iterationCount = 65536;
        const TargetInfo target = TargetInfo::WindowsArm64();
        for (std::size_t index = 0; index < iterationCount; ++index)
        {
            const std::size_t family = index % 5;
            const std::size_t sample = index / 5;
            std::string name;
            bool expected = true;
            if (family == 0) name = kAliases[sample % kAliases.size()];
            else if (family == 1) name = numbered("x", static_cast<unsigned>(sample % 31));
            else if (family == 2) name = numbered("w", static_cast<unsigned>(sample % 31));
            else if (family == 3) name = numbered("v", static_cast<unsigned>(sample % 32));
            else
            {
                name = kInvalidRegisters[sample % kInvalidRegisters.size()];
                expected = false;
            }
            context.expect(target.isAssemblyRegister(name) == expected,
                "TF-TGT-232 every scaled verdict must match the enumerated corpus");
        }
    });
}
}

// Registers the complete CF-TGT-029 case set. The registry owns each callable for the process lifetime.
void registerTargetArm64AssemblyRegisterModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerCorpusCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
