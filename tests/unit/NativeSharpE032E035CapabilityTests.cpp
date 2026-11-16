#include "nativesharp/Runtime/RuntimeCapabilityReport.h"

#include "../support/TestContext.h"

#include <ranges>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::runtime;

    const std::vector<RuntimeCapability> capabilities = runtimeCapabilities();
    tests.expect(capabilities.size() == 4,
        "runtime report has one explicit entry for E032 through E035");
    tests.expect(capabilities[0].id == "E032" &&
            capabilities[0].status == RuntimeCapabilityStatus::Missing,
        "shared-memory regions stay missing without physical ABI hooks");
    tests.expect(capabilities[1].id == "E033" &&
            capabilities[1].status == RuntimeCapabilityStatus::Partial &&
            capabilities[1].evidence.size() == 2,
        "existing named synchronization primitives are evidence but not SharedMutex/wait-notify");
    tests.expect(capabilities[2].id == "E034" &&
            capabilities[2].status == RuntimeCapabilityStatus::Missing,
        "future continuations stay missing without WhenAll/WhenAny runtime contracts");
    tests.expect(capabilities[3].id == "E035" &&
            capabilities[3].status == RuntimeCapabilityStatus::Partial &&
            capabilities[3].evidence.size() == 1,
        "raw FILETIME is reported without claiming Windows time-zone support");

    const std::string report = runtimeCapabilityReport();
    tests.expect(report.find("runtime-abi-fingerprint:") != std::string::npos &&
            report.find("E033 shared-synchronization status=partial") != std::string::npos &&
            report.find("missing: atomic wait/notify hooks") != std::string::npos,
        "capability report is ABI-bound, stable, and explains missing contracts");
    std::string detail;
    tests.expect(!runtimeCapabilityReady("E033", detail) &&
            detail.find("partial") != std::string::npos,
        "capability gate rejects a partial subsystem");
    tests.expect(!runtimeCapabilityReady("E999", detail) &&
            detail.find("unknown runtime capability") != std::string::npos,
        "capability gate rejects unknown identifiers causally");
    return tests.result();
}
