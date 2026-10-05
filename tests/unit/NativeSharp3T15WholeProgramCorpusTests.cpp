#include "nativesharp/Language/IntegrationQuality.h"

#include "../support/TestContext.h"

#include <algorithm>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    const auto corpus = buildNativeSharp3WholeProgramCorpus();
    tests.expect(corpus.size() == 6 && validateNativeSharp3WholeProgramCorpus(corpus, error),
        "the canonical corpus contains six complete SDK-independent witnesses");
    tests.expect(std::ranges::all_of(corpus, [](const WholeProgramWitness& witness)
        { return witness.deterministic && witness.supportsIncremental && !witness.requiresSdk; }),
        "every witness is deterministic, incremental, and independent of a broad SDK");
    tests.expect(std::ranges::all_of(corpus, [](const WholeProgramWitness& witness)
        { return witness.debugAndRelease && witness.cleanAndIncremental &&
            witness.ltoAndNoLto && witness.sanitizerReady; }),
        "every program owns Debug/Release, clean/incremental, LTO/no-LTO, and sanitizer profiles");
    tests.expect(std::ranges::count_if(corpus, [](const WholeProgramWitness& witness)
        { return witness.partialNoHeap; }) == 4,
        "parser, arena, job graph, and serializer define partial noheap profiles");
    tests.expect(std::ranges::all_of(corpus, [](const WholeProgramWitness& witness)
        { return witness.minimumSourceUnits >= 3 && witness.expectedExitCode == 42; }),
        "each witness is a multi-unit program with an observable expected result");

    auto sdkDependent = corpus;
    sdkDependent[0].requiresSdk = true;
    tests.expect(!validateNativeSharp3WholeProgramCorpus(sdkDependent, error) &&
            error.find("SDK-independent") != std::string::npos,
        "a hidden SDK dependency invalidates the whole-program corpus");
    auto incompleteProfiles = corpus;
    incompleteProfiles[1].ltoAndNoLto = false;
    tests.expect(!validateNativeSharp3WholeProgramCorpus(incompleteProfiles, error) &&
            error.find("build profile") != std::string::npos,
        "missing a required build profile fails with a causal diagnostic");
    auto duplicateKind = corpus;
    duplicateKind[5].kind = WholeProgramWitness::Kind::Parser;
    tests.expect(!validateNativeSharp3WholeProgramCorpus(duplicateKind, error) &&
            error.find("duplicate witness kind") != std::string::npos,
        "six names cannot mask a missing program category");
    return tests.result();
}
