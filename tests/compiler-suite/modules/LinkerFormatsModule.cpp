#include "LinkerTestSupport.h"

#include <array>

namespace nsc::suite
{
using namespace linker_support;

/**
 * Materializes binary-format cases whose independent COFF, PE32+ or PDB fixture
 * builders are not present yet. None enters the ordinary executable registry.
 */
void registerLinkerFormatsModule(TestRegistry& registry)
{
    (void)registry;
    constexpr std::array<std::size_t, 14> contracts{
        1, 2, 3,
        14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
        38,
    };
    for (const std::size_t contract : contracts)
    {
        addInfrastructureContract(
            contract,
            "independent-bounded-coff-pe32-pdb-fixture-builder-and-mutation-oracle");
    }
}
}
