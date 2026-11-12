#include "../../src/Driver/PublicSurfaceEmitterSupport.h"
#include "../support/TestContext.h"

#include <limits>
#include <string>

int main()
{
    namespace support = nativesharp::driver::public_surface_support;
    nativesharp::tests::TestContext tests;
    const nativesharp::interop::LibraryMetadata metadata{};
    const auto expectValid = [&](const std::string& key, const std::string& expected) {
        std::string output;
        std::string error;
        bool ok = support::sourceTypeForMetadataKey(key, output, error);
        tests.expect(ok && output == expected, "direct mapping failed for " + key + ": " + error);
        output.clear();
        error.clear();
        ok = support::sourceTypeForMetadataKey(metadata, key, output, error);
        tests.expect(ok && output == expected, "metadata mapping failed for " + key + ": " + error);
    };
    const auto expectInvalid = [&](const std::string& key) {
        std::string output;
        std::string error;
        bool ok = support::sourceTypeForMetadataKey(key, output, error);
        tests.expect(!ok && !error.empty(), "direct mapping accepted " + key);
        output.clear();
        error.clear();
        ok = support::sourceTypeForMetadataKey(metadata, key, output, error);
        tests.expect(!ok && !error.empty(), "metadata mapping accepted " + key);
    };

    expectValid("mask<builtin:int,4>", "mask<int, 4>");
    expectValid("simd<builtin:float,8>", "simd<float, 8>");
    expectValid("simd<builtin:long,8>", "simd<long, 8>");
    expectValid("mask<builtin:int,4>*", "mask<int, 4>*");
    expectValid("iterator:simd<builtin:int,4>", "Iterator<simd<int, 4>>");
    expectInvalid("mask<builtin:int,1>");
    expectInvalid("simd<builtin:int,3>");
    expectInvalid("simd<builtin:long,16>");
    expectInvalid("mask<builtin:bool,4>");
    expectInvalid("mask<builtin:int,04>");
    expectInvalid("simd<builtin:int,184467440737095516160>");
    // A power of two that wraps laneBits*count to zero in size_t.
    const std::size_t overflowingCount = std::numeric_limits<std::size_t>::max() / 2U + 1U;
    expectInvalid("simd<builtin:int," + std::to_string(overflowingCount) + ">");
    return tests.result();
}
