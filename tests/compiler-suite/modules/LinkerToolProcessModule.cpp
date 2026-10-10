#include "LinkerTestSupport.h"

#include <array>

namespace nsc::suite
{
using namespace linker_support;

/**
 * Materializes tool-discovery, response-file, process and import-library cases.
 * They remain pending until controlled NSLink/llvm-lib child fixtures exist.
 */
void registerLinkerToolProcessModule(TestRegistry& registry)
{
    (void)registry;
    constexpr std::array<std::size_t, 12> contracts{
        4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
        35, 36,
    };
    for (const std::size_t contract : contracts)
    {
        addInfrastructureContract(
            contract,
            "controlled-nslink-llvm-lib-response-file-child-process-and-import-library-fixture");
    }
}
}
