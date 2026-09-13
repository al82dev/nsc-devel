#include "nativesharp/Frontend/InliningPolicy.h"

#include "../support/TestContext.h"

#include <cstdint>

int main()
{
    using nativesharp::frontend::InliningPolicy;
    using nativesharp::frontend::inliningPolicyName;
    using nativesharp::frontend::isValidInliningPolicy;
    using nativesharp::frontend::permitsInlining;
    using nativesharp::frontend::requestsInlining;
    using nativesharp::frontend::requiresInlining;
    using nativesharp::frontend::requiresOutOfLineCall;
    using nativesharp::tests::TestContext;

    TestContext tests;

    tests.expect(inliningPolicyName(InliningPolicy::Automatic) == "automatic",
        "automatic policy has a stable diagnostic and metadata name");
    tests.expect(inliningPolicyName(InliningPolicy::Preferred) == "inline",
        "preferred policy maps to the future inline source modifier");
    tests.expect(inliningPolicyName(InliningPolicy::Required) == "forceinline",
        "required policy maps to the future forceinline source modifier");
    tests.expect(inliningPolicyName(InliningPolicy::Forbidden) == "noinline",
        "forbidden policy maps to the future noinline source modifier");

    tests.expect(permitsInlining(InliningPolicy::Automatic),
        "automatic policy permits a cost-based optimizer decision");
    tests.expect(permitsInlining(InliningPolicy::Preferred),
        "inline permits a legal optimizer decision");
    tests.expect(permitsInlining(InliningPolicy::Required),
        "forceinline permits and requires a legal inline expansion");
    tests.expect(!permitsInlining(InliningPolicy::Forbidden),
        "noinline forbids every inline expansion");

    tests.expect(!requestsInlining(InliningPolicy::Automatic),
        "automatic policy does not express a source request");
    tests.expect(requestsInlining(InliningPolicy::Preferred),
        "inline expresses a preferred request");
    tests.expect(requestsInlining(InliningPolicy::Required),
        "forceinline expresses a mandatory request");
    tests.expect(!requestsInlining(InliningPolicy::Forbidden),
        "noinline does not request an expansion");

    tests.expect(requiresInlining(InliningPolicy::Required),
        "only forceinline requires inline expansion");
    tests.expect(requiresOutOfLineCall(InliningPolicy::Forbidden),
        "noinline requires an out-of-line call site");

    const auto invalid = static_cast<InliningPolicy>(static_cast<std::uint8_t>(255));
    tests.expect(!isValidInliningPolicy(invalid) && inliningPolicyName(invalid) == "invalid",
        "unknown serialized policies fail closed instead of becoming automatic");

    return tests.result();
}
