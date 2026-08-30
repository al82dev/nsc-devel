#include "nativesharp/Passes/PassPipelineDumper.h"

#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Passes/PassRecord.h"

#include <ostream>

namespace nativesharp::passes
{
void PassPipelineDumper::dump(
    const OptimizationModule* module,
    std::ostream& output) const
{
    if (module == nullptr)
    {
        return;
    }

    output << "pass.pipeline\n";
    output << "iterations: " << module->iterations() << '\n';
    output << "cfg-builds: " << module->cfgBuildCount() << '\n';
    output << "dominance-builds: "
           << module->dominanceBuildCount() << '\n';

    for (const PassRecord& record : module->records())
    {
        output << "iteration " << record.iteration
               << " pass " << record.name
               << " changed=" << (record.changed ? "yes" : "no")
               << " transforms=" << record.transformations
               << " instructions=" << record.instructionsBefore
               << "->" << record.instructionsAfter
               << " blocks=" << record.blocksBefore
               << "->" << record.blocksAfter
               << " preserves=";

        if (record.preservedCfg && record.preservedDominance)
        {
            output << "cfg,dominance";
        }
        else if (record.preservedCfg)
        {
            output << "cfg";
        }
        else
        {
            output << "none";
        }

        output << " cfg-builds=" << record.cfgBuildsAfter
               << " dominance-builds="
               << record.dominanceBuildsAfter
               << '\n';
    }
}
}
