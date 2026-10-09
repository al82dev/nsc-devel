#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/TraitSemantics.h"

#include <array>
#include <limits>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-021", "Host.Language", "unit", "PR",
        {"language", "trait", "dispatch"}, std::move(function)});
}

void registerTraitObjectLayoutCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-279", [](TestContext& context) {
        const TraitObjectLayout layout = windowsX64TraitObjectLayout();
        context.expect(layout.size == 8 && layout.alignment == 8 && layout.descriptorInterfaceMap &&
                layout.nullIsZero && !layout.conversionAllocates,
            "TL-LANG-279 the Windows x64 trait object layout must match all five frozen fields");
    });
}

void registerDispatchMetadataCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-280", [](TestContext& context) {
        context.expect(dispatchMetadata(DispatchKind::Static, "Demo.Trait") == "static:Demo.Trait" &&
                dispatchMetadata(DispatchKind::Dynamic, "Demo.Trait") == "dyn:Demo.Trait" &&
                dispatchMetadata(DispatchKind::Static, "") == "static:" &&
                dispatchMetadata(DispatchKind::Dynamic, "") == "dyn:",
            "TL-LANG-280 static/dynamic and empty/non-empty identities must have exact spelling");
    });
}

void registerExtensionResolutionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-281", [](TestContext& context) {
        context.expect(resolveExtension(true, 0) == ExtensionResolution::InstanceMember &&
                resolveExtension(true, 1) == ExtensionResolution::InstanceMember &&
                resolveExtension(true, std::numeric_limits<std::size_t>::max()) ==
                    ExtensionResolution::InstanceMember,
            "TL-LANG-281 an instance member must win independently of visible extension count");
        context.expect(resolveExtension(false, 0) == ExtensionResolution::Missing &&
                resolveExtension(false, 1) == ExtensionResolution::UniqueExtension &&
                resolveExtension(false, 2) == ExtensionResolution::Ambiguous &&
                resolveExtension(false, std::numeric_limits<std::size_t>::max()) ==
                    ExtensionResolution::Ambiguous,
            "TL-LANG-281 zero, one and many extensions must resolve to missing, unique and ambiguous");
    });
}
}

void registerLanguageTraitDispatchModule(TestRegistry& registry)
{
    registerTraitObjectLayoutCase(registry);
    registerDispatchMetadataCase(registry);
    registerExtensionResolutionCase(registry);
}
}
