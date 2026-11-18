#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"

#include <string_view>

int nscLlvmOptimizationLegacyEntry();
int nscLlvmOptimizationLegacyChildEntry(int argumentCount, char** arguments);
int nscNativeSharpE046SmartPointerLegacyEntry();
int nscNativeSharpT10CoroutineLegacyEntry();
int nscStage46NativeLibrariesLegacyEntry();

namespace nsc::test
{
namespace
{
using LegacyEntry = int (*)();

void addArgumentLegacyEntry(
    TestRegistry& registry, const char* id, const char* component, LegacyEntry entry)
{
    registry.add(TestCase{
        id,
        component,
        "historical argument-aware owner preserved until fixture parity",
        {"legacy-parity", "transitional", "exclusive", "argument-aware"},
        [entry](TestContext& context) {
            if (entry() != 0)
            {
                context.fail("historical argument-aware owner reported a failure");
            }
        },
    });
}
}

void registerArgumentLegacyUnitTests(TestRegistry& registry)
{
    addArgumentLegacyEntry(
        registry, "nsc.llvm-optimization", "llvm", nscLlvmOptimizationLegacyEntry);
    addArgumentLegacyEntry(registry,
        "nsc.language.e046.smart-pointer-contract",
        "frontend",
        nscNativeSharpE046SmartPointerLegacyEntry);
    addArgumentLegacyEntry(registry,
        "nsc.language.t10.coroutines",
        "frontend",
        nscNativeSharpT10CoroutineLegacyEntry);
    addArgumentLegacyEntry(registry,
        "nsc.stage46.native-libraries",
        "interop",
        nscStage46NativeLibrariesLegacyEntry);
}

bool handleArgumentLegacyChild(int argumentCount, char** arguments, int& result)
{
    if (argumentCount != 2 || arguments[1] == nullptr ||
        std::string_view(arguments[1]) != "--nsc-test-llvm-trace-child")
    {
        return false;
    }
    result = nscLlvmOptimizationLegacyChildEntry(argumentCount, arguments);
    return true;
}
}
