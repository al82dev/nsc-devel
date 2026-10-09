#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/SmartPointerSemantics.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

constexpr std::array kFamilies = {
    SmartPointerFamily::Unique,
    SmartPointerFamily::Rc,
    SmartPointerFamily::WeakRc,
    SmartPointerFamily::Arc,
    SmartPointerFamily::WeakArc,
};

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-019", "Host.Language", "component", "PR",
        {"language", "smart-pointer", "operation"}, std::move(function)});
}

SmartPointerOperationContext operation(SmartPointerFamily family, SmartPointerOperation kind)
{
    SmartPointerOperationContext context;
    context.family = family;
    context.operation = kind;
    return context;
}

void registerFamilyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-249", [](TestContext& context) {
        struct ExpectedFamily final
        {
            SmartPointerFamily family;
            std::string_view name;
            SmartPointerThreadModel threadModel;
            bool explicitClone;
            bool weak;
        };
        constexpr std::array expected = {
            ExpectedFamily{SmartPointerFamily::Unique, "Unique", SmartPointerThreadModel::UniqueTransfer,
                false, false},
            ExpectedFamily{SmartPointerFamily::Rc, "Rc", SmartPointerThreadModel::LocalOnly, true, false},
            ExpectedFamily{SmartPointerFamily::WeakRc, "WeakRc", SmartPointerThreadModel::LocalOnly, true, true},
            ExpectedFamily{SmartPointerFamily::Arc, "Arc", SmartPointerThreadModel::AtomicShared, true, false},
            ExpectedFamily{SmartPointerFamily::WeakArc, "WeakArc", SmartPointerThreadModel::AtomicShared,
                true, true},
        };
        context.expect(kSmartPointerFamilies.size() == expected.size(),
            "TL-LANG-249 the family table must contain exactly five entries");
        for (const ExpectedFamily& item : expected)
        {
            const SmartPointerFamilyContract& actual = smartPointerFamilyContract(item.family);
            context.expect(actual.family == item.family && actual.name == item.name &&
                    actual.threadModel == item.threadModel && actual.nullable && !actual.implicitCopy &&
                    actual.explicitClone == item.explicitClone && actual.weak == item.weak,
                "TL-LANG-249 every family field must equal the frozen family matrix");
        }
    });
}

void registerInvalidFamilyCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LANG-250",
        "CL-LANG-019",
        "Host.Language",
        "component",
        "NIGHTLY",
        {"language", "smart-pointer", "operation", "known-product-defect"},
        [](TestContext& context) {
            constexpr SmartPointerFamily invalid = static_cast<SmartPointerFamily>(5);
            const SmartPointerFamilyContract& observed = smartPointerFamilyContract(invalid);
            context.expect(false,
                "TL-LANG-250 an external SmartPointerFamily must be rejected before table indexing; "
                "the current product returned family " +
                    std::to_string(static_cast<unsigned int>(observed.family)));
        },
        TestDisposition::KnownProductDefect,
    });
}

void registerMoveCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-251", [](TestContext& context) {
        for (const SmartPointerFamily family : kFamilies)
        {
            std::string error = "stale";
            context.expect(validateSmartPointerOperation(operation(family, SmartPointerOperation::Move), error) &&
                    error.empty(),
                "TL-LANG-251 Move must accept every family and clear a stale error");
        }
    });
}

void registerImplicitCopyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-252", [](TestContext& context) {
        for (const SmartPointerFamily family : kFamilies)
        {
            std::string error;
            const std::string name(smartPointerFamilyContract(family).name);
            context.expect(!validateSmartPointerOperation(
                    operation(family, SmartPointerOperation::ImplicitCopy), error) &&
                    error == name + " is never implicitly copyable; use Move or explicit Clone",
                "TL-LANG-252 implicit Copy must reject every family with its exact diagnostic");
        }
    });
}

void registerCloneDowngradeUpgradeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-253", [](TestContext& context) {
        std::string error;
        context.expect(!validateSmartPointerOperation(
                operation(SmartPointerFamily::Unique, SmartPointerOperation::Clone), error) &&
                error == "Unique does not support Clone",
            "TL-LANG-253 Unique Clone must be rejected exactly");
        for (const SmartPointerFamily family : {SmartPointerFamily::Rc, SmartPointerFamily::WeakRc,
                 SmartPointerFamily::Arc, SmartPointerFamily::WeakArc})
        {
            context.expect(validateSmartPointerOperation(operation(family, SmartPointerOperation::Clone), error) &&
                    error.empty(),
                "TL-LANG-253 shared and weak families must support Clone");
            auto overflow = operation(family, SmartPointerOperation::Clone);
            if (smartPointerFamilyContract(family).weak)
                overflow.weakCount = std::numeric_limits<std::uint64_t>::max();
            else
                overflow.strongCount = std::numeric_limits<std::uint64_t>::max();
            const std::string name(smartPointerFamilyContract(family).name);
            context.expect(!validateSmartPointerOperation(overflow, error) &&
                    error == name + " reference count overflow",
                "TL-LANG-253 Clone must reject the relevant maximum count before wraparound");
        }
        for (const SmartPointerFamily family : kFamilies)
        {
            const bool supported = family == SmartPointerFamily::Rc || family == SmartPointerFamily::Arc;
            const bool accepted = validateSmartPointerOperation(
                operation(family, SmartPointerOperation::Downgrade), error);
            context.expect(accepted == supported &&
                    (supported ? error.empty() : error == "Downgrade requires Rc or Arc"),
                "TL-LANG-253 Downgrade must follow the exact strong-family matrix");
            if (supported)
            {
                auto overflow = operation(family, SmartPointerOperation::Downgrade);
                overflow.weakCount = std::numeric_limits<std::uint64_t>::max();
                const std::string name(smartPointerFamilyContract(family).name);
                context.expect(!validateSmartPointerOperation(overflow, error) &&
                        error == name + " weak reference count overflow",
                    "TL-LANG-253 Downgrade must reject maximum weak count before wraparound");
            }
        }
        for (const SmartPointerFamily family : kFamilies)
        {
            const bool supported = family == SmartPointerFamily::WeakRc || family == SmartPointerFamily::WeakArc;
            const bool accepted = validateSmartPointerOperation(
                operation(family, SmartPointerOperation::Upgrade), error);
            context.expect(accepted == supported &&
                    (supported ? error.empty() : error == "Upgrade requires WeakRc or WeakArc"),
                "TL-LANG-253 Upgrade must follow the exact weak-family matrix");
        }
    });
}

void registerUpgradeBoundaryCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-254", [](TestContext& context) {
        for (const SmartPointerFamily family : {SmartPointerFamily::WeakRc, SmartPointerFamily::WeakArc})
        {
            const std::string name(smartPointerFamilyContract(family).name);
            auto expired = operation(family, SmartPointerOperation::Upgrade);
            expired.strongCount = 0;
            std::string error;
            context.expect(!validateSmartPointerOperation(expired, error) &&
                    error == name + " is expired; Upgrade cannot resurrect the payload",
                "TL-LANG-254 zero strong owners must prevent resurrection exactly");
            auto overflow = operation(family, SmartPointerOperation::Upgrade);
            overflow.strongCount = std::numeric_limits<std::uint64_t>::max();
            context.expect(!validateSmartPointerOperation(overflow, error) &&
                    error == name + " strong reference count overflow",
                "TL-LANG-254 maximum strong count must reject Upgrade before wraparound");
        }
    });
}

void registerUniqueTransferCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-255", [](TestContext& context) {
        for (const SmartPointerOperation kind : {SmartPointerOperation::Release, SmartPointerOperation::Reset})
        {
            for (const SmartPointerFamily family : kFamilies)
            {
                std::string error;
                const bool accepted = validateSmartPointerOperation(operation(family, kind), error);
                const std::string expected = std::string(smartPointerFamilyContract(family).name) +
                    " does not expose Unique ownership transfer operations";
                context.expect(accepted == (family == SmartPointerFamily::Unique) &&
                        (accepted ? error.empty() : error == expected),
                    "TL-LANG-255 Release and Reset must be exclusive to Unique");
            }
        }
    });
}

void registerCrossThreadCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-256", [](TestContext& context) {
        std::string error;
        for (const SmartPointerFamily family : {SmartPointerFamily::Rc, SmartPointerFamily::WeakRc})
        {
            auto local = operation(family, SmartPointerOperation::CrossThread);
            local.payloadSendSync = true;
            local.producerModulePinned = true;
            const std::string name(smartPointerFamilyContract(family).name);
            context.expect(!validateSmartPointerOperation(local, error) &&
                    error == name + " is local-only; use Arc/WeakArc for shared cross-thread ownership",
                "TL-LANG-256 Rc and WeakRc must remain local-only before other cross-thread gates");
        }
        for (const SmartPointerFamily family : {SmartPointerFamily::Unique, SmartPointerFamily::Arc,
                 SmartPointerFamily::WeakArc})
        {
            const std::string name(smartPointerFamilyContract(family).name);
            auto crossThread = operation(family, SmartPointerOperation::CrossThread);
            context.expect(!validateSmartPointerOperation(crossThread, error) &&
                    error == name + " cannot cross threads because the payload is not Send/Sync",
                "TL-LANG-256 transferable families must require a Send/Sync payload first");
            crossThread.payloadSendSync = true;
            crossThread.producerModulePinned = false;
            context.expect(!validateSmartPointerOperation(crossThread, error) &&
                    error == name + " cannot outlive its producer module destroy/deallocate thunks",
                "TL-LANG-256 transferable families must require a pinned producer module");
            crossThread.producerModulePinned = true;
            context.expect(validateSmartPointerOperation(crossThread, error) && error.empty(),
                "TL-LANG-256 transferable families must pass once both cross-thread gates hold");
        }
    });
}
}

void registerLanguageSmartPointerOperationsModule(TestRegistry& registry)
{
    registerFamilyCase(registry);
    registerInvalidFamilyCase(registry);
    registerMoveCase(registry);
    registerImplicitCopyCase(registry);
    registerCloneDowngradeUpgradeCase(registry);
    registerUpgradeBoundaryCase(registry);
    registerUniqueTransferCase(registry);
    registerCrossThreadCase(registry);
}
}
