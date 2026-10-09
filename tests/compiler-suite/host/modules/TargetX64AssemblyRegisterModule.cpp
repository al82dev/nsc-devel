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

constexpr std::array kLegacyRegisters{
    "rax", "eax", "ax", "al", "ah", "rbx", "ebx", "bx", "bl", "bh",
    "rcx", "ecx", "cx", "cl", "ch", "rdx", "edx", "dx", "dl", "dh",
    "rsi", "esi", "si", "sil", "rdi", "edi", "di", "dil",
    "rbp", "ebp", "bp", "bpl", "rsp", "esp", "sp", "spl",
};

constexpr std::array kExtendedSuffixes{"", "d", "w", "b"};

constexpr std::array kInvalidRegisters{
    "", "RAX", "%rax", "rax ", "r", "r0", "r7", "r7d", "r16", "r16d", "r8q", "r8D",
    "xmm", "xmm16", "XMM0", "ymm0", "mm", "mm8", "st", "st8", "rip",
    "xmm18446744073709551615", "r18446744073709551615",
};

// Registers one case under the fixed Host.Target contract without shared mutable state. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-028", "Host.Target", "unit",
        std::move(profile), {"target", "x64-assembly-register"}, std::move(function)});
}

// Builds one canonical numbered register spelling. The returned string owns its storage. Complexity is O(1).
std::string numbered(std::string_view prefix, unsigned ordinal, std::string_view suffix = {})
{
    return std::string(prefix) + std::to_string(ordinal) + std::string(suffix);
}

// Registers the minimum, representative, and configuration-independent valid cases. Complexity is O(R).
void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-217", "PR", [](TestContext& context) {
        context.expect(TargetInfo::WindowsX64().isAssemblyRegister("rax"),
            "TF-TGT-217 rax must be recognized by the x64 catalog");
    });
    addCase(registry, "TF-TGT-218", "PR", [](TestContext& context) {
        constexpr std::array representatives{
            "ah", "sil", "r8", "r15d", "r12w", "r9b", "xmm15", "mm7", "st7",
        };
        const TargetInfo target = TargetInfo::WindowsX64();
        for (const std::string_view name : representatives)
            context.expect(target.isAssemblyRegister(name),
                "TF-TGT-218 every representative x64 register must be recognized");
    });
    addCase(registry, "TF-TGT-219", "PR", [](TestContext& context) {
        const TargetInfo fresh = TargetInfo::WindowsX64();
        context.expect(fresh.isAssemblyRegister("rsp") && fresh.isAssemblyRegister("xmm0"),
            "TF-TGT-219 the default factory must expose the register catalog without metadata");
    });
}

// Registers malformed and exact family-boundary cases. Complexity is O(I), with a fixed invalid corpus.
void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-220", "PR", [](TestContext& context) {
        constexpr std::array malformed{
            "RAX", "%rax", "r8q", "r8D", "ymm0", "rip", "xmm18446744073709551615",
        };
        const TargetInfo target = TargetInfo::WindowsX64();
        for (const std::string_view name : malformed)
            context.expect(!target.isAssemblyRegister(name),
                "TF-TGT-220 malformed, uppercase, unsupported, or overflowing names must be rejected");
    });
    addCase(registry, "TF-TGT-221", "PR", [](TestContext& context) {
        const TargetInfo target = TargetInfo::WindowsX64();
        context.expect(!target.isAssemblyRegister("") && !target.isAssemblyRegister("r7") &&
            target.isAssemblyRegister("r8") && target.isAssemblyRegister("r15") &&
            !target.isAssemblyRegister("r16"), "TF-TGT-221 extended GPR boundaries must be exact");
        context.expect(target.isAssemblyRegister("xmm0") && target.isAssemblyRegister("xmm15") &&
            !target.isAssemblyRegister("xmm16"), "TF-TGT-221 XMM boundaries must be exact");
        context.expect(target.isAssemblyRegister("mm0") && target.isAssemblyRegister("mm7") &&
            !target.isAssemblyRegister("mm8"), "TF-TGT-221 MM boundaries must be exact");
        context.expect(target.isAssemblyRegister("st0") && target.isAssemblyRegister("st7") &&
            !target.isAssemblyRegister("st8"), "TF-TGT-221 x87 stack boundaries must be exact");
    });
}

// Verifies all 100 canonical names and the independent invalid corpus. Complexity is O(V + I).
void registerCorpusCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-222", "PR", [](TestContext& context) {
        const TargetInfo target = TargetInfo::WindowsX64();
        std::size_t validCount = 0;
        for (const std::string_view name : kLegacyRegisters)
        {
            context.expect(target.isAssemblyRegister(name),
                "TF-TGT-222 every legacy alias must be recognized");
            ++validCount;
        }
        for (unsigned ordinal = 8; ordinal <= 15; ++ordinal)
        {
            for (const std::string_view suffix : kExtendedSuffixes)
            {
                context.expect(target.isAssemblyRegister(numbered("r", ordinal, suffix)),
                    "TF-TGT-222 every r8-r15 width variant must be recognized");
                ++validCount;
            }
        }
        for (unsigned ordinal = 0; ordinal <= 15; ++ordinal)
        {
            context.expect(target.isAssemblyRegister(numbered("xmm", ordinal)),
                "TF-TGT-222 every xmm0-xmm15 register must be recognized");
            ++validCount;
        }
        for (unsigned ordinal = 0; ordinal <= 7; ++ordinal)
        {
            context.expect(target.isAssemblyRegister(numbered("mm", ordinal)) &&
                target.isAssemblyRegister(numbered("st", ordinal)),
                "TF-TGT-222 every mm0-mm7 and st0-st7 register must be recognized");
            validCount += 2;
        }
        context.expect(validCount == 100, "TF-TGT-222 the canonical valid corpus must contain 100 names");
        for (const std::string_view name : kInvalidRegisters)
            context.expect(!target.isAssemblyRegister(name),
                "TF-TGT-222 every enumerated invalid spelling must be rejected");
    });
}

// Verifies that order and repetition cannot affect the stateless query. Complexity is O(R).
void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-223", "PR", [](TestContext& context) {
        constexpr std::array corpus{
            "rax", "r8", "r15b", "xmm0", "xmm15", "mm7",
            "st0", "RAX", "r16", "xmm16", "ymm0", "",
        };
        std::array<bool, corpus.size()> forward{};
        std::array<bool, corpus.size()> reverse{};
        const TargetInfo target = TargetInfo::WindowsX64();
        for (std::size_t index = 0; index < corpus.size(); ++index)
            forward[index] = target.isAssemblyRegister(corpus[index]);
        for (std::size_t index = corpus.size(); index-- > 0;)
            reverse[index] = target.isAssemblyRegister(corpus[index]);
        for (std::size_t index = 0; index < corpus.size(); ++index)
            context.expect(forward[index] == reverse[index] &&
                forward[index] == target.isAssemblyRegister(corpus[index]),
                "TF-TGT-223 verdicts must be independent of order and repetition");
    });
}

// Exercises 65,536 iterative queries across five valid families and one invalid corpus. Complexity is O(N).
void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-224", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t iterationCount = 65536;
        const TargetInfo target = TargetInfo::WindowsX64();
        for (std::size_t index = 0; index < iterationCount; ++index)
        {
            const std::size_t family = index % 6;
            const std::size_t sample = index / 6;
            std::string name;
            bool expected = true;
            if (family == 0) name = kLegacyRegisters[sample % kLegacyRegisters.size()];
            else if (family == 1) name = numbered("r", 8U + static_cast<unsigned>(sample % 8),
                kExtendedSuffixes[sample % kExtendedSuffixes.size()]);
            else if (family == 2) name = numbered("xmm", static_cast<unsigned>(sample % 16));
            else if (family == 3) name = numbered("mm", static_cast<unsigned>(sample % 8));
            else if (family == 4) name = numbered("st", static_cast<unsigned>(sample % 8));
            else
            {
                name = kInvalidRegisters[sample % kInvalidRegisters.size()];
                expected = false;
            }
            context.expect(target.isAssemblyRegister(name) == expected,
                "TF-TGT-224 every scaled verdict must match the enumerated corpus");
        }
    });
}
}

// Registers the complete CF-TGT-028 case set. The registry owns each callable for the process lifetime.
void registerTargetX64AssemblyRegisterModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerCorpusCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
