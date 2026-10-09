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

using Mapping = std::pair<std::string_view, std::string_view>;

constexpr std::array kMappings{
    Mapping{"vaddps", "avx"}, Mapping{"v", "avx"},
    Mapping{"aesenc", "aes"}, Mapping{"aes", "aes"},
    Mapping{"pclmulqdq", "pclmul"}, Mapping{"pclmul", "pclmul"},
    Mapping{"popcnt", "popcnt"}, Mapping{"lzcnt", "lzcnt"},
    Mapping{"crc32", "sse4.2"}, Mapping{"pcmpestri", "sse4.2"},
    Mapping{"pcmpestr", "sse4.2"}, Mapping{"pcmpistri", "sse4.2"},
    Mapping{"pcmpistr", "sse4.2"}, Mapping{"andn", "bmi"},
    Mapping{"bextr", "bmi"}, Mapping{"blsi", "bmi"},
    Mapping{"blsmsk", "bmi"}, Mapping{"blsr", "bmi"},
    Mapping{"bzhi", "bmi"}, Mapping{"mulx", "bmi"},
    Mapping{"pdep", "bmi"}, Mapping{"pext", "bmi"},
    Mapping{"rorx", "bmi"}, Mapping{"sarx", "bmi"},
    Mapping{"shlx", "bmi"}, Mapping{"shrx", "bmi"},
};

constexpr std::array kUnknownMnemonics{
    "", "add", "VADDPS", "POPCNT", " popcnt", "popcnt ", "popcntx", "crc32x",
    "pcmpest", "unknown",
};

// Registers one case under the fixed Host.Target contract without shared mutable state. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-030", "Host.Target", "unit",
        std::move(profile), {"target", "assembly-feature-requirement"}, std::move(function)});
}

// Registers minimum, representative, and configuration-independent mappings. Complexity is O(R).
void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-233", "PR", [](TestContext& context) {
        context.expect(TargetInfo::WindowsX64().requiredAssemblyFeature("popcnt") == "popcnt",
            "TF-TGT-233 popcnt must require the popcnt feature");
    });
    addCase(registry, "TF-TGT-234", "PR", [](TestContext& context) {
        constexpr std::array representatives{
            Mapping{"vaddps", "avx"}, Mapping{"aesenc", "aes"},
            Mapping{"pclmulqdq", "pclmul"}, Mapping{"lzcnt", "lzcnt"},
            Mapping{"crc32", "sse4.2"}, Mapping{"pcmpestri", "sse4.2"},
            Mapping{"andn", "bmi"},
        };
        const TargetInfo target = TargetInfo::WindowsX64();
        for (const Mapping& mapping : representatives)
            context.expect(target.requiredAssemblyFeature(mapping.first) == mapping.second,
                "TF-TGT-234 every representative mnemonic must map to its exact feature");
    });
    addCase(registry, "TF-TGT-235", "PR", [](TestContext& context) {
        const TargetInfo fresh = TargetInfo::WindowsX64();
        context.expect(fresh.requiredAssemblyFeature("vaddps") == "avx",
            "TF-TGT-235 the default factory must classify mnemonics without metadata");
    });
}

// Registers invalid spellings and exact prefix or exact-name boundaries. Complexity is O(I).
void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-236", "PR", [](TestContext& context) {
        constexpr std::array invalid{
            "VADDPS", "POPCNT", " popcnt", "popcnt ", "popcntx", "crc32x", "unknown",
        };
        const TargetInfo target = TargetInfo::WindowsX64();
        for (const std::string_view mnemonic : invalid)
            context.expect(target.requiredAssemblyFeature(mnemonic).empty(),
                "TF-TGT-236 noncanonical or unknown mnemonics must return an empty feature");
    });
    addCase(registry, "TF-TGT-237", "PR", [](TestContext& context) {
        const TargetInfo target = TargetInfo::WindowsX64();
        context.expect(target.requiredAssemblyFeature("").empty() &&
            target.requiredAssemblyFeature("v") == "avx" &&
            target.requiredAssemblyFeature("aes") == "aes" &&
            target.requiredAssemblyFeature("pclmul") == "pclmul",
            "TF-TGT-237 prefix boundaries must be exact");
        context.expect(target.requiredAssemblyFeature("popcntx").empty() &&
            target.requiredAssemblyFeature("crc32x").empty() &&
            target.requiredAssemblyFeature("pcmpestr") == "sse4.2" &&
            target.requiredAssemblyFeature("pcmpest").empty(),
            "TF-TGT-237 exact and pcmp prefix boundaries must be exact");
    });
}

// Verifies the complete x64 table, unknowns, and the ARM64 empty-result rule. Complexity is O(M + U).
void registerMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-238", "PR", [](TestContext& context) {
        const TargetInfo x64 = TargetInfo::WindowsX64();
        const TargetInfo arm64 = TargetInfo::WindowsArm64();
        for (const Mapping& mapping : kMappings)
        {
            context.expect(x64.requiredAssemblyFeature(mapping.first) == mapping.second,
                "TF-TGT-238 every x64 mnemonic must map to the independent table");
            context.expect(arm64.requiredAssemblyFeature(mapping.first).empty(),
                "TF-TGT-238 every ARM64 mnemonic must return an empty feature");
        }
        for (const std::string_view mnemonic : kUnknownMnemonics)
            context.expect(x64.requiredAssemblyFeature(mnemonic).empty(),
                "TF-TGT-238 every unknown x64 mnemonic must return an empty feature");
    });
}

// Verifies that order and repetition cannot affect the stateless query. Complexity is O(R).
void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-239", "PR", [](TestContext& context) {
        constexpr std::array corpus{
            "vaddps", "aesenc", "pclmulqdq", "popcnt", "lzcnt", "crc32",
            "pcmpestri", "andn", "shrx", "VADDPS", "unknown", "",
        };
        std::array<std::string_view, corpus.size()> forward{};
        std::array<std::string_view, corpus.size()> reverse{};
        const TargetInfo target = TargetInfo::WindowsX64();
        for (std::size_t index = 0; index < corpus.size(); ++index)
            forward[index] = target.requiredAssemblyFeature(corpus[index]);
        for (std::size_t index = corpus.size(); index-- > 0;)
            reverse[index] = target.requiredAssemblyFeature(corpus[index]);
        for (std::size_t index = 0; index < corpus.size(); ++index)
            context.expect(forward[index] == reverse[index] &&
                forward[index] == target.requiredAssemblyFeature(corpus[index]),
                "TF-TGT-239 feature results must be independent of order and repetition");
    });
}

// Exercises 65,536 iterative target-and-mnemonic queries against independent tables. Complexity is O(N).
void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-240", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t iterationCount = 65536;
        const TargetInfo x64 = TargetInfo::WindowsX64();
        const TargetInfo arm64 = TargetInfo::WindowsArm64();
        for (std::size_t index = 0; index < iterationCount; ++index)
        {
            if ((index % 3) == 0)
            {
                const Mapping& mapping = kMappings[(index / 3) % kMappings.size()];
                context.expect(x64.requiredAssemblyFeature(mapping.first) == mapping.second,
                    "TF-TGT-240 every scaled x64 mapping must match the independent table");
            }
            else if ((index % 3) == 1)
            {
                const std::string_view mnemonic = kUnknownMnemonics[(index / 3) % kUnknownMnemonics.size()];
                context.expect(x64.requiredAssemblyFeature(mnemonic).empty(),
                    "TF-TGT-240 every scaled unknown x64 mnemonic must remain empty");
            }
            else
            {
                const Mapping& mapping = kMappings[(index / 3) % kMappings.size()];
                context.expect(arm64.requiredAssemblyFeature(mapping.first).empty(),
                    "TF-TGT-240 every scaled ARM64 mnemonic must remain empty");
            }
        }
    });
}
}

// Registers the complete CF-TGT-030 case set. The registry owns each callable for the process lifetime.
void registerTargetAssemblyFeatureRequirementModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
