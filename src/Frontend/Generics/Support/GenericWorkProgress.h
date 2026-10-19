#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"

namespace nativesharp::frontend::internal
{
#if 0 // INSTRUMENTATION CODE: preserved diagnostic breadcrumbs; disabled in normal builds.
// Live progress has a different lifetime from timing telemetry: its opening event must be flushed
// before the risky operation starts, while its closing event is deliberately absent if the process
// dies inside that operation. This makes the last unmatched line a causal breadcrumb instead of a
// summary that is emitted only after a successful build.
class GenericWorkProgress final
{
public:
    GenericWorkProgress(
        GenericTelemetry& telemetry,
        std::string_view phase,
        std::string detail,
        std::size_t current = 1,
        std::size_t total = 1);

    ~GenericWorkProgress();

    GenericWorkProgress(const GenericWorkProgress&) = delete;
    GenericWorkProgress& operator=(const GenericWorkProgress&) = delete;

private:
    GenericTelemetry* telemetry_;
    std::string_view phase_;
    std::string detail_;
    std::size_t current_;
    std::size_t total_;
};

std::string genericUnitDetail(const ProjectSyntaxUnit& unit);
std::string compactTraceText(std::string_view value);
std::string genericNodeDetail(const ProjectSyntaxUnit& unit, const ast::AstNode& node);
std::string genericTypeDetail(const ast::TypeSyntax& syntax);
#endif // INSTRUMENTATION CODE
}
