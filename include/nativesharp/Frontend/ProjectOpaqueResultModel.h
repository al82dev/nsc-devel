#pragma once

#include <memory>
#include <span>

namespace nativesharp::diagnostics { class DiagnosticEngine; }

namespace nativesharp::frontend
{
class ProjectSyntaxUnit;

// T05-F04 runs before generic-template collection. It replaces each source-level `some Trait`
// return with a declaration-scoped nominal value wrapper whose single representation field is the
// unique concrete return type. The wrapper is ordinary AST afterwards: name lookup, layout,
// NSMETA, HIR, MIR, ABI and debug information all consume one existing representation.
class ProjectOpaqueResultModel final
{
public:
    bool build(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
};
}
