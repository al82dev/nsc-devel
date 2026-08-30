#pragma once

#include <iosfwd>

namespace nativesharp::passes
{
class OptimizationModule;

class PassPipelineDumper final
{
public:
    void dump(
        const OptimizationModule* module,
        std::ostream& output) const;
};
}
