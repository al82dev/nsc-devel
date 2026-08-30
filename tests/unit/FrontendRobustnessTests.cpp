#include "nativesharp/Frontend/FrontendCompilation.h"

#include "../support/TestContext.h"

#include <cstdint>
#include <random>
#include <string>

int main()
{
    using nativesharp::frontend::FrontendCompilation;
    using nativesharp::tests::TestContext;

    TestContext tests;
    std::mt19937 generator(0x4E534330U);
    const std::string alphabet =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+-*/%&|^~!=<>(){}[];,. \\n\\r\\t'\\\"";
    std::uniform_int_distribution<std::size_t> lengthDistribution(0, 256);
    std::uniform_int_distribution<std::size_t> characterDistribution(0, alphabet.size() - 1);

    constexpr std::size_t caseCount = 500;
    for (std::size_t caseIndex = 0; caseIndex < caseCount; ++caseIndex)
    {
        const std::size_t length = lengthDistribution(generator);
        std::string source;
        source.reserve(length);
        for (std::size_t index = 0; index < length; ++index)
        {
            source.push_back(alphabet[characterDistribution(generator)]);
        }

        FrontendCompilation compilation;
        (void)compilation.compileBuffer("robustness.ns", std::move(source));
    }

    tests.expect(true, "500 deterministic arbitrary frontend inputs complete without a crash");
    return tests.result();
}
