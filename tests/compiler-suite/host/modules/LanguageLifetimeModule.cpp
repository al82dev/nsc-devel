#include "nsc_suite/TestRegistry.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

// TL-LANG-084 must seed the otherwise unreachable maximum counter without changing product API.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "nativesharp/Language/CrossModuleLifetime.h"
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <array>
#include <atomic>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

constexpr std::string_view kRequiredIdentityError =
    "cross-module lifetime contract requires producer, type, and allocator identities";
constexpr std::string_view kRequiredThunkError =
    "cross-module lifetime contract requires create, destroy, and deallocate thunks";

void addCase(
    TestRegistry& registry,
    std::string id,
    TestFunction function,
    std::string profile = "PR")
{
    registry.add(TestCase{std::move(id), "CL-LANG-006", "Host.Language", "unit",
        std::move(profile), {"language", "lifetime"}, std::move(function)});
}

CrossModuleLifetimeContract exampleContract(
    std::string_view producer = "Producer",
    ModuleUnloadPolicy policy = ModuleUnloadPolicy::RejectWhileObjectsLive)
{
    return makeCrossModuleLifetimeContract(
        producer, "Producer::Payload", "producer.heap.v1", policy);
}

void registerContractConstructionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-078", [](TestContext& context) {
        constexpr std::string_view suffix = "3e0a1ce29ea72ce1";
        for (const ModuleUnloadPolicy policy : std::array{
                 ModuleUnloadPolicy::RejectWhileObjectsLive,
                 ModuleUnloadPolicy::PinUntilObjectsDestroyed})
        {
            const CrossModuleLifetimeContract contract = exampleContract("Producer", policy);
            context.expect(contract.abiMajor == 1 && contract.abiMinor == 0 &&
                    contract.producerModule == "Producer" && contract.typeIdentity == "Producer::Payload" &&
                    contract.allocatorIdentity == "producer.heap.v1" && contract.unloadPolicy == policy,
                "TL-LANG-078 construction must preserve ABI, identities, and unload policy");
            context.expect(contract.createThunk == "__nsc_xmod_v1_create_" + std::string(suffix) &&
                    contract.destroyThunk == "__nsc_xmod_v1_destroy_" + std::string(suffix) &&
                    contract.deallocateThunk == "__nsc_xmod_v1_deallocate_" + std::string(suffix),
                "TL-LANG-078 construction must derive all three exact versioned FNV-1a thunks");
        }
    });
}

void registerContractValidationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-079", [](TestContext& context) {
        std::string error = "stale";
        const CrossModuleLifetimeContract valid = exampleContract();
        context.expect(validateCrossModuleLifetimeContract(valid, error) && error.empty(),
            "TL-LANG-079 a complete contract must validate and clear stale diagnostics");

        for (std::size_t field = 0; field < 3; ++field)
        {
            CrossModuleLifetimeContract invalid = valid;
            if (field == 0) invalid.producerModule.clear();
            if (field == 1) invalid.typeIdentity.clear();
            if (field == 2) invalid.allocatorIdentity.clear();
            context.expect(!validateCrossModuleLifetimeContract(invalid, error) && error == kRequiredIdentityError,
                "TL-LANG-079 every missing identity must produce the exact shared diagnostic");
        }
        for (std::size_t field = 0; field < 3; ++field)
        {
            CrossModuleLifetimeContract invalid = valid;
            if (field == 0) invalid.createThunk.clear();
            if (field == 1) invalid.destroyThunk.clear();
            if (field == 2) invalid.deallocateThunk.clear();
            context.expect(!validateCrossModuleLifetimeContract(invalid, error) && error == kRequiredThunkError,
                "TL-LANG-079 every missing thunk must produce the exact shared diagnostic");
        }
    });
}

void registerAbiCompatibilityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-080", [](TestContext& context) {
        std::string error;
        const CrossModuleLifetimeContract current = exampleContract();
        CrossModuleLifetimeContract forwardMinor = current;
        ++forwardMinor.abiMinor;
        context.expect(validateCrossModuleLifetimeContract(current, error) &&
                validateCrossModuleLifetimeContract(forwardMinor, error) &&
                crossModuleLifetimeFingerprint(current) != crossModuleLifetimeFingerprint(forwardMinor),
            "TL-LANG-080 minor ABI revisions must validate but remain fingerprint-visible");

        CrossModuleLifetimeContract incompatible = current;
        ++incompatible.abiMajor;
        incompatible.producerModule.clear();
        context.expect(!validateCrossModuleLifetimeContract(incompatible, error) &&
                error == "cross-module lifetime ABI major is incompatible",
            "TL-LANG-080 incompatible major must have priority over malformed fields");
    });
}

void registerFingerprintCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-081", [](TestContext& context) {
        const CrossModuleLifetimeContract base = exampleContract();
        const std::string fingerprint = crossModuleLifetimeFingerprint(base);
        context.expect(fingerprint == "dcb5c8234fafc747" &&
                crossModuleLifetimeFingerprint(base) == fingerprint,
            "TL-LANG-081 identical contracts must produce the frozen deterministic fingerprint");

        std::array<CrossModuleLifetimeContract, 9> variants{};
        variants.fill(base);
        ++variants[0].abiMajor;
        ++variants[1].abiMinor;
        variants[2].producerModule += ".v2";
        variants[3].typeIdentity += ".v2";
        variants[4].allocatorIdentity += ".v2";
        variants[5].createThunk += ".v2";
        variants[6].destroyThunk += ".v2";
        variants[7].deallocateThunk += ".v2";
        variants[8].unloadPolicy = ModuleUnloadPolicy::PinUntilObjectsDestroyed;
        for (const CrossModuleLifetimeContract& variant : variants)
        {
            context.expect(crossModuleLifetimeFingerprint(variant) != fingerprint,
                "TL-LANG-081 every contractual field must affect the fingerprint");
        }
    });
}

void registerObjectRegistrationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-082", [](TestContext& context) {
        ModuleLifetimeRegistry registry;
        const CrossModuleLifetimeContract contract = exampleContract();
        std::string error = "stale";
        for (std::size_t count = 1; count <= 3; ++count)
        {
            context.expect(registry.registerObject(contract, error) && error.empty() &&
                    registry.liveObjects("Producer") == count,
                "TL-LANG-082 repeated registrations must increment the live count exactly");
        }
        const auto found = registry.modules_.find("Producer");
        context.expect(found != registry.modules_.end() &&
                found->second.contractFingerprint == "dcb5c8234fafc747" && found->second.liveObjects == 3,
            "TL-LANG-082 the first registration must freeze the producer fingerprint");
    });
}

void registerObjectRejectionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-083", [](TestContext& context) {
        const CrossModuleLifetimeContract contract = exampleContract();
        CrossModuleLifetimeContract incompatible = contract;
        incompatible.allocatorIdentity += ".other";
        ModuleLifetimeRegistry active;
        std::string error;
        context.expect(active.registerObject(contract, error) &&
                !active.registerObject(incompatible, error) &&
                error == "producer module attempted to mix incompatible allocator/destruction contracts" &&
                active.liveObjects("Producer") == 1,
            "TL-LANG-083 an incompatible producer contract must be rejected without changing count");

        ModuleLifetimeRegistry unloading;
        context.expect(unloading.requestUnload("Producer", error) &&
                !unloading.registerObject(contract, error) &&
                error == "producer module is unloading and cannot create another cross-module object" &&
                unloading.liveObjects("Producer") == 0,
            "TL-LANG-083 an unloading producer must reject registration without changing count");
    });
}

void registerCounterOverflowCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-084", [](TestContext& context) {
        const CrossModuleLifetimeContract contract = exampleContract();
        ModuleLifetimeRegistry registry;
        registry.modules_.emplace("Producer", ModuleLifetimeRegistry::ModuleState{
            crossModuleLifetimeFingerprint(contract), std::numeric_limits<std::size_t>::max(), false});
        std::string error;
        context.expect(!registry.registerObject(contract, error) &&
                error == "producer module live-object counter overflow" &&
                registry.liveObjects("Producer") == std::numeric_limits<std::size_t>::max(),
            "TL-LANG-084 overflow must fail without wrapping or mutating the live count");
    });
}

void registerObjectReleaseCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-085", [](TestContext& context) {
        const CrossModuleLifetimeContract contract = exampleContract();
        CrossModuleLifetimeContract incompatible = contract;
        incompatible.destroyThunk += ".other";
        ModuleLifetimeRegistry registry;
        std::string error;

        context.expect(!registry.unregisterObject(exampleContract("Unknown"), error) &&
                error == "cross-module object release has no matching live registration" &&
                registry.liveObjects("Unknown") == 0,
            "TL-LANG-085 an unknown producer release must fail without creating live state");
        context.expect(registry.registerObject(contract, error) &&
                !registry.unregisterObject(incompatible, error) &&
                error == "cross-module object must be destroyed and deallocated by its producer contract" &&
                registry.liveObjects("Producer") == 1,
            "TL-LANG-085 a mismatched release contract must not decrement the live count");
        context.expect(registry.unregisterObject(contract, error) && error.empty() &&
                registry.liveObjects("Producer") == 0,
            "TL-LANG-085 a matching release must decrement the live count exactly");
        context.expect(!registry.unregisterObject(contract, error) &&
                error == "cross-module object release has no matching live registration" &&
                registry.liveObjects("Producer") == 0,
            "TL-LANG-085 a zero-count release must fail without underflow");
    });
}

void registerUnloadCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-086", [](TestContext& context) {
        const CrossModuleLifetimeContract contract = exampleContract();
        std::string error = "stale";
        ModuleLifetimeRegistry empty;
        context.expect(empty.requestUnload("Producer", error) && error.empty() &&
                !empty.registerObject(contract, error) &&
                error == "producer module is unloading and cannot create another cross-module object",
            "TL-LANG-086 zero live objects must mark the producer as unloading");

        ModuleLifetimeRegistry active;
        context.expect(active.registerObject(contract, error) && active.registerObject(contract, error) &&
                !active.requestUnload("Producer", error) &&
                error == "module unload rejected: 2 cross-module object(s) still require producer "
                    "destroy/deallocate thunks" && active.liveObjects("Producer") == 2,
            "TL-LANG-086 live objects must reject unload with exact count and no mutation");
        context.expect(active.unregisterObject(contract, error) && active.unregisterObject(contract, error) &&
                active.requestUnload("Producer", error) && error.empty(),
            "TL-LANG-086 unloading must succeed after the last matching release");
    });
}

void registerConcurrentRegistryCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-087", [](TestContext& context) {
        constexpr std::size_t workerCount = 8;
        constexpr std::size_t operationsPerWorker = 64;
        const CrossModuleLifetimeContract contract = exampleContract();
        ModuleLifetimeRegistry registry;
        std::atomic<std::size_t> failures = 0;
        std::vector<std::thread> workers;
        workers.reserve(workerCount);
        for (std::size_t worker = 0; worker < workerCount; ++worker)
        {
            workers.emplace_back([&registry, &contract, &failures] {
                for (std::size_t operation = 0; operation < operationsPerWorker; ++operation)
                {
                    std::string error;
                    if (!registry.registerObject(contract, error) || !error.empty()) ++failures;
                }
            });
        }
        for (std::thread& worker : workers) worker.join();
        context.expect(failures == 0 &&
                registry.liveObjects("Producer") == workerCount * operationsPerWorker,
            "TL-LANG-087 concurrent registration must serialize the exact live count");

        workers.clear();
        for (std::size_t worker = 0; worker < workerCount; ++worker)
        {
            workers.emplace_back([&registry, &contract, &failures] {
                for (std::size_t operation = 0; operation < operationsPerWorker; ++operation)
                {
                    std::string error;
                    if (!registry.unregisterObject(contract, error) || !error.empty()) ++failures;
                }
            });
        }
        for (std::thread& worker : workers) worker.join();
        context.expect(failures == 0 && registry.liveObjects("Producer") == 0,
            "TL-LANG-087 concurrent release must serialize without underflow or lost decrements");

        ModuleLifetimeRegistry mixed;
        std::string error;
        context.expect(mixed.registerObject(contract, error),
            "TL-LANG-087 setup must establish one canonical producer contract");
        CrossModuleLifetimeContract incompatible = contract;
        incompatible.allocatorIdentity += ".other";
        workers.clear();
        for (std::size_t worker = 0; worker < workerCount; ++worker)
        {
            workers.emplace_back([&mixed, &incompatible, &failures] {
                std::string localError;
                if (mixed.registerObject(incompatible, localError) ||
                    localError != "producer module attempted to mix incompatible allocator/destruction contracts")
                    ++failures;
            });
        }
        for (std::thread& worker : workers) worker.join();
        context.expect(failures == 0 && mixed.liveObjects("Producer") == 1,
            "TL-LANG-087 concurrency must not mix contracts or mutate the canonical count");

        workers.clear();
        for (std::size_t worker = 0; worker < workerCount; ++worker)
        {
            workers.emplace_back([&mixed, &failures] {
                std::string localError;
                if (mixed.requestUnload("Producer", localError) ||
                    localError != "module unload rejected: 1 cross-module object(s) still require producer "
                        "destroy/deallocate thunks")
                    ++failures;
            });
        }
        for (std::thread& worker : workers) worker.join();
        context.expect(failures == 0 && mixed.liveObjects("Producer") == 1,
            "TL-LANG-087 concurrent unload requests must observe live objects without mutation");
        context.expect(mixed.unregisterObject(contract, error) && mixed.requestUnload("Producer", error),
            "TL-LANG-087 the serialized terminal release must permit unload");
    }, "NIGHTLY");
}
}

void registerLanguageLifetimeModule(TestRegistry& registry)
{
    registerContractConstructionCase(registry);
    registerContractValidationCase(registry);
    registerAbiCompatibilityCase(registry);
    registerFingerprintCase(registry);
    registerObjectRegistrationCase(registry);
    registerObjectRejectionCase(registry);
    registerCounterOverflowCase(registry);
    registerObjectReleaseCase(registry);
    registerUnloadCase(registry);
    registerConcurrentRegistryCase(registry);
}
}
