#include "RuntimeTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using namespace runtime_support;

void expectStatusMatrix(TestContext& context);
void expectCapabilityStates(TestContext& context,
    const std::vector<runtime::RuntimeCapability>& capabilities);
void expectCapabilityEvidence(TestContext& context,
    const std::vector<runtime::RuntimeCapability>& capabilities);
void expectReadinessMatrix(TestContext& context);

const runtime::RuntimeCapability* findCapability(
    const std::vector<runtime::RuntimeCapability>& capabilities, std::string_view id)
{
    const auto found = std::ranges::find_if(capabilities,
        [id](const runtime::RuntimeCapability& capability) { return capability.id == id; });
    return found == capabilities.end() ? nullptr : &*found;
}

void capabilityStatusContract(TestContext& context, std::size_t variant)
{
    using runtime::RuntimeCapabilityStatus;
    if (variant == 1)
        context.expect(std::string_view(runtime::runtimeCapabilityStatusName(RuntimeCapabilityStatus::Ready)) ==
            "ready", "Ready status must retain its stable spelling");
    else if (variant == 2)
        context.expect(std::string_view(runtime::runtimeCapabilityStatusName(RuntimeCapabilityStatus::Partial)) ==
            "partial", "Partial status must retain its stable spelling");
    else if (variant == 3)
        context.expect(std::string_view(runtime::runtimeCapabilityStatusName(RuntimeCapabilityStatus::Missing)) ==
            "missing", "Missing status must retain its stable spelling");
    else if (variant == 4)
        context.expect(std::string_view(runtime::runtimeCapabilityStatusName(
            static_cast<RuntimeCapabilityStatus>(255))) == "unknown",
            "invalid status value must not be mislabeled as a supported state");
    else if (variant == 5)
        context.expect(runtime::runtimeCapabilityStatusName(RuntimeCapabilityStatus::Ready) != nullptr &&
            runtime::runtimeCapabilityStatusName(RuntimeCapabilityStatus::Missing) != nullptr,
            "status projection must produce a value for every boundary state");
    else if (variant == 6)
        expectStatusMatrix(context);
    else if (variant == 7)
        context.expect(std::string_view(runtime::runtimeCapabilityStatusName(RuntimeCapabilityStatus::Partial)) ==
            runtime::runtimeCapabilityStatusName(RuntimeCapabilityStatus::Partial),
            "status projection must be deterministic");
    else
        for (std::size_t repetition = 0; repetition < 4096; ++repetition)
            context.expect(std::string_view(runtime::runtimeCapabilityStatusName(
                static_cast<RuntimeCapabilityStatus>(repetition % 3))).size() >= 5,
                "repeated status projections must remain bounded and nonempty");
}

void expectStatusMatrix(TestContext& context)
{
    using runtime::RuntimeCapabilityStatus;
    constexpr std::array expected = {
        std::pair{RuntimeCapabilityStatus::Ready, std::string_view("ready")},
        std::pair{RuntimeCapabilityStatus::Partial, std::string_view("partial")},
        std::pair{RuntimeCapabilityStatus::Missing, std::string_view("missing")},
    };
    for (const auto& [status, name] : expected)
        context.expect(std::string_view(runtime::runtimeCapabilityStatusName(status)) == name,
            "status matrix must preserve every stable name");
}

void capabilityInventoryContract(TestContext& context, std::size_t variant)
{
    const std::vector<runtime::RuntimeCapability> capabilities = runtime::runtimeCapabilities();
    if (variant == 1)
        context.expect(capabilities.size() == 4 && capabilities.front().id == "E032",
            "runtime capability inventory must begin with E032 and contain four entries");
    else if (variant == 2)
        context.expect(capabilities.back().id == "E035" && capabilities.back().evidence.size() == 1,
            "representative E035 entry must preserve its FILETIME evidence");
    else if (variant == 3)
        context.expect(findCapability(capabilities, "E036") == nullptr,
            "an absent capability must not be synthesized into the inventory");
    else if (variant == 4)
        expectCapabilityStates(context, capabilities);
    else if (variant == 5)
        context.expect(capabilities[0].missing.size() == 4 && capabilities[2].missing.size() == 4,
            "missing capabilities must retain their complete bounded gap catalogues");
    else if (variant == 6)
        expectCapabilityEvidence(context, capabilities);
    else if (variant == 7)
        context.expect(runtime::runtimeCapabilities()[1].evidence == capabilities[1].evidence,
            "capability evidence ordering must be deterministic");
    else
        for (std::size_t repetition = 0; repetition < 512; ++repetition)
            context.expect(runtime::runtimeCapabilities().size() == 4,
                "repeated capability materialization must preserve cardinality");
}

void expectCapabilityStates(TestContext& context,
    const std::vector<runtime::RuntimeCapability>& capabilities)
{
    using runtime::RuntimeCapabilityStatus;
    context.expect(capabilities[0].status == RuntimeCapabilityStatus::Missing,
        "E032 must remain Missing without shared-memory hooks");
    context.expect(capabilities[1].status == RuntimeCapabilityStatus::Partial,
        "E033 must remain Partial with synchronization evidence and known gaps");
    context.expect(capabilities[2].status == RuntimeCapabilityStatus::Missing,
        "E034 must remain Missing without future-continuation hooks");
    context.expect(capabilities[3].status == RuntimeCapabilityStatus::Partial,
        "E035 must remain Partial with FILETIME but no time-zone contract");
}

void expectCapabilityEvidence(TestContext& context,
    const std::vector<runtime::RuntimeCapability>& capabilities)
{
    context.expect(capabilities[1].evidence.size() == 2 && !capabilities[1].missing.empty(),
        "E033 must not promote partial synchronization evidence to Ready");
    context.expect(capabilities[3].evidence.size() == 1 && capabilities[3].missing.size() == 4,
        "E035 must preserve one evidence item and four missing contracts");
}

void capabilityReportContract(TestContext& context, std::size_t variant)
{
    const std::string report = runtime::runtimeCapabilityReport();
    std::string detail;
    if (variant == 1)
        context.expect(report.starts_with("NativeSharp runtime capability report v1\n"),
            "capability report must retain its schema version header");
    else if (variant == 2)
        context.expect(report.find("runtime-abi-fingerprint: 2cddeff864c7dfc6\n") != std::string::npos,
            "capability report must bind itself to the frozen runtime ABI");
    else if (variant == 3)
        context.expect(!runtime::runtimeCapabilityReady("E999", detail) &&
            detail.find("unknown runtime capability") != std::string::npos,
            "unknown capability must be rejected causally");
    else if (variant == 4)
        context.expect(!runtime::runtimeCapabilityReady("", detail) && !detail.empty(),
            "empty capability identity must be rejected with detail");
    else if (variant == 5)
        context.expect(std::ranges::count(report, '\n') == 25,
            "report must retain its bounded line-oriented shape");
    else if (variant == 6)
        expectReadinessMatrix(context);
    else if (variant == 7)
        context.expect(runtime::runtimeCapabilityReport() == report,
            "capability report must be deterministic");
    else
        for (std::size_t repetition = 0; repetition < 512; ++repetition)
            context.expect(runtime::runtimeCapabilityReport() == report,
                "repeated capability reports must remain byte-identical");
}

void expectReadinessMatrix(TestContext& context)
{
    for (const std::string_view id : {"E032", "E033", "E034", "E035"})
    {
        std::string detail;
        context.expect(!runtime::runtimeCapabilityReady(id, detail) && detail.find(id) != std::string::npos,
            "non-ready capability must remain false and identify itself");
    }
}
}

void registerRuntimeCapabilityModule(TestRegistry& registry)
{
    addRuntimeContract(registry, 8, "unit", capabilityStatusContract, {}, {"capability-status"});
    addRuntimeContract(registry, 9, "unit", capabilityInventoryContract, {}, {"capability-inventory"});
    addRuntimeContract(registry, 10, "unit", capabilityReportContract, {}, {"capability-report"});
}
}
